#ifndef WAVE_DETECTOR_HPP
#define WAVE_DETECTOR_HPP

#include "csi_parser.hpp"
#include <deque>
#include <string>
#include <vector>

/**
 * Hand-Wave Gesture Detector (Original model_package / wave_rf.joblib).
 *
 * Pipeline:
 * - 2.0-second sliding window (WINDOW_S = 2.0s)
 * - 80 MHz channel (208 non-DC subcarriers, with 20/40MHz resample fallback)
 * - Minimum 10 valid samples and 5 short consecutive pairs (dt <= 100ms)
 * - 5 amplitude & shape-step features matching sparse_wave_v1:
 *     0: relative_delta_rate_c0
 *     1: relative_delta_rate_c1
 *     2: relative_delta_rate_c2 (virtual fallback to c1 when 2 antennas)
 *     3: shape_step_rate_c0
 *     4: shape_step_rate_c2 (virtual fallback to c0 when 2 antennas)
 * - Evaluated with 300 Random Forest trees (pure C implementation)
 * - Trigger threshold = 0.35 (matching protocol.json / test_metrics.json)
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

    void setThreshold(float threshold) { trigger_threshold_ = threshold; }
    float getThreshold() const { return trigger_threshold_; }

  private:
    static constexpr size_t kMinChains = 2;
    static constexpr size_t kMaxChains = 3;
    static constexpr size_t kNonDcTones = 208;
    static constexpr uint64_t kWindowUs = 2000000ULL;       // 2.0s window
    static constexpr uint64_t kMaxPairGapUs = 100000ULL;    // 100ms max pair gap
    static constexpr uint64_t kHoldDurationUs = 1500000ULL; // 1.5s active hold time
    static constexpr size_t kMinSamples = 10;               // Minimum samples in 2.0s
    static constexpr size_t kMinShortPairs = 5;             // Minimum consecutive pairs
    static constexpr float kDefaultThreshold = 0.35f;       // Model threshold

    struct Sample {
        uint64_t timestamp_us = 0;
        float amp[kMaxChains][kNonDcTones]{};
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
    float trigger_threshold_ = kDefaultThreshold;

    void processWindow(uint64_t current_ts_us);
    static float computeMedian(std::vector<float> &values);
};

#endif // WAVE_DETECTOR_HPP
