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
    float coherence = 0.0f;
    float prominence = 0.0f;
    float evidence_score = 0.0f;
    size_t peak_count = 0;
    float average_ibi_s = 0.0f;
    bool is_human = false;
    bool is_sleeping = false;
    std::string classification;
};

// Bộ lọc Bandpass thời gian thực IIR (0.10 Hz - 0.55 Hz) chạy liên tục trên từng frame
class BreathBandpassFilter {
  public:
    BreathBandpassFilter();
    void reset();
    float process(float input, float dt_s);

  private:
    float prev_x_ = 0.0f;
    float y_hp_ = 0.0f;
    float y_lp1_ = 0.0f;
    float y_lp2_ = 0.0f;
    bool initialized_ = false;
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

    // Bộ lọc IIR thời gian thực chạy trên từng frame đơn lẻ
    BreathBandpassFilter real_time_filter_;
    uint64_t last_frame_time_ns_ = 0;

    // Danh sách các subcarrier nhạy cảm nhất được chọn lọc
    std::vector<size_t> sensitive_tones_;

    // Các chỉ số phục vụ nhận diện hiện diện con người vs môi trường
    float current_bpm_ = 0.0f;
    float current_wave_ = 0.0f;
    float current_quality_ = 0.0f;
    float current_phase_span_ = 0.0f;
    float current_asymmetry_ = 0.0f;
    float current_rrv_ = 0.0f;
    float current_regularity_ = 0.0f;
    float current_coherence_ = 0.0f;
    float current_prominence_ = 0.0f;
    bool current_breath_valid_ = false;
    bool is_human_presence_ = false;
    bool is_sleeping_ = false;
    size_t settling_frames_ = 0;
    std::string presence_status_ = "empty_room";

    // Lịch sử sóng hơi thở đã lọc
    std::deque<std::pair<uint64_t, float>> breath_wave_history_;

    // Bộ tích lũy độ tin cậy rò rỉ (Leaky Integrator Confidence Engine)
    float presence_confidence_ = 0.0f;
    size_t sleep_qualify_counter_ = 0;

    // Dynamic Floor Profiling & Fast Exit Recovery
    float noise_floor_std_ = 0.015f;
    size_t gross_motion_cooldown_ = 0;

    std::vector<float> getWrappedPhaseDiff(const CSIData &frame) const;
    void updateSensitiveTones(const CSIData &frame);
    float synthesizeCompositePhase(const std::vector<float> &unwrapped) const;
    void updateBreathingEstimate(CSIData &frame);
    WaveMorphology analyzeWaveform();
    float computeAutocorrelationRegularity(const std::vector<double> &wave,
                                           float frequency_hz,
                                           double sample_rate) const;
    float computeSubcarrierCoherence() const;
};

#endif
