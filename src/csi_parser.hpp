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

    // Gesture Detection (Wave - VNPT Sensing 3.2s)
    bool wave_detected = false;
    float wave_confidence = 0.0f;
    std::string wave_status = "idle"; // "idle", "waving", "unavailable"
    uint32_t wave_samples_in_window = 0;
    float wave_raw_prob = 0.0f;
    std::string wave_reason = "idle";

    // RSSI & AGC Hardware Metadata
    int8_t rssi_comb_db = 0;
    float smoothed_rssi_db = 0.0f;
    uint8_t agc_indices[3]{};

    // Distance Estimation (RSSI + CSI Fusion)
    float estimated_distance_m = 0.0f;
    float distance_confidence = 0.0f;
    float distance_error_margin_m = 0.15f;      // Sai số cự ly ước tính (± m)
    std::string distance_env_mode = "los";      // "los", "standard", "nlos"
    std::string distance_zone = "unknown";      // "immediate", "near", "mid", "far"
    std::string distance_trend = "stationary";  // "approaching", "departing", "stationary"
    float csi_los_ratio = 0.0f;
    float csi_rician_k = 0.0f;
    std::string distance_band = "5.2 GHz";      // "2.4 GHz", "5.2 GHz", "6 GHz"
    float distance_freq_mhz = 5210.0f;
    float distance_a_1m = -40.0f;
    float distance_n = 1.75f;
    std::string distance_cal_source = "default"; // "default", "custom"

    // Link Stability & Connection Quality Assessment (RSSI + CSI Fusion)
    float link_stability_score = 0.0f;           // Điểm chất lượng kết nối 0 - 100
    std::string link_status_text = "calibrating"; // "clean_los", "multipath_stable", "deep_fade", "dynamic_motion", "weak"
    float spectral_correlation = 1.0f;           // Hệ số tương quan vân phổ Pearson với frame trước
    float deep_fade_ratio = 1.0f;                // Tỷ số suy hao chọn lọc tần số (Mean/Min)
    float temporal_csi_variance = 0.0f;          // Phương sai biên độ theo thời gian (độ động môi trường)
    bool is_agc_event = false;                   // Cờ báo hiệu bước nhảy do AGC phần cứng
    bool deep_fade_warning = false;              // Cảnh báo xuất hiện vết lõm tần số (nguy cơ lỗi MCS cao)

    size_t tonesPerChain() const;
    std::string toJson(bool include_raw_iq = false, bool include_subcarriers = false) const;
};

std::string escapeJsonString(const std::string &input);

class CSIParser {
  public:
    bool feed(const uint8_t *data, size_t size, std::vector<CSIData> &frames);
    void reset();

  private:
    std::vector<uint8_t> pending_;
    int8_t last_valid_rssi_db_ = 0;
    bool decodeRecord(const uint8_t *record, size_t size, CSIData &frame);
};

#endif
