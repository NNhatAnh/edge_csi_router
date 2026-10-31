#include "csi_parser.hpp"
#include "motion.hpp"
#include "air_draw.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <bin_file>\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open " << argv[1] << "\n";
        return 1;
    }

    CSIParser parser;
    MotionDetector motion;
    AirDrawTracker tracker;

    std::vector<uint8_t> buffer(32768);
    std::vector<CSIData> frames;

    size_t frame_count = 0;
    size_t active_draw_frames = 0;
    float max_speed = 0.0f;
    float min_x = 1.0f, max_x = 0.0f;
    float min_y = 1.0f, max_y = 0.0f;

    while (file) {
        file.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
        size_t count = file.gcount();
        if (count == 0) break;

        if (parser.feed(buffer.data(), count, frames)) {
            for (auto &frame : frames) {
                if (frame.chains.size() < 2 || frame.tonesPerChain() == 0) continue;
                frame_count++;

                motion.update(frame);
                tracker.update(frame);

                if (frame.draw_active) {
                    active_draw_frames++;
                }

                max_speed = std::max(max_speed, tracker.getSpeed());
                min_x = std::min(min_x, frame.draw_cursor_x);
                max_x = std::max(max_x, frame.draw_cursor_x);
                min_y = std::min(min_y, frame.draw_cursor_y);
                max_y = std::max(max_y, frame.draw_cursor_y);
            }
        }
    }

    std::cout << "Replay Summary for " << argv[1] << ":\n"
              << "  Total frames: " << frame_count << "\n"
              << "  Active draw frames: " << active_draw_frames << " (" 
              << (frame_count > 0 ? (active_draw_frames * 100.0f / frame_count) : 0.0f) << "%)\n"
              << "  Max speed: " << max_speed << " m/s\n"
              << "  Cursor X span: [" << min_x << ", " << max_x << "]\n"
              << "  Cursor Y span: [" << min_y << ", " << max_y << "]\n"
              << "  Final cursor: (" << tracker.getCursorX() << ", " << tracker.getCursorY() << ")\n";

    return 0;
}
