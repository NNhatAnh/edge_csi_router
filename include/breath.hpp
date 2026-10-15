#ifndef BREATH_HPP
#define BREATH_HPP

#include "csi_parser.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

class BreathDetector
{
public:
    BreathDetector(size_t window_size = 900);
    void update(CSIData &frame);
    void reset();

private:
    size_t window_size_;
    std::deque<std::vector<float>> phase_window_;
    std::deque<uint64_t> timestamp_window_ns_;
    std::vector<float> previous_wrapped_phase_;
    std::vector<float> previous_unwrapped_phase_;
    size_t estimate_counter_ = 0;
    float current_bpm_ = 0.0f;
    float current_quality_ = 0.0f;

    std::vector<float> getWrappedPhaseDiff(const CSIData &frame) const;
    void updateBreathingEstimate(CSIData &frame);
};

#endif
