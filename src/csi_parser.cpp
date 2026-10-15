#include "csi_parser.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
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
    constexpr size_t kMinimumCfrHeaderSize = 90;
    constexpr size_t kMaximumMetadataSize = 4096;
    constexpr size_t kMaximumCfrHeaderSize = 4096;
    constexpr size_t kMaximumPayloadSize = 16 * 1024 * 1024;
    constexpr size_t kMaximumPendingSize = 32 * 1024 * 1024;

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
} // namespace

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
         << ",\"breath_bpm\":" << breath_bpm
         << ",\"breath_wave\":" << breath_wave
         << ",\"breath_valid\":" << (breath_valid ? "true" : "false")
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
            frames.push_back(std::move(frame));

        pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(record_size));
    }
    return true;
}

void CSIParser::reset()
{
    pending_.clear();
}

bool CSIParser::decodeRecord(const uint8_t *record, size_t size, CSIData &frame)
{
    if (size < kRelayHeaderSize + kMinimumCfrHeaderSize + 4)
        return false;

    const uint32_t metadata_size = readU32(record + 10);
    const size_t cfr_offset = kRelayHeaderSize + metadata_size;
    if (metadata_size > kMaximumMetadataSize ||
        cfr_offset > size ||
        size - cfr_offset < kMinimumCfrHeaderSize + 4)
        return false;

    const uint8_t *cfr_header = record + cfr_offset;
    const size_t cfr_header_size = static_cast<size_t>(cfr_header[7]) * 4;
    const uint32_t payload_size = readU32(cfr_header + 8);
    const uint8_t chain_count = cfr_header[18];
    if (cfr_header_size < kMinimumCfrHeaderSize ||
        cfr_header_size > size - cfr_offset - 4 ||
        payload_size > size - cfr_offset - cfr_header_size - 4 ||
        chain_count == 0 || chain_count > 8 ||
        payload_size == 0 || payload_size % (4 * chain_count) != 0)
        return false;

    frame.timestamp_ns = readU64(record + 14);
    frame.chip_tsf_us = readU64(cfr_header + 82);
    frame.phy_ppdu_id = readU16(cfr_header + 16);
    frame.num_chains = chain_count;
    frame.nss = cfr_header[19];
    frame.channel_bw = cfr_header[20];
    frame.packet_bw = cfr_header[21];

    for (size_t i = 0; i < 5; ++i)
        frame.amplitude_gain_ratio[i] = cfr_header[44 + i];

    char mac_buf[18]{};
    std::snprintf(mac_buf, sizeof(mac_buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                  cfr_header[76], cfr_header[77], cfr_header[78],
                  cfr_header[79], cfr_header[80], cfr_header[81]);
    frame.ap_mac = mac_buf;

    const size_t raw_tones = payload_size / (frame.num_chains * 4);
    const uint8_t *payload = cfr_header + cfr_header_size;
    frame.chains.resize(frame.num_chains);

    for (size_t chain_idx = 0; chain_idx < frame.num_chains; ++chain_idx)
    {
        auto &chain = frame.chains[chain_idx];
        chain.i.resize(raw_tones);
        chain.q.resize(raw_tones);
        chain.amplitudes.resize(raw_tones);
        chain.phases.resize(raw_tones);

        for (size_t tone = 0; tone < raw_tones; ++tone)
        {
            const size_t byte_idx = (chain_idx * raw_tones + tone) * 4;
            const int16_t ival = static_cast<int16_t>(readU16(payload + byte_idx));
            const int16_t qval = static_cast<int16_t>(readU16(payload + byte_idx + 2));

            chain.i[tone] = ival;
            chain.q[tone] = qval;
            chain.amplitudes[tone] = std::sqrt(static_cast<float>(ival * ival + qval * qval));
            chain.phases[tone] = std::atan2(static_cast<float>(qval), static_cast<float>(ival));
        }
    }
    return true;
}