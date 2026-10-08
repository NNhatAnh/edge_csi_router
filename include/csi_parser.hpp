#ifndef CSI_PARSER_HPP
#define CSI_PARSER_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

struct CSIChain
{
    std::vector<int16_t> i;
    std::vector<int16_t> q;
    std::vector<float> amplitudes;
    std::vector<float> phases;
};

struct CSIData
{
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
    float motion_score = 0.0f;
    float motion_threshold_high = 0.0f;
    float motion_threshold_low = 0.0f;
    size_t motion_window_samples = 0;
    size_t motion_calibration_frames = 0;
    bool motion_calibration_stable = false;
    bool motion_ready = false;
    bool motion_calibrated = false;
    bool presence_detected = false;

    size_t tonesPerChain() const;
    std::string toJson() const;
};

class CSIParser
{
public:
    bool feed(const uint8_t *data, size_t size, std::vector<CSIData> &frames);
    void reset();

private:
    std::vector<uint8_t> pending_;

    bool decodeRecord(const uint8_t *record, size_t size, CSIData &frame);
};

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

    void resetCalibration();
};

#endif
