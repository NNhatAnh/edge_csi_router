#ifndef BREATH_HPP
#define BREATH_HPP

#include "csi_parser.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

struct WaveMorphology {
    float phase_span = 0.0f;
    float asymmetry_ratio = 0.0f;
    float rrv_cv = 0.0f;
    float regularity = 0.0f;
    size_t peak_count = 0;
    float average_ibi_s = 0.0f;
    bool is_human = false;
    bool is_sleeping = false;
    std::string classification;
};

class BreathDetector {
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

    // Metrics for human vs object discrimination
    float current_bpm_ = 0.0f;
    float current_wave_ = 0.0f;
    float current_quality_ = 0.0f;
    float current_phase_span_ = 0.0f;
    float current_asymmetry_ = 0.0f;
    float current_rrv_ = 0.0f;
    float current_regularity_ = 0.0f;
    bool current_breath_valid_ = false;
    bool is_human_presence_ = false;
    bool is_sleeping_ = false;
    size_t settling_frames_ = 0;
    std::string presence_status_ = "empty_room";

    // Breath wave history for morphology analysis
    std::deque<std::pair<uint64_t, float>> breath_wave_history_;

    // Stability persistence
    size_t human_confirm_streak_ = 0;
    size_t reject_streak_ = 0;

    std::vector<float> getWrappedPhaseDiff(const CSIData &frame) const;
    void updateBreathingEstimate(CSIData &frame);
    WaveMorphology analyzeWaveform();
    float computeAutocorrelationRegularity(const std::vector<double> &wave,
                                           float frequency_hz,
                                           double sample_rate) const;
};

#endif
