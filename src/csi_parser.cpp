#include "csi_parser.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace {
constexpr uint8_t kRelayMagic[] = {0xaf, 0xbe, 0xad, 0xde};
constexpr uint8_t kCfrMagic[] = {0xba, 0x00, 0xde, 0xc0};
constexpr uint8_t kRelayEndMagic[] = {0xad, 0xde, 0xaf, 0xbe};
constexpr size_t kRelayHeaderSize = 22;
constexpr size_t kMinimumCfrHeaderSize = 90;
constexpr size_t kMaximumMetadataSize = 4096;
constexpr size_t kMaximumCfrHeaderSize = 4096;
constexpr size_t kMaximumPayloadSize = 16 * 1024 * 1024;
constexpr size_t kMaximumPendingSize = 32 * 1024 * 1024;

uint16_t readU16(const uint8_t *data) {
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readU32(const uint8_t *data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

uint64_t readU64(const uint8_t *data) {
    return static_cast<uint64_t>(readU32(data)) |
           (static_cast<uint64_t>(readU32(data + 4)) << 32);
}

size_t findMagic(const std::vector<uint8_t> &data, const uint8_t *magic,
                 size_t from) {
    if (data.size() < 4 || from > data.size() - 4)
        return std::string::npos;

    for (size_t i = from; i + 4 <= data.size(); ++i) {
        if (std::equal(magic, magic + 4,
                       data.begin() + static_cast<std::ptrdiff_t>(i)))
            return i;
    }
    return std::string::npos;
}

void appendFloatArray(std::ostringstream &json,
                      const std::vector<float> &values) {
    json << '[';
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0)
            json << ',';
        // Giới hạn 1 chữ số thập phân cho mảng hàng nghìn phần tử để giảm mạnh kích thước JSON
        json << std::fixed << std::setprecision(1) << values[i];
    }
    json << std::defaultfloat << ']';
}

void appendIntArray(std::ostringstream &json,
                    const std::vector<int16_t> &values) {
    json << '[';
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0)
            json << ',';
        json << values[i];
    }
    json << ']';
}
} // namespace

size_t CSIData::tonesPerChain() const {
    return chains.empty() ? 0 : chains.front().amplitudes.size();
}

std::string CSIData::toJson() const {
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
         << ",\"motion_calibration_stable\":"
         << (motion_calibration_stable ? "true" : "false")
         << ",\"motion_threshold_high\":" << motion_threshold_high
         << ",\"motion_threshold_low\":" << motion_threshold_low
         << ",\"presence_detected\":" << (presence_detected ? "true" : "false")
         << ",\"breath_bpm\":" << breath_bpm
         << ",\"breath_wave\":" << breath_wave
         << ",\"breath_quality\":" << breath_quality
         << ",\"breath_phase_span\":" << breath_phase_span
         << ",\"breath_asymmetry\":" << breath_asymmetry
         << ",\"breath_rrv\":" << breath_rrv
         << ",\"breath_regularity\":" << breath_regularity
         << ",\"breath_valid\":" << (breath_valid ? "true" : "false")
         << ",\"is_human_presence\":" << (is_human_presence ? "true" : "false")
         << ",\"is_sleeping\":" << (is_sleeping ? "true" : "false")
         << ",\"presence_status\":\"" << presence_status << "\""
         << ",\"wave_detected\":" << (wave_detected ? "true" : "false")
         << ",\"wave_confidence\":" << wave_confidence
         << ",\"wave_status\":\"" << wave_status << "\""
         << ",\"wave_samples_in_window\":" << wave_samples_in_window
         << ",\"wave_raw_prob\":" << wave_raw_prob
         << ",\"wave_reason\":\"" << wave_reason << "\""
         << ",\"rssi_comb_db\":" << static_cast<int>(rssi_comb_db)
         << ",\"estimated_distance_m\":" << estimated_distance_m
         << ",\"distance_confidence\":" << distance_confidence
         << ",\"distance_zone\":\"" << distance_zone << "\""
         << ",\"distance_trend\":\"" << distance_trend << "\""
         << ",\"csi_los_ratio\":" << csi_los_ratio
         << ",\"csi_rician_k\":" << csi_rician_k
         << ",\"ap_mac\":\"" << ap_mac << "\",\"amplitude_gain_ratio\":[";

    for (size_t i = 0; i < 5; ++i) {
        if (i != 0)
            json << ',';
        json << amplitude_gain_ratio[i];
    }

    json << "],\"chains\":[";
    for (size_t i = 0; i < chains.size(); ++i) {
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

bool CSIParser::feed(const uint8_t *data, size_t size,
                     std::vector<CSIData> &frames) {
    frames.clear();
    if (data == nullptr && size != 0)
        return false;
    if (size > kMaximumPendingSize ||
        pending_.size() > kMaximumPendingSize - size) {
        reset();
        return false;
    }
    if (size != 0)
        pending_.insert(pending_.end(), data, data + size);

    size_t read_offset = 0;
    while (true) {
        const size_t relay_start = findMagic(pending_, kRelayMagic, read_offset);
        if (relay_start == std::string::npos) {
            read_offset = (pending_.size() > 3) ? (pending_.size() - 3) : 0;
            break;
        }
        read_offset = relay_start;
        if (pending_.size() - read_offset < kRelayHeaderSize)
            break;

        const uint32_t metadata_size = readU32(pending_.data() + read_offset + 10);
        if (metadata_size > kMaximumMetadataSize) {
            read_offset += 1;
            continue;
        }
        const size_t cfr_offset = kRelayHeaderSize + metadata_size;
        if (pending_.size() - read_offset < cfr_offset + 12)
            break;
        if (!std::equal(kCfrMagic, kCfrMagic + 4,
                        pending_.data() + read_offset + cfr_offset)) {
            read_offset += 1;
            continue;
        }

        const uint8_t *cfr_header = pending_.data() + read_offset + cfr_offset;
        const size_t cfr_header_size = static_cast<size_t>(cfr_header[7]) * 4;
        const uint32_t payload_size = readU32(cfr_header + 8);
        if (cfr_header_size < kMinimumCfrHeaderSize ||
            cfr_header_size > kMaximumCfrHeaderSize || payload_size == 0 ||
            payload_size > kMaximumPayloadSize) {
            read_offset += 1;
            continue;
        }

        const size_t record_size =
            cfr_offset + cfr_header_size + payload_size + 4;
        if (record_size > kMaximumPendingSize) {
            read_offset += 1;
            continue;
        }
        if (pending_.size() - read_offset < record_size)
            break;
        if (!std::equal(kRelayEndMagic, kRelayEndMagic + 4,
                        pending_.data() + read_offset + record_size - 4)) {
            read_offset += 1;
            continue;
        }

        CSIData frame;
        if (decodeRecord(pending_.data() + read_offset, record_size, frame))
            frames.push_back(std::move(frame));

        read_offset += record_size;
    }

    if (read_offset > 0) {
        if (read_offset >= pending_.size()) {
            pending_.clear();
        } else {
            pending_.erase(pending_.begin(),
                           pending_.begin() +
                               static_cast<std::ptrdiff_t>(read_offset));
        }
    }
    return true;
}

void CSIParser::reset() {
    pending_.clear();
    last_valid_rssi_db_ = 0;
}

bool CSIParser::decodeRecord(const uint8_t *record, size_t size,
                             CSIData &frame) {
    if (size < kRelayHeaderSize + kMinimumCfrHeaderSize + 4)
        return false;

    const uint32_t metadata_size = readU32(record + 10);
    const size_t cfr_offset = kRelayHeaderSize + metadata_size;
    if (metadata_size > kMaximumMetadataSize || cfr_offset > size ||
        size - cfr_offset < kMinimumCfrHeaderSize + 4)
        return false;

    const uint8_t *cfr_header = record + cfr_offset;
    const size_t cfr_header_size = static_cast<size_t>(cfr_header[7]) * 4;
    const uint32_t payload_size = readU32(cfr_header + 8);
    const uint8_t chain_count = cfr_header[18];
    if (cfr_header_size < kMinimumCfrHeaderSize ||
        cfr_header_size > size - cfr_offset - 4 ||
        payload_size > size - cfr_offset - cfr_header_size - 4 ||
        chain_count == 0 || chain_count > 8 || payload_size == 0 ||
        payload_size % (4 * chain_count) != 0)
        return false;

    frame.timestamp_ns = readU64(record + 14);
    frame.chip_tsf_us = readU64(cfr_header + 82);
    frame.phy_ppdu_id = readU16(cfr_header + 16);
    frame.num_chains = chain_count;
    frame.nss = cfr_header[19];
    frame.channel_bw = cfr_header[20];
    frame.packet_bw = cfr_header[21];

    if (metadata_size >= 70) {
        const uint8_t *meta = record + kRelayHeaderSize;
        const uint32_t cr7 = readU32(meta + 42);
        if ((cr7 >> 24) == 0xC0) {
            const uint8_t raw_rssi = cr7 & 0xFF;
            if (raw_rssi > 0) {
                // Qualcomm ath12k: raw_rssi là SNR (dB so với sàn nhiễu noise floor -95 dBm)
                // Càng gần router thì tín hiệu càng mạnh (raw_rssi càng cao: ví dụ 65 ở 0.3m, 51 ở 1.5m, 38 ở 3m)
                // RSSI thực tế (dBm) = raw_rssi - 95 dBm
                frame.rssi_comb_db = static_cast<int8_t>(static_cast<int>(raw_rssi) - 95);
                last_valid_rssi_db_ = frame.rssi_comb_db;
            } else if (last_valid_rssi_db_ < 0) {
                frame.rssi_comb_db = last_valid_rssi_db_;
            }
        } else if (last_valid_rssi_db_ < 0) {
            // Giữ mức RSSI hợp lệ gần nhất khi driver chưa kịp ghép WMI RxStatus (tránh rơi về 0)
            frame.rssi_comb_db = last_valid_rssi_db_;
        }
        frame.agc_indices[0] = meta[66];
        frame.agc_indices[1] = meta[67];
        frame.agc_indices[2] = meta[68];
    } else if (last_valid_rssi_db_ < 0) {
        frame.rssi_comb_db = last_valid_rssi_db_;
    }

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

    for (size_t chain_idx = 0; chain_idx < frame.num_chains; ++chain_idx) {
        auto &chain = frame.chains[chain_idx];
        chain.i.resize(raw_tones);
        chain.q.resize(raw_tones);
        chain.amplitudes.resize(raw_tones);
        chain.phases.resize(raw_tones);

        for (size_t tone = 0; tone < raw_tones; ++tone) {
            const size_t byte_idx = (chain_idx * raw_tones + tone) * 4;
            const int16_t ival =
                static_cast<int16_t>(readU16(payload + byte_idx));
            const int16_t qval =
                static_cast<int16_t>(readU16(payload + byte_idx + 2));

            chain.i[tone] = ival;
            chain.q[tone] = qval;
            chain.amplitudes[tone] =
                std::sqrt(static_cast<float>(ival * ival + qval * qval));
            chain.phases[tone] =
                std::atan2(static_cast<float>(qval), static_cast<float>(ival));
        }
    }
    return true;
}