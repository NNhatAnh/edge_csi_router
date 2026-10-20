#ifndef CSI_PARSER_HPP
#define CSI_PARSER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct CSIChain {
    std::vector<int16_t> i;
    std::vector<int16_t> q;
    std::vector<float> amplitudes;
    std::vector<float> phases;
};

struct CSIData {
    uint64_t timestamp_ns = 0;
    uint64_t chip_tsf_us = 0;
    uint16_t phy_ppdu_id = 0;
    uint8_t num_chains = 0;
    uint8_t nss = 0;
    uint8_t channel_bw = 0;
    uint8_t packet_bw = 0;
    uint16_t amplitude_gain_ratio[5]{};
    std::string ap_mac;
    std::vector<CSIChain> chains;

    // Macro-motion
    float motion_score = 0.0f;
    float motion_threshold_high = 0.0f;
    float motion_threshold_low = 0.0f;
    size_t motion_window_samples = 0;
    size_t motion_calibration_frames = 0;
    bool motion_calibration_stable = false;
    bool motion_ready = false;
    bool motion_calibrated = false;
    bool presence_detected = false;

    // Breath (Micro-motion) & Human vs Object Discrimination
    float breath_bpm = 0.0f;
    float breath_wave = 0.0f;
    float breath_quality = 0.0f;
    float breath_phase_span = 0.0f;
    float breath_asymmetry = 0.0f;
    float breath_rrv = 0.0f;
    float breath_regularity = 0.0f;
    bool breath_valid = false;
    bool is_human_presence = false;
    bool is_sleeping = false;
    std::string presence_status = "empty_room";
    size_t tonesPerChain() const;
    std::string toJson() const;
};

class CSIParser {
  public:
    bool feed(const uint8_t *data, size_t size, std::vector<CSIData> &frames);
    void reset();

  private:
    std::vector<uint8_t> pending_;
    bool decodeRecord(const uint8_t *record, size_t size, CSIData &frame);
};

#endif
