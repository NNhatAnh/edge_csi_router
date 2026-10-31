#include "motion.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
constexpr size_t kMotionWindowSize = 30;
constexpr size_t kStableCandidateSize = 40;
constexpr size_t kLowMotionFramesToClear = 15;
constexpr float kMinimumBaselineDeviation = 2.0e-3f;
constexpr float kMaximumStableMotionScore = 0.40f;
constexpr float kMaximumStableScoreDeviation = 0.035f;
constexpr float kMaximumStableScoreStep = 0.05f;
constexpr size_t kMaxDisruptionStreakToClear = 6;
} // namespace

void MotionDetector::reset() {
    previous_amplitudes_.clear();
    motion_window_.clear();
    resetCalibration();
}

void MotionDetector::resetCalibration() {
    calibration_candidate_.clear();
    calibration_disruption_streak_ = 0;
    low_motion_frames_ = 0;
    motion_threshold_high_ = 0.0f;
    motion_threshold_low_ = 0.0f;
    motion_calibrated_ = false;
    calibration_sample_stable_ = false;
    presence_detected_ = false;
    baseline_mean_ = 0.0f;
    baseline_deviation_ = 0.0f;
    quiet_frames_ = 0;
    static_amplitudes_.clear();
    has_static_amplitudes_ = false;
}

void MotionDetector::update(CSIData &frame) {
    std::vector<float> current_amplitudes;
    for (const auto &chain : frame.chains) {
        current_amplitudes.insert(current_amplitudes.end(),
                                  chain.amplitudes.begin(),
                                  chain.amplitudes.end());
    }

    if (current_amplitudes.empty()) {
        frame.motion_ready = false;
        frame.motion_score = 0.0f;
        return;
    }

    // 1. Chuẩn hóa Frobenius Norm trên toàn bộ vector biên độ
    // Triệt tiêu 100% bước nhảy độ lợi phần cứng do ath12k AGC hunting
    const size_t n = current_amplitudes.size();
    float sum_sq = 0.0f;
    for (float a : current_amplitudes) {
        sum_sq += a * a;
    }
    const float rms = std::sqrt(sum_sq / static_cast<float>(n));
    if (rms > 1.0e-5f) {
        const float inv_rms = 1.0f / rms;
        for (float &a : current_amplitudes) {
            a *= inv_rms;
        }
    }

    if (current_amplitudes.size() != previous_amplitudes_.size()) {
        previous_amplitudes_ = current_amplitudes;
        static_amplitudes_ = current_amplitudes;
        has_static_amplitudes_ = true;
        motion_window_.clear();
        resetCalibration();
        frame.motion_ready = false;
        frame.motion_score = 0.0f;
        return;
    }

    if (!previous_amplitudes_.empty()) {
        // 2. WiDraw Temporal Correlation Gating (Section 4.2)
        // Kiểm tra tính tương đồng thời gian để loại bỏ frame lỗi/rớt gói đột ngột
        float dot_prod = 0.0f;
        float pwr_curr = 0.0f;
        float pwr_prev = 0.0f;
        for (size_t i = 0; i < n; ++i) {
            dot_prod += current_amplitudes[i] * previous_amplitudes_[i];
            pwr_curr += current_amplitudes[i] * current_amplitudes[i];
            pwr_prev += previous_amplitudes_[i] * previous_amplitudes_[i];
        }
        float temporal_corr = 1.0f;
        if (pwr_curr > 1.0e-7f && pwr_prev > 1.0e-7f) {
            temporal_corr = dot_prod / std::sqrt(pwr_curr * pwr_prev);
            temporal_corr = std::clamp(temporal_corr, -1.0f, 1.0f);
        }

        // 3. WiDraw Static Path Removal (Tách đường truyền tĩnh H_static)
        if (!has_static_amplitudes_ || static_amplitudes_.size() != n) {
            static_amplitudes_ = current_amplitudes;
            has_static_amplitudes_ = true;
        } else {
            const float alpha_static = 0.005f;
            for (size_t i = 0; i < n; ++i) {
                static_amplitudes_[i] =
                    (1.0f - alpha_static) * static_amplitudes_[i] + alpha_static * current_amplitudes[i];
            }
        }

        float dynamic_energy = 0.0f;
        for (size_t i = 0; i < n; ++i) {
            float d = current_amplitudes[i] - static_amplitudes_[i];
            dynamic_energy += d * d;
        }
        dynamic_energy /= static_cast<float>(n);

        float mean_x = std::accumulate(current_amplitudes.begin(),
                                       current_amplitudes.end(), 0.0f) /
                       n;
        float mean_y = std::accumulate(previous_amplitudes_.begin(),
                                       previous_amplitudes_.end(), 0.0f) /
                       n;

        float num = 0.0f;
        float den_x = 0.0f;
        float den_y = 0.0f;

        for (size_t i = 0; i < n; ++i) {
            float dx = current_amplitudes[i] - mean_x;
            float dy = previous_amplitudes_[i] - mean_y;
            num += dx * dy;
            den_x += dx * dx;
            den_y += dy * dy;
        }

        float correlation = 1.0f;
        if (den_x > 1.0e-9f && den_y > 1.0e-9f) {
            correlation = num / std::sqrt(den_x * den_y);
            correlation = std::clamp(correlation, -1.0f, 1.0f);
        }
        float pearson_score = 1.0f - correlation;

        // Tính biến động vi mô trên từng subcarrier đã chuẩn hóa
        std::vector<float> diffs(n);
        for (size_t i = 0; i < n; ++i) {
            const float base = std::max(current_amplitudes[i], 0.1f);
            diffs[i] = std::abs(current_amplitudes[i] - previous_amplitudes_[i]) / base;
        }
        const size_t top_k = std::max<size_t>(3, n / 10);
        std::nth_element(diffs.begin(), diffs.begin() + top_k, diffs.end(), std::greater<float>());
        const float top_k_diff =
            std::accumulate(diffs.begin(), diffs.begin() + top_k, 0.0f) / static_cast<float>(top_k);

        // Nếu temporal_corr < 0.82 (frame bị méo đột ngột do rớt gói/chuyển MCS), kẹp chặn điểm số giả
        if (temporal_corr < 0.82f) {
            pearson_score = std::min(pearson_score, 0.025f);
        }

        // Cổng năng lượng động (Dynamic Energy Gate theo chuẩn WiDraw):
        // Khi môi trường đứng yên, dynamic_energy rất nhỏ (< 0.001) -> dynamic_gate = 0.08
        // Khi có người cử động, dynamic_energy > 0.003 -> dynamic_gate = 1.0
        const float dynamic_gate = std::clamp((dynamic_energy - 0.0006f) / 0.0030f, 0.08f, 1.0f);
        const float combined_score = (pearson_score + top_k_diff * 0.08f) * dynamic_gate;

        motion_window_.push_back(combined_score);
        if (motion_window_.size() > kMotionWindowSize)
            motion_window_.pop_front();
    }

    previous_amplitudes_ = std::move(current_amplitudes);
    frame.motion_ready = (motion_window_.size() == kMotionWindowSize);
    frame.motion_window_samples = motion_window_.size();

    if (frame.motion_ready) {
        frame.motion_score = std::accumulate(motion_window_.begin(),
                                             motion_window_.end(), 0.0f) /
                             motion_window_.size();

        if (!motion_calibrated_) {
            calibration_sample_stable_ = false;

            const bool high_motion =
                frame.motion_score > kMaximumStableMotionScore;
            const bool step_too_big =
                !calibration_candidate_.empty() &&
                std::abs(frame.motion_score - calibration_candidate_.back()) >
                    kMaximumStableScoreStep;

            if (high_motion || step_too_big) {
                ++calibration_disruption_streak_;
                // Chỉ xóa sạch hàng đợi khi có gián đoạn liên tiếp kéo dài (streak >= 6 frame)
                if (calibration_disruption_streak_ >= kMaxDisruptionStreakToClear) {
                    calibration_candidate_.clear();
                    calibration_disruption_streak_ = 0;
                }
            } else {
                calibration_disruption_streak_ = 0;
                calibration_candidate_.push_back(frame.motion_score);
                if (calibration_candidate_.size() > kStableCandidateSize)
                    calibration_candidate_.pop_front();

                if (calibration_candidate_.size() == kStableCandidateSize) {
                    float candidate_mean =
                        std::accumulate(calibration_candidate_.begin(),
                                        calibration_candidate_.end(), 0.0f) /
                        kStableCandidateSize;
                    float variance = 0.0f;
                    for (float s : calibration_candidate_)
                        variance += (s - candidate_mean) * (s - candidate_mean);
                    float standard_deviation =
                        std::sqrt(variance / kStableCandidateSize);

                    if (standard_deviation <= kMaximumStableScoreDeviation) {
                        calibration_sample_stable_ = true;
                        baseline_deviation_ = std::max(
                            standard_deviation, kMinimumBaselineDeviation);
                        baseline_mean_ = candidate_mean;
                        // Ngưỡng thích ứng: tối thiểu candidate_mean + 0.04f để chống báo động giả do trôi RF
                        motion_threshold_high_ = std::max(
                            baseline_mean_ + 4.0f * baseline_deviation_,
                            baseline_mean_ + 0.04f);
                        motion_threshold_low_ = std::max(
                            baseline_mean_ + 2.0f * baseline_deviation_,
                            baseline_mean_ + 0.02f);
                        motion_calibrated_ = true;
                        quiet_frames_ = 0;
                    }
                }
            }
        } else {
            // Đánh giá phương sai trên cửa sổ chuyển động để đo độ tĩnh lặng của môi trường
            float window_mean = frame.motion_score;
            float window_var = 0.0f;
            for (float s : motion_window_) {
                window_var += (s - window_mean) * (s - window_mean);
            }
            const float window_std =
                std::sqrt(window_var / static_cast<float>(motion_window_.size()));

            if (frame.motion_score > motion_threshold_high_) {
                presence_detected_ = true;
                low_motion_frames_ = 0;
                quiet_frames_ = 0;
            } else if (frame.motion_score < motion_threshold_low_) {
                if (++low_motion_frames_ >= kLowMotionFramesToClear) {
                    presence_detected_ = false;
                    low_motion_frames_ = kLowMotionFramesToClear;
                }
                ++quiet_frames_;
            } else {
                if (window_std < 0.012f) {
                    if (++low_motion_frames_ >= (kLowMotionFramesToClear + 5)) {
                        presence_detected_ = false;
                        low_motion_frames_ = kLowMotionFramesToClear + 5;
                    }
                    ++quiet_frames_;
                } else {
                    low_motion_frames_ = 0;
                    quiet_frames_ = 0;
                }
            }
            
            if (!presence_detected_ && quiet_frames_ >= 45 && window_std < 0.015f) {
                const float alpha = 0.008f;
                baseline_mean_ = (1.0f - alpha) * baseline_mean_ + alpha * frame.motion_score;
                baseline_deviation_ = (1.0f - alpha) * baseline_deviation_ +
                                      alpha * std::max(window_std, kMinimumBaselineDeviation);

                motion_threshold_high_ = std::max(
                    baseline_mean_ + 4.0f * baseline_deviation_,
                    baseline_mean_ + 0.04f);
                motion_threshold_low_ = std::max(
                    baseline_mean_ + 2.0f * baseline_deviation_,
                    baseline_mean_ + 0.02f);
            }
        }
    }

    frame.motion_calibrated = motion_calibrated_;
    frame.motion_calibration_frames = motion_calibrated_
                                          ? kStableCandidateSize
                                          : calibration_candidate_.size();
    frame.motion_calibration_stable =
        motion_calibrated_ || calibration_sample_stable_;
    frame.motion_threshold_high = motion_threshold_high_;
    frame.motion_threshold_low = motion_threshold_low_;
    frame.presence_detected = presence_detected_;
}