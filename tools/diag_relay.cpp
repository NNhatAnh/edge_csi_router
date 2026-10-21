#include "csi_parser.hpp"
#include "wave_detector.hpp"
#include <iostream>
#include <fstream>
#include <vector>

void testRelayFile(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open " << path << std::endl;
        return;
    }

    CSIParser parser;
    WaveDetector detector;
    std::vector<uint8_t> buffer(16384);
    std::vector<CSIData> frames;

    long total_raw_frames = 0;
    long wave_active_count = 0;
    long unavailable_count = 0;
    float max_confidence = 0.0f;
    std::string last_reason = "none";

    while (file) {
        file.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
        size_t count = file.gcount();
        if (count == 0) break;

        if (parser.feed(buffer.data(), count, frames)) {
            for (auto &f : frames) {
                total_raw_frames++;
                detector.update(f);
                if (f.wave_confidence > max_confidence) {
                    max_confidence = f.wave_confidence;
                }
                if (f.wave_status == "unavailable") {
                    unavailable_count++;
                }
                if (f.wave_detected) {
                    wave_active_count++;
                }
                last_reason = f.wave_reason;
            }
        }
    }

    std::cout << "[Diagnostic VNPT Sensing on " << path << "]" << std::endl;
    std::cout << "  Total parsed frames: " << total_raw_frames << std::endl;
    std::cout << "  Unavailable count:   " << unavailable_count << std::endl;
    std::cout << "  Wave detected count: " << wave_active_count << std::endl;
    std::cout << "  Max confidence:      " << max_confidence << std::endl;
    std::cout << "  Final status:        " << detector.getStatus() << std::endl;
    std::cout << "  Final reason:        " << last_reason << std::endl;
}

int main(int argc, char **argv) {
    std::string path = "/home/nanh/Downloads/VNPT_WIFI_SENSING/data/raw/20261020-103850_11-1_wave_S1_P1/relay.bin";
    if (argc > 1) path = argv[1];
    testRelayFile(path);
    return 0;
}
