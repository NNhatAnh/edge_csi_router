#include "csi_parser.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void appendU16(std::vector<uint8_t> &bytes, size_t offset, uint16_t value)
{
    bytes[offset] = static_cast<uint8_t>(value & 0xff);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void appendU32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
{
    for (size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xff);
}

void appendU64(std::vector<uint8_t> &bytes, size_t offset, uint64_t value)
{
    for (size_t i = 0; i < 8; ++i)
        bytes[offset + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xff);
}

std::vector<uint8_t> makeRecord(uint16_t ppdu, uint64_t timestamp_ns, const std::array<int16_t, 4> &values)
{
    constexpr size_t relay_header_size = 22;
    constexpr size_t cfr_header_size = 120;
    constexpr size_t payload_size = 16;
    const size_t cfr_offset = relay_header_size;
    std::vector<uint8_t> record(relay_header_size + cfr_header_size + payload_size + 4, 0);

    record[0] = 0xaf;
    record[1] = 0xbe;
    record[2] = 0xad;
    record[3] = 0xde;
    appendU32(record, 10, 0);
    appendU64(record, 14, timestamp_ns);

    const size_t header = cfr_offset;
    record[header] = 0xba;
    record[header + 1] = 0x00;
    record[header + 2] = 0xde;
    record[header + 3] = 0xc0;
    record[header + 4] = 3;
    record[header + 5] = 1;
    record[header + 6] = 65;
    record[header + 7] = static_cast<uint8_t>(cfr_header_size / 4);
    appendU32(record, header + 8, payload_size);
    appendU16(record, header + 16, ppdu);
    record[header + 18] = 2;
    record[header + 19] = 1;
    record[header + 20] = 2;
    for (size_t chain = 0; chain < 5; ++chain)
        record[header + 44 + chain] = static_cast<uint8_t>(128 - chain);
    const uint8_t ap_mac[] = {0x06, 0x03, 0x7f, 0x73, 0x71, 0xb8};
    for (size_t i = 0; i < sizeof(ap_mac); ++i)
        record[header + 76 + i] = ap_mac[i];
    appendU64(record, header + 82, 17404875522ULL);

    const size_t payload = header + cfr_header_size;
    for (size_t sample = 0; sample < 4; ++sample)
    {
        appendU16(record, payload + sample * 4, static_cast<uint16_t>(values[sample]));
        appendU16(record, payload + sample * 4 + 2, 0);
    }

    const size_t end = record.size() - 4;
    record[end] = 0xad;
    record[end + 1] = 0xde;
    record[end + 2] = 0xaf;
    record[end + 3] = 0xbe;
    return record;
}
}

int main(int argc, char **argv)
{
    CSIParser parser;
    std::vector<CSIData> decoded;
    std::vector<CSIData> batch;
    const std::vector<uint8_t> first = makeRecord(0x1234, 1000000000ULL, {3, 4, 5, 12});
    const std::vector<uint8_t> second = makeRecord(0x1235, 1100000000ULL, {4, 4, 5, 12});
    std::vector<uint8_t> stream = first;
    stream.insert(stream.end(), second.begin(), second.end());

    for (size_t offset = 0; offset < stream.size(); offset += 7)
    {
        const size_t count = std::min<size_t>(7, stream.size() - offset);
        assert(parser.feed(stream.data() + offset, count, batch));
        decoded.insert(decoded.end(), batch.begin(), batch.end());
    }

    assert(decoded.size() == 2);
    MotionDetector decoded_motion;
    for (CSIData &frame : decoded)
        decoded_motion.update(frame);
    assert(decoded[0].phy_ppdu_id == 0x1234);
    assert(decoded[0].timestamp_ns == 1000000000ULL);
    assert(decoded[0].chip_tsf_us == 17404875522ULL);
    assert(decoded[0].num_chains == 2);
    assert(decoded[0].tonesPerChain() == 2);
    assert(decoded[0].chains[0].i[0] == 3);
    assert(decoded[0].chains[0].q[1] == 0);
    assert(std::abs(decoded[0].chains[0].amplitudes[1] - 4.0f) < 0.001f);
    assert(std::abs(decoded[0].chains[1].amplitudes[1] - 12.0f) < 0.001f);
    assert(decoded[0].ap_mac == "06:03:7f:73:71:b8");
    assert(decoded[0].amplitude_gain_ratio[1] == 127);
    assert(decoded[0].motion_score == 0.0f);
    assert(!decoded[0].motion_ready);
    assert(!decoded[1].motion_calibrated);
    assert(decoded[1].toJson().find("\"motion_ready\":false") != std::string::npos);
    assert(decoded[1].toJson().find("\"chains\":[") != std::string::npos);

    CSIParser sensing_parser;
    MotionDetector motion_detector;
    auto processRecord = [&](const std::vector<uint8_t> &record) {
        if (!sensing_parser.feed(record.data(), record.size(), batch))
            return false;
        for (CSIData &frame : batch)
            motion_detector.update(frame);
        return true;
    };
    const std::array<int16_t, 4> quiet_amplitudes = {1, 4, 9, 16};
    const std::array<int16_t, 4> changed_amplitudes = {16, 9, 4, 1};
    for (uint16_t frame = 0; frame < 81; ++frame)
    {
        const std::vector<uint8_t> record =
            makeRecord(frame, 2000000000ULL + static_cast<uint64_t>(frame) * 100000000ULL, quiet_amplitudes);
        assert(processRecord(record));
    }
    assert(batch.size() == 1);
    assert(batch.back().motion_ready);
    assert(batch.back().motion_calibrated);
    assert(batch.back().motion_calibration_frames == 50);
    assert(batch.back().motion_calibration_stable);
    assert(!batch.back().presence_detected);
    assert(std::abs(batch.back().motion_threshold_high - 0.005f) < 0.0001f);
    assert(std::abs(batch.back().motion_threshold_low - 0.003f) < 0.0001f);

    const std::vector<uint8_t> scaled_quiet_record =
        makeRecord(81, 10000000000ULL, {2, 8, 18, 32});
    assert(processRecord(scaled_quiet_record));
    assert(batch.back().motion_score < batch.back().motion_threshold_low);
    assert(!batch.back().presence_detected);

    for (uint16_t frame = 0; frame < 60; ++frame)
    {
        const auto &amplitudes = frame % 2 == 0 ? changed_amplitudes : quiet_amplitudes;
        const std::vector<uint8_t> record =
            makeRecord(static_cast<uint16_t>(82 + frame), 10100000000ULL + static_cast<uint64_t>(frame) * 100000000ULL, amplitudes);
        assert(processRecord(record));
    }
    assert(batch.back().motion_score > batch.back().motion_threshold_high);
    assert(batch.back().presence_detected);
    assert(batch.back().toJson().find("\"presence_detected\":true") != std::string::npos);

    for (uint16_t frame = 0; frame < 50; ++frame)
    {
        const std::vector<uint8_t> record =
            makeRecord(static_cast<uint16_t>(142 + frame), 16100000000ULL + static_cast<uint64_t>(frame) * 100000000ULL, changed_amplitudes);
        assert(processRecord(record));
    }
    assert(batch.back().motion_score < batch.back().motion_threshold_low);
    assert(!batch.back().presence_detected);

    CSIParser unstable_parser;
    MotionDetector unstable_detector;
    for (uint16_t frame = 0; frame < 100; ++frame)
    {
        const auto &amplitudes = frame % 2 == 0 ? changed_amplitudes : quiet_amplitudes;
        const std::vector<uint8_t> record =
            makeRecord(frame, 30000000000ULL + static_cast<uint64_t>(frame) * 100000000ULL, amplitudes);
        assert(unstable_parser.feed(record.data(), record.size(), batch));
        for (CSIData &decoded_frame : batch)
            unstable_detector.update(decoded_frame);
    }
    assert(!batch.back().motion_calibrated);
    assert(batch.back().motion_calibration_frames == 0);

    motion_detector.reset();
    const std::vector<uint8_t> recalibration_record =
        makeRecord(187, 21000000000ULL, quiet_amplitudes);
    assert(processRecord(recalibration_record));
    assert(!batch.back().motion_ready);
    assert(!batch.back().motion_calibrated);
    assert(batch.back().motion_window_samples == 0);

    if (argc > 1)
    {
        std::ifstream sample(argv[1], std::ios::binary);
        assert(sample && "Could not open relay sample");
        CSIParser sample_parser;
        std::vector<CSIData> sample_frames;
        std::vector<CSIData> sample_batch;
        uint8_t buffer[4096];
        while (sample)
        {
            sample.read(reinterpret_cast<char *>(buffer), sizeof(buffer));
            const std::streamsize count = sample.gcount();
            if (count > 0)
            {
                assert(sample_parser.feed(buffer, static_cast<size_t>(count), sample_batch));
                sample_frames.insert(sample_frames.end(), sample_batch.begin(), sample_batch.end());
            }
        }
        assert(!sample_frames.empty() && "No CSI records decoded from relay sample");
        std::cerr << "Sample first frame: chains=" << static_cast<unsigned int>(sample_frames.front().num_chains)
                  << ", tones=" << sample_frames.front().tonesPerChain()
                  << ", records=" << sample_frames.size() << '\n';
        assert(sample_frames.front().num_chains == 2);
        assert(sample_frames.front().tonesPerChain() == 54);
        assert(sample_frames.front().chip_tsf_us != 0);
        std::cout << "Decoded " << sample_frames.size() << " sample relay records\n";
    }
}
