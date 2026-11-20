#include "motion.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
constexpr size_t kMotionWindowSize = 10; // 10 frames (~100ms tại 100Hz) - phản hồi tức thì bám sát chuyển động tay
constexpr size_t kStableCandidateSize = 40;
constexpr size_t kLowMotionFramesToClear = 15;
constexpr float kMinimumBaselineDeviation = 2.0e-3f;
constexpr float kMaximumStableMotionScore = 0.40f;
constexpr float kMaximumStableScoreDeviation = 0.035f;
constexpr float kMaximumStableScoreStep = 0.05f;
constexpr size_t kMaxDisruptionStreakToClear = 6;
} // namespace

void MotionDetector::reset() {
    previous_chains_.clear();
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
}

void MotionDetector::update(CSIData &frame) {
    if (frame.chains.empty()) {
        frame.motion_ready = false;
        frame.motion_score = 0.0f;
        return;
    }

    // Kiểm tra cấu trúc số chuỗi anten
    if (previous_chains_.size() != frame.chains.size()) {
        previous_chains_.resize(frame.chains.size());
        for (size_t ch = 0; ch < frame.chains.size(); ++ch) {
            previous_chains_[ch] = frame.chains[ch].amplitudes;
        }
        motion_window_.clear();
        resetCalibration();
        frame.motion_ready = false;
        frame.motion_score = 0.0f;
        return;
    }

    // Kiểm tra kích thước tone trên từng chuỗi
    for (size_t ch = 0; ch < frame.chains.size(); ++ch) {
        if (frame.chains[ch].amplitudes.size() != previous_chains_[ch].size() ||
            frame.chains[ch].amplitudes.empty()) {
            for (size_t c = 0; c < frame.chains.size(); ++c) {
                previous_chains_[c] = frame.chains[c].amplitudes;
            }
            motion_window_.clear();
            resetCalibration();
            frame.motion_ready = false;
            frame.motion_score = 0.0f;
            return;
        }
    }

    // 1. Tính tương quan Pearson riêng biệt từng antenna chain rồi lấy trung bình
    float sum_decorr = 0.0f;
    size_t valid_chains = 0;
    std::vector<float> all_diffs;

    for (size_t ch = 0; ch < frame.chains.size(); ++ch) {
        const auto &curr = frame.chains[ch].amplitudes;
        const auto &prev = previous_chains_[ch];
        const size_t n = curr.size();
        if (n == 0) continue;

        float mean_x = std::accumulate(curr.begin(), curr.end(), 0.0f) / n;
        float mean_y = std::accumulate(prev.begin(), prev.end(), 0.0f) / n;

        float num = 0.0f;
        float den_x = 0.0f;
        float den_y = 0.0f;

        for (size_t i = 0; i < n; ++i) {
            float dx = curr[i] - mean_x;
            float dy = prev[i] - mean_y;
            num += dx * dy;
            den_x += dx * dx;
            den_y += dy * dy;

            // Thu thập vi sai tương đối trên các subcarrier có công suất đủ lớn (loại bỏ nhiễu biên và DC)
            float base = std::max({curr[i], prev[i], 50.0f});
            float d = std::abs(curr[i] - prev[i]) / base;
            all_diffs.push_back(std::clamp(d, 0.0f, 1.0f));
        }

        float correlation = 1.0f;
        if (den_x > 1.0e-9f && den_y > 1.0e-9f) {
            correlation = num / std::sqrt(den_x * den_y);
            correlation = std::clamp(correlation, -1.0f, 1.0f);
        }

        // Độ mất tương quan (decorrelation) của riêng chain ch trong dải [0.0, 1.0]
        float chain_decorr = std::clamp(1.0f - correlation, 0.0f, 1.0f);
        sum_decorr += chain_decorr;
        valid_chains++;
    }

    float avg_decorr = (valid_chains > 0) ? (sum_decorr / static_cast<float>(valid_chains)) : 0.0f;

    // 2. Tính vi sai vi mô tương đối (top 10% subcarrier biến động mạnh nhất trên các subcarrier hợp lệ)
    float top_k_diff = 0.0f;
    if (!all_diffs.empty()) {
        const size_t top_k = std::max<size_t>(3, all_diffs.size() / 10);
        std::nth_element(all_diffs.begin(), all_diffs.begin() + top_k, all_diffs.end(), std::greater<float>());
        top_k_diff = std::accumulate(all_diffs.begin(), all_diffs.begin() + top_k, 0.0f) / static_cast<float>(top_k);
        top_k_diff = std::clamp(top_k_diff, 0.0f, 1.0f);
    }

    // 3. Kết hợp có trọng số và kẹp trần nghiêm ngặt trong dải chuẩn [0.0, 1.0]
    // 75% tương quan hình thái không gian, 25% biến động vi sai biên độ
    const float combined_score = std::clamp(0.75f * avg_decorr + 0.25f * top_k_diff, 0.0f, 1.0f);

    motion_window_.push_back(combined_score);
    if (motion_window_.size() > kMotionWindowSize)
        motion_window_.pop_front();

    // Lưu lại frame hiện tại làm previous_chains_
    for (size_t ch = 0; ch < frame.chains.size(); ++ch) {
        previous_chains_[ch] = frame.chains[ch].amplitudes;
    }

    frame.motion_ready = (motion_window_.size() == kMotionWindowSize);
    frame.motion_window_samples = motion_window_.size();

    if (frame.motion_ready) {
        frame.motion_score = std::accumulate(motion_window_.begin(),
                                             motion_window_.end(), 0.0f) /
                             motion_window_.size();
        frame.motion_score = std::clamp(frame.motion_score, 0.0f, 1.0f);

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
                        // Ngưỡng thích ứng: tối thiểu candidate_mean + 0.10f để phản hồi nhạy bén và tự nhiên
                        motion_threshold_high_ = std::clamp(
                            std::max(baseline_mean_ + 3.0f * baseline_deviation_,
                                     baseline_mean_ + 0.10f),
                            0.20f, 0.60f);
                        motion_threshold_low_ = std::clamp(
                            std::max(baseline_mean_ + 1.5f * baseline_deviation_,
                                     baseline_mean_ + 0.05f),
                            0.10f, 0.40f);
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
                // ĐANG TRONG KHOẢNG CHẾT (DEADBAND): motion_threshold_low_ <= score <= motion_threshold_high_
                // Nếu cửa sổ rất phẳng (window_std < 0.012f), đây là nhiễu nền tĩnh (xuyên kính hoặc trôi nhiệt)
                // chứ không phải người đang bước đi. Cho phép xả trạng thái có người sau 20 frame.
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

            // Dynamic Leaky Baseline: Khi phòng tĩnh lặng liên tục (> 45 frame ~1.5s),
            // bộ bám EMA chậm (alpha = 0.008) tự thích ứng với sàn nhiễu và nhiệt độ chip của phòng hiện tại.
            if (!presence_detected_ && quiet_frames_ >= 45 && window_std < 0.015f) {
                const float alpha = 0.008f;
                baseline_mean_ = (1.0f - alpha) * baseline_mean_ + alpha * frame.motion_score;
                baseline_deviation_ = (1.0f - alpha) * baseline_deviation_ +
                                      alpha * std::max(window_std, kMinimumBaselineDeviation);

                motion_threshold_high_ = std::clamp(
                    std::max(baseline_mean_ + 3.0f * baseline_deviation_,
                             baseline_mean_ + 0.10f),
                    0.20f, 0.60f);
                motion_threshold_low_ = std::clamp(
                    std::max(baseline_mean_ + 1.5f * baseline_deviation_,
                             baseline_mean_ + 0.05f),
                    0.10f, 0.40f);
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