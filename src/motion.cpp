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
}

void MotionDetector::update(CSIData &frame) {
    std::vector<float> current_amplitudes;
    for (const auto &chain : frame.chains) {
        current_amplitudes.insert(current_amplitudes.end(),
                                  chain.amplitudes.begin(),
                                  chain.amplitudes.end());
    }

    if (current_amplitudes.size() != previous_amplitudes_.size()) {
        previous_amplitudes_ = std::move(current_amplitudes);
        motion_window_.clear();
        resetCalibration();
        frame.motion_ready = false;
        frame.motion_score = 0.0f;
        return;
    }

    if (!previous_amplitudes_.empty()) {
        const size_t n = current_amplitudes.size();
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
        const float pearson_score = 1.0f - correlation;

        // Tính biến động vi mô trên từng subcarrier (bắt rung chân, cử động nhỏ)
        std::vector<float> diffs(n);
        for (size_t i = 0; i < n; ++i) {
            const float base = std::max(current_amplitudes[i], 1.0f);
            diffs[i] = std::abs(current_amplitudes[i] - previous_amplitudes_[i]) / base;
        }
        // Lấy top 10% subcarrier biến động mạnh nhất
        const size_t top_k = std::max<size_t>(3, n / 10);
        std::nth_element(diffs.begin(), diffs.begin() + top_k, diffs.end(), std::greater<float>());
        const float top_k_diff = std::accumulate(diffs.begin(), diffs.begin() + top_k, 0.0f) / static_cast<float>(top_k);

        // Kết hợp cả biến đổi tương quan toàn dải và biến động vi mô top-k (hệ số 0.08 vừa bắt rung chân vừa không thổi phồng nhiễu RF)
        const float combined_score = pearson_score + top_k_diff * 0.08f;

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
                        float baseline_deviation = std::max(
                            standard_deviation, kMinimumBaselineDeviation);
                        // Ngưỡng thích ứng: tối thiểu candidate_mean + 0.04f để chống báo động giả do trôi RF
                        motion_threshold_high_ = std::max(
                            candidate_mean + 4.0f * baseline_deviation,
                            candidate_mean + 0.04f);
                        motion_threshold_low_ = std::max(
                            candidate_mean + 2.0f * baseline_deviation,
                            candidate_mean + 0.02f);
                        motion_calibrated_ = true;
                    }
                }
            }
        } else if (frame.motion_score > motion_threshold_high_) {
            presence_detected_ = true;
            low_motion_frames_ = 0;
        } else if (frame.motion_score < motion_threshold_low_) {
            if (++low_motion_frames_ >= kLowMotionFramesToClear) {
                presence_detected_ = false;
                low_motion_frames_ = kLowMotionFramesToClear;
            }
        } else {
            low_motion_frames_ = 0;
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