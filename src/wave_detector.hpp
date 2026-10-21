#ifndef WAVE_DETECTOR_HPP
#define WAVE_DETECTOR_HPP

#include "csi_parser.hpp"
#include <deque>
#include <string>
#include <vector>

/**
 * Hand-Wave Gesture Detector (VNPT Wi-Fi Sensing Model).
 *
 * Pipeline:
 * - 3.2-second sliding window
 * - 80 MHz channel (208 non-DC subcarriers)
 * - 3 antenna chains (c0, c1, c2)
 * - Interpolated onto uniform 20 Hz grid (64 time points)
 * - 9 motion_core features:
 *     3 chains x (std, step, log_power)
 * - Evaluated with Random Forest (96 trees)
 */
class WaveDetector {
  public:
    WaveDetector();

    /**
     * Process a new incoming CSI frame and update gesture detection state.
     */
    void update(CSIData &frame);

    /**
     * Reset internal buffers and detection state.
     */
    void reset();

    bool isWaving() const { return is_waving_; }
    float getConfidence() const { return wave_confidence_; }
    float getRawProb() const { return raw_prob_; }
    uint32_t getSamplesInWindow() const { return samples_in_window_; }
    const std::string &getStatus() const { return status_; }
    const std::string &getReason() const { return reason_; }

  private:
    static constexpr size_t kRequiredChains = 3;
    static constexpr size_t kNonDcTones = 208;
    static constexpr uint64_t kWindowUs = 3200000ULL;       // 3.2s window
    static constexpr uint64_t kHoldDurationUs = 1500000ULL; // 1.5s active hold time
    static constexpr size_t kGridSamples = 64;              // 64 points on 20Hz grid
    static constexpr size_t kMinSamples = 16;               // At least 5 Hz in 3.2s
    static constexpr float kTriggerThreshold = 0.40f;       // Instant trigger threshold
    static constexpr float kReleaseThreshold = 0.25f;       // Cooldown release threshold

    struct Sample {
        uint64_t timestamp_us = 0;
        std::vector<float> amplitudes[kRequiredChains];
    };

    std::deque<Sample> history_;
    bool is_waving_ = false;
    float wave_confidence_ = 0.0f;
    float raw_prob_ = 0.0f;
    uint32_t samples_in_window_ = 0;
    std::string status_ = "idle"; // "idle", "waving", "unavailable"
    std::string reason_ = "idle";
    uint64_t last_active_ts_us_ = 0;
    uint64_t last_infer_ts_us_ = 0;

    // Precalculated Hann window and Fourier harmonic tables
    double hann_[kGridSamples]{};
    double twc_[33][kGridSamples]{};
    double tws_[33][kGridSamples]{};

    void initTables();
    void processWindow(uint64_t current_ts_us);
};

#endif // WAVE_DETECTOR_HPP
