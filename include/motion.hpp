#ifndef MOTION_HPP
#define MOTION_HPP

#include "csi_parser.hpp"
#include <deque>
#include <vector>

class MotionDetector
{
public:
    void update(CSIData &frame);
    void reset();

private:
    std::vector<float> previous_amplitudes_;
    std::deque<float> motion_window_;
    std::deque<float> calibration_candidate_;
    size_t low_motion_frames_ = 0;
    float motion_threshold_high_ = 0.0f;
    float motion_threshold_low_ = 0.0f;
    bool motion_calibrated_ = false;
    bool calibration_sample_stable_ = false;
    bool presence_detected_ = false;
    size_t calibration_disruption_streak_ = 0;

    void resetCalibration();
};

#endif