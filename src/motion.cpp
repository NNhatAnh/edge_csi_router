#include "motion.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
    constexpr size_t kMotionWindowSize = 30;
    constexpr size_t kStableCandidateSize = 50;
    constexpr size_t kLowMotionFramesToClear = 15;
    constexpr float kMinimumBaselineDeviation = 1.0e-3f;
    constexpr float kMaximumStableMotionScore = 1.0e-1f;
    constexpr float kMaximumStableScoreDeviation = 1.0e-2f;
    constexpr float kMaximumStableScoreStep = 2.0e-2f;
} // namespace

void MotionDetector::reset()
{
    previous_amplitudes_.clear();
    motion_window_.clear();
    resetCalibration();
}

void MotionDetector::resetCalibration()
{
    calibration_candidate_.clear();
    calibration_disruption_streak_ = 0;
    low_motion_frames_ = 0;
    motion_threshold_high_ = 0.0f;
    motion_threshold_low_ = 0.0f;
    motion_calibrated_ = false;
    calibration_sample_stable_ = false;
    presence_detected_ = false;
}

void MotionDetector::update(CSIData &frame)
{
    std::vector<float> current_amplitudes;
    for (const auto &chain : frame.chains)
    {
        current_amplitudes.insert(current_amplitudes.end(),
                                  chain.amplitudes.begin(),
                                  chain.amplitudes.end());
    }

    if (current_amplitudes.size() != previous_amplitudes_.size())
    {
        previous_amplitudes_ = std::move(current_amplitudes);
        motion_window_.clear();
        resetCalibration();
        frame.motion_ready = false;
        frame.motion_score = 0.0f;
        return;
    }

    if (!previous_amplitudes_.empty())
    {
        const size_t n = current_amplitudes.size();
        float mean_x = std::accumulate(current_amplitudes.begin(), current_amplitudes.end(), 0.0f) / n;
        float mean_y = std::accumulate(previous_amplitudes_.begin(), previous_amplitudes_.end(), 0.0f) / n;

        float num = 0.0f;
        float den_x = 0.0f;
        float den_y = 0.0f;

        for (size_t i = 0; i < n; ++i)
        {
            float dx = current_amplitudes[i] - mean_x;
            float dy = previous_amplitudes_[i] - mean_y;
            num += dx * dy;
            den_x += dx * dx;
            den_y += dy * dy;
        }

        float correlation = 1.0f;
        if (den_x > 1.0e-9f && den_y > 1.0e-9f)
        {
            correlation = num / std::sqrt(den_x * den_y);
            correlation = std::clamp(correlation, -1.0f, 1.0f);
        }

        motion_window_.push_back(1.0f - correlation);
        if (motion_window_.size() > kMotionWindowSize)
            motion_window_.pop_front();
    }

    previous_amplitudes_ = std::move(current_amplitudes);
    frame.motion_ready = (motion_window_.size() == kMotionWindowSize);
    frame.motion_window_samples = motion_window_.size();

    if (frame.motion_ready)
    {
        frame.motion_score = std::accumulate(motion_window_.begin(), motion_window_.end(), 0.0f) / motion_window_.size();

        if (!motion_calibrated_)
        {
            calibration_sample_stable_ = false;

            const bool high_motion = frame.motion_score > kMaximumStableMotionScore;
            const bool step_too_big = !calibration_candidate_.empty() &&
                                      std::abs(frame.motion_score - calibration_candidate_.back()) > kMaximumStableScoreStep;

            if (high_motion || step_too_big)
            {
                ++calibration_disruption_streak_;
                if (calibration_disruption_streak_ >= 2)
                {
                    calibration_candidate_.clear();
                    calibration_disruption_streak_ = 0;
                }
            }
            else
            {
                calibration_disruption_streak_ = 0;
                calibration_candidate_.push_back(frame.motion_score);
                if (calibration_candidate_.size() > kStableCandidateSize)
                    calibration_candidate_.pop_front();

                if (calibration_candidate_.size() == kStableCandidateSize)
                {
                    float candidate_mean = std::accumulate(calibration_candidate_.begin(), calibration_candidate_.end(), 0.0f) / kStableCandidateSize;
                    float variance = 0.0f;
                    for (float s : calibration_candidate_)
                        variance += (s - candidate_mean) * (s - candidate_mean);
                    float standard_deviation = std::sqrt(variance / kStableCandidateSize);

                    if (standard_deviation <= kMaximumStableScoreDeviation)
                    {
                        calibration_sample_stable_ = true;
                        float baseline_deviation = std::max(standard_deviation, kMinimumBaselineDeviation);
                        motion_threshold_high_ = candidate_mean + 5.0f * baseline_deviation;
                        motion_threshold_low_ = candidate_mean + 3.0f * baseline_deviation;
                        motion_calibrated_ = true;
                    }
                }
            }
        }
        else if (frame.motion_score > motion_threshold_high_)
        {
            presence_detected_ = true;
            low_motion_frames_ = 0;
        }
        else if (frame.motion_score < motion_threshold_low_)
        {
            if (++low_motion_frames_ >= kLowMotionFramesToClear)
            {
                presence_detected_ = false;
                low_motion_frames_ = kLowMotionFramesToClear;
            }
        }
        else
        {
            low_motion_frames_ = 0;
        }
    }

    frame.motion_calibrated = motion_calibrated_;
    frame.motion_calibration_frames = motion_calibrated_ ? kStableCandidateSize : calibration_candidate_.size();
    frame.motion_calibration_stable = motion_calibrated_ || calibration_sample_stable_;
    frame.motion_threshold_high = motion_threshold_high_;
    frame.motion_threshold_low = motion_threshold_low_;
    frame.presence_detected = presence_detected_;
}