#include "csi_parser.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <utility>

namespace
{
constexpr uint8_t kRelayMagic[] = {0xaf, 0xbe, 0xad, 0xde};
constexpr uint8_t kCfrMagic[] = {0xba, 0x00, 0xde, 0xc0};
constexpr uint8_t kRelayEndMagic[] = {0xad, 0xde, 0xaf, 0xbe};
constexpr size_t kRelayHeaderSize = 22;
constexpr size_t kMinimumCfrHeaderSize = 64;
constexpr size_t kMaximumMetadataSize = 4096;
constexpr size_t kMaximumCfrHeaderSize = 4096;
constexpr size_t kMaximumPayloadSize = 16 * 1024 * 1024;
constexpr size_t kMaximumPendingSize = 32 * 1024 * 1024;
constexpr size_t kMotionWindowSize = 30;
constexpr size_t kStableCandidateSize = 50;
constexpr size_t kLowMotionFramesToClear = 15;
constexpr float kMinimumBaselineDeviation = 1.0e-3f;
constexpr float kMaximumStableMotionScore = 1.0e-1f;
constexpr float kMaximumStableScoreDeviation = 1.0e-2f;
constexpr float kMaximumStableScoreStep = 2.0e-2f;

uint16_t readU16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readU32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

uint64_t readU64(const uint8_t *data)
{
    return static_cast<uint64_t>(readU32(data)) |
           (static_cast<uint64_t>(readU32(data + 4)) << 32);
}

size_t findMagic(const std::vector<uint8_t> &data, const uint8_t *magic, size_t from)
{
    if (data.size() < 4 || from > data.size() - 4)
        return std::string::npos;

    for (size_t i = from; i + 4 <= data.size(); ++i)
    {
        if (std::equal(magic, magic + 4, data.begin() + static_cast<std::ptrdiff_t>(i)))
            return i;
    }
    return std::string::npos;
}

void appendFloatArray(std::ostringstream &json, const std::vector<float> &values)
{
    json << '[';
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
            json << ',';
        json << values[i];
    }
    json << ']';
}

void appendIntArray(std::ostringstream &json, const std::vector<int16_t> &values)
{
    json << '[';
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
            json << ',';
        json << values[i];
    }
    json << ']';
}
}

size_t CSIData::tonesPerChain() const
{
    return chains.empty() ? 0 : chains.front().amplitudes.size();
}

std::string CSIData::toJson() const
{
    std::ostringstream json;
    json << std::setprecision(7)
         << "{\"has_data\":true,\"timestamp_ns\":" << timestamp_ns
         << ",\"chip_tsf_us\":" << chip_tsf_us
         << ",\"phy_ppdu_id\":" << phy_ppdu_id
         << ",\"num_chains\":" << static_cast<unsigned int>(num_chains)
         << ",\"nss\":" << static_cast<unsigned int>(nss)
         << ",\"channel_bw_code\":" << static_cast<unsigned int>(channel_bw)
         << ",\"packet_bw_code\":" << static_cast<unsigned int>(packet_bw)
         << ",\"tones_per_chain\":" << tonesPerChain()
         << ",\"subcarriers_count\":" << tonesPerChain() * chains.size()
         << ",\"motion_score\":" << motion_score
         << ",\"motion_ready\":" << (motion_ready ? "true" : "false")
         << ",\"motion_window_samples\":" << motion_window_samples
         << ",\"motion_calibrated\":" << (motion_calibrated ? "true" : "false")
         << ",\"motion_calibration_frames\":" << motion_calibration_frames
         << ",\"motion_calibration_stable\":" << (motion_calibration_stable ? "true" : "false")
         << ",\"motion_threshold_high\":" << motion_threshold_high
         << ",\"motion_threshold_low\":" << motion_threshold_low
         << ",\"presence_detected\":" << (presence_detected ? "true" : "false")
         << ",\"ap_mac\":\"" << ap_mac << "\",\"amplitude_gain_ratio\":[";

    for (size_t i = 0; i < 5; ++i)
    {
        if (i != 0)
            json << ',';
        json << amplitude_gain_ratio[i];
    }

    json << "],\"chains\":[";
    for (size_t i = 0; i < chains.size(); ++i)
    {
        if (i != 0)
            json << ',';
        json << "{\"i\":";
        appendIntArray(json, chains[i].i);
        json << ",\"q\":";
        appendIntArray(json, chains[i].q);
        json << ",\"amplitudes\":";
        appendFloatArray(json, chains[i].amplitudes);
        json << ",\"phases\":";
        appendFloatArray(json, chains[i].phases);
        json << '}';
    }
    json << "]}";
    return json.str();
}

bool CSIParser::feed(const uint8_t *data, size_t size, std::vector<CSIData> &frames)
{
    frames.clear();
    if (data == nullptr && size != 0)
        return false;
    if (size > kMaximumPendingSize || pending_.size() > kMaximumPendingSize - size)
    {
        reset();
        return false;
    }
    if (size != 0)
        pending_.insert(pending_.end(), data, data + size);

    while (true)
    {
        const size_t relay_start = findMagic(pending_, kRelayMagic, 0);
        if (relay_start == std::string::npos)
        {
            if (pending_.size() > 3)
                pending_.erase(pending_.begin(), pending_.end() - 3);
            break;
        }
        if (relay_start != 0)
            pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(relay_start));
        if (pending_.size() < kRelayHeaderSize)
            break;

        const uint32_t metadata_size = readU32(pending_.data() + 10);
        if (metadata_size > kMaximumMetadataSize)
        {
            pending_.erase(pending_.begin());
            continue;
        }
        const size_t cfr_offset = kRelayHeaderSize + metadata_size;
        if (pending_.size() < cfr_offset + 12)
            break;
        if (!std::equal(kCfrMagic, kCfrMagic + 4, pending_.begin() + static_cast<std::ptrdiff_t>(cfr_offset)))
        {
            pending_.erase(pending_.begin());
            continue;
        }

        const uint8_t *cfr_header = pending_.data() + cfr_offset;
        const size_t cfr_header_size = static_cast<size_t>(cfr_header[7]) * 4;
        const uint32_t payload_size = readU32(cfr_header + 8);
        if (cfr_header_size < kMinimumCfrHeaderSize ||
            cfr_header_size > kMaximumCfrHeaderSize ||
            payload_size == 0 || payload_size > kMaximumPayloadSize)
        {
            pending_.erase(pending_.begin());
            continue;
        }

        const size_t record_size = cfr_offset + cfr_header_size + payload_size + 4;
        if (record_size > kMaximumPendingSize)
        {
            pending_.erase(pending_.begin());
            continue;
        }
        if (pending_.size() < record_size)
            break;
        if (!std::equal(kRelayEndMagic, kRelayEndMagic + 4,
                        pending_.begin() + static_cast<std::ptrdiff_t>(record_size - 4)))
        {
            pending_.erase(pending_.begin());
            continue;
        }

        CSIData frame;
        if (decodeRecord(pending_.data(), record_size, frame))
        {
            frames.push_back(std::move(frame));
        }
        pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(record_size));
    }
    return true;
}

void CSIParser::reset()
{
    pending_.clear();
}

void MotionDetector::reset()
{
    previous_amplitudes_.clear();
    motion_window_.clear();
    resetCalibration();
    presence_detected_ = false;
}

void MotionDetector::resetCalibration()
{
    calibration_candidate_.clear();
    low_motion_frames_ = 0;
    motion_threshold_high_ = 0.0f;
    motion_threshold_low_ = 0.0f;
    motion_calibrated_ = false;
    calibration_sample_stable_ = false;
}

void MotionDetector::update(CSIData &frame)
{
    std::vector<float> current_amplitudes;
    for (const CSIChain &chain : frame.chains)
        current_amplitudes.insert(current_amplitudes.end(), chain.amplitudes.begin(), chain.amplitudes.end());

    if (current_amplitudes.size() != previous_amplitudes_.size())
    {
        previous_amplitudes_.clear();
        motion_window_.clear();
        resetCalibration();
        presence_detected_ = false;
    }

    if (!previous_amplitudes_.empty())
    {
        const float current_mean = std::accumulate(current_amplitudes.begin(), current_amplitudes.end(), 0.0f) /
                                   static_cast<float>(current_amplitudes.size());
        const float previous_mean = std::accumulate(previous_amplitudes_.begin(), previous_amplitudes_.end(), 0.0f) /
                                    static_cast<float>(previous_amplitudes_.size());

        float covariance = 0.0f;
        float current_variance = 0.0f;
        float previous_variance = 0.0f;
        for (size_t i = 0; i < current_amplitudes.size(); ++i)
        {
            const float current = current_amplitudes[i] / (current_mean + 1.0e-6f);
            const float previous = previous_amplitudes_[i] / (previous_mean + 1.0e-6f);
            const float current_delta = current - current_mean / (current_mean + 1.0e-6f);
            const float previous_delta = previous - previous_mean / (previous_mean + 1.0e-6f);
            covariance += current_delta * previous_delta;
            current_variance += current_delta * current_delta;
            previous_variance += previous_delta * previous_delta;
        }

        float correlation = 0.0f;
        const float denominator = std::sqrt(current_variance * previous_variance);
        if (denominator > 1.0e-12f)
        {
            correlation = std::clamp(covariance / denominator, -1.0f, 1.0f);
        }
        else
        {
            const bool same_shape = std::equal(
                current_amplitudes.begin(), current_amplitudes.end(), previous_amplitudes_.begin(),
                [current_mean, previous_mean](float current, float previous) {
                    const float normalized_current = current / (current_mean + 1.0e-6f);
                    const float normalized_previous = previous / (previous_mean + 1.0e-6f);
                    return std::abs(normalized_current - normalized_previous) <= 1.0e-6f;
                });
            correlation = same_shape ? 1.0f : 0.0f;
        }

        motion_window_.push_back(1.0f - correlation);
        if (motion_window_.size() > kMotionWindowSize)
            motion_window_.pop_front();
    }

    previous_amplitudes_ = std::move(current_amplitudes);
    frame.motion_ready = motion_window_.size() == kMotionWindowSize;
    frame.motion_window_samples = motion_window_.size();

    if (frame.motion_ready)
    {
        std::vector<float> sorted_scores(motion_window_.begin(), motion_window_.end());
        std::sort(sorted_scores.begin(), sorted_scores.end());
        frame.motion_score = (sorted_scores[(kMotionWindowSize - 1) / 2] +
                              sorted_scores[kMotionWindowSize / 2]) *
                             0.5f;

        if (!motion_calibrated_)
        {
            calibration_sample_stable_ = false;
            if (frame.motion_score > kMaximumStableMotionScore)
            {
                calibration_candidate_.clear();
            }
            else
            {
                if (!calibration_candidate_.empty() &&
                    std::abs(frame.motion_score - calibration_candidate_.back()) > kMaximumStableScoreStep)
                {
                    calibration_candidate_.clear();
                }
                calibration_candidate_.push_back(frame.motion_score);
                calibration_sample_stable_ = true;
                if (calibration_candidate_.size() == kStableCandidateSize)
                {
                    const float candidate_mean =
                        std::accumulate(calibration_candidate_.begin(), calibration_candidate_.end(), 0.0f) /
                        static_cast<float>(calibration_candidate_.size());
                    float squared_difference = 0.0f;
                    for (const float score : calibration_candidate_)
                    {
                        const float difference = score - candidate_mean;
                        squared_difference += difference * difference;
                    }
                    const float candidate_deviation =
                        std::sqrt(squared_difference / static_cast<float>(calibration_candidate_.size()));

                    if (candidate_deviation <= kMaximumStableScoreDeviation)
                    {
                        const float baseline_deviation = std::max(candidate_deviation, kMinimumBaselineDeviation);
                        motion_threshold_high_ = candidate_mean + 5.0f * baseline_deviation;
                        motion_threshold_low_ = candidate_mean + 3.0f * baseline_deviation;
                        motion_calibrated_ = true;
                    }
                    else
                    {
                        calibration_candidate_.clear();
                        calibration_candidate_.push_back(frame.motion_score);
                        calibration_sample_stable_ = false;
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
            ++low_motion_frames_;
            if (low_motion_frames_ >= kLowMotionFramesToClear)
                presence_detected_ = false;
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

bool CSIParser::decodeRecord(const uint8_t *record, size_t size, CSIData &frame)
{
    if (size < kRelayHeaderSize + kMinimumCfrHeaderSize + 4)
        return false;

    const uint32_t metadata_size = readU32(record + 10);
    const size_t cfr_offset = kRelayHeaderSize + metadata_size;
    if (cfr_offset + kMinimumCfrHeaderSize + 4 > size)
        return false;

    const uint8_t *header = record + cfr_offset;
    const size_t header_size = static_cast<size_t>(header[7]) * 4;
    const uint32_t payload_size = readU32(header + 8);
    const uint8_t chain_count = header[18];
    if (header_size < kMinimumCfrHeaderSize ||
        header_size > size - cfr_offset - 4 ||
        payload_size > size - cfr_offset - header_size - 4 ||
        chain_count == 0 || chain_count > 8 ||
        payload_size % (4 * chain_count) != 0 ||
        header_size < 90)
        return false;

    frame.timestamp_ns = readU64(record + 14);
    frame.phy_ppdu_id = readU16(header + 16);
    frame.num_chains = chain_count;
    frame.nss = header[19];
    frame.channel_bw = header[20];
    frame.packet_bw = header[21];
    frame.chip_tsf_us = readU64(header + 82);
    for (size_t chain = 0; chain < 5; ++chain)
        frame.amplitude_gain_ratio[chain] = header[44 + chain];
    {
        std::ostringstream mac;
        mac << std::hex << std::setfill('0');
        for (size_t i = 76; i < 82; ++i)
        {
            if (i != 76)
                mac << ':';
            mac << std::setw(2) << static_cast<unsigned int>(header[i]);
        }
        frame.ap_mac = mac.str();
    }

    const size_t tones_per_chain = payload_size / (4 * chain_count);
    const uint8_t *payload = header + header_size;
    frame.chains.resize(chain_count);
    for (size_t chain_index = 0; chain_index < chain_count; ++chain_index)
    {
        CSIChain &chain = frame.chains[chain_index];
        chain.i.reserve(tones_per_chain);
        chain.q.reserve(tones_per_chain);
        chain.amplitudes.reserve(tones_per_chain);
        chain.phases.reserve(tones_per_chain);
        for (size_t tone = 0; tone < tones_per_chain; ++tone)
        {
            const size_t offset = (chain_index * tones_per_chain + tone) * 4;
            const int16_t i_value = static_cast<int16_t>(readU16(payload + offset));
            const int16_t q_value = static_cast<int16_t>(readU16(payload + offset + 2));
            const float i_float = static_cast<float>(i_value);
            const float q_float = static_cast<float>(q_value);
            chain.i.push_back(i_value);
            chain.q.push_back(q_value);
            chain.amplitudes.push_back(std::sqrt(i_float * i_float + q_float * q_float));
            chain.phases.push_back(std::atan2(q_float, i_float));
        }
    }
    return true;
}
