#include "cfr_controller.hpp"
#include "csi_parser.hpp"
#include "prober.hpp"
#include "web_server.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>


namespace {
volatile std::sig_atomic_t stop_requested = 0;

void handleSignal(int) { stop_requested = 1; }

struct Options {
    int web_port = 8080;
    int ingest_port = 10000;
    std::string web_root = "web";
    std::string watch_directory = "/tmp";
    std::string filename_prefix = "cfr_dump_phy00_";
    std::string replay_file;
    std::string probe_ip;
    int probe_interval = 50;
    bool probe_enabled = false;
    std::string probe_iface = "br-lan";
};

void printUsage(const char *program) {
    std::cout
        << "Usage: " << program << " [options]\n"
        << "  --web-port PORT       HTTP and SSE port (default: 8080)\n"
        << "  --ingest-port PORT    TCP relay ingest port (default: 10000)\n"
        << "  --web-root PATH       Directory containing index.html, style.css "
           "and script.js\n"
        << "  --watch-dir PATH      Follow cfr_test_app relay files (default: "
           "/tmp)\n"
        << "  --prefix PREFIX       Relay filename prefix (default: "
           "cfr_dump_phy00_)\n"
        << "  --replay FILE         Replay a captured relay file for testing\n"
        << "  --probe-ip IP         Target client IP for active probing (keep-alive)\n"
        << "  --probe-interval MS   Interval in ms between active probes (default: 50)\n"
        << "  --probe-iface IFACE   Network interface for ARP probing (default: br-lan)\n"
        << "  --probe-enable        Enable active probing on startup\n";
}

bool parseOptions(int argc, char **argv, Options &options) {
    bool watch_directory_explicit = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            printUsage(argv[0]);
            return false;
        }
        if (argument == "--probe-enable") {
            options.probe_enabled = true;
            continue;
        }
        if (i + 1 >= argc) {
            std::cerr << "Missing value for " << argument << '\n';
            return false;
        }

        const std::string value = argv[++i];
        try {
            if (argument == "--web-port")
                options.web_port = std::stoi(value);
            else if (argument == "--ingest-port")
                options.ingest_port = std::stoi(value);
            else if (argument == "--web-root")
                options.web_root = value;
            else if (argument == "--watch-dir") {
                options.watch_directory = value;
                watch_directory_explicit = true;
            } else if (argument == "--prefix")
                options.filename_prefix = value;
            else if (argument == "--replay")
                options.replay_file = value;
            else if (argument == "--probe-ip") {
                options.probe_ip = value;
                options.probe_enabled = true;
            } else if (argument == "--probe-interval")
                options.probe_interval = std::stoi(value);
            else if (argument == "--probe-iface")
                options.probe_iface = value;
            else {
                std::cerr << "Unknown option: " << argument << '\n';
                return false;
            }
        } catch (const std::exception &error) {
            std::cerr << "Invalid value for " << argument << ": "
                      << error.what() << '\n';
            return false;
        }
    }

    if (options.web_port < 1 || options.web_port > 65535 ||
        options.ingest_port < 1 || options.ingest_port > 65535 ||
        options.web_port == options.ingest_port) {
        std::cerr << "Ports must be in 1..65535 and must be different\n";
        return false;
    }
    if (!options.replay_file.empty() && !watch_directory_explicit)
        options.watch_directory.clear();
    if (!options.watch_directory.empty() && !options.replay_file.empty()) {
        std::cerr << "--watch-dir and --replay cannot be used together\n";
        return false;
    }
    return true;
}

void publishBytes(CSIParser &parser, WebServer &server, const uint8_t *bytes,
                  size_t size) {
    std::vector<CSIData> frames;
    if (!parser.feed(bytes, size, frames)) {
        std::cerr << "[Relay] Invalid or oversized relay stream\n";
        return;
    }
    for (const CSIData &frame : frames)
        server.publish(frame);
}

bool readWriterPosition(const std::filesystem::path &fdinfo_path,
                        uintmax_t &position) {
    std::ifstream fdinfo(fdinfo_path);
    std::string line;
    while (std::getline(fdinfo, line)) {
        if (line.compare(0, 4, "pos:") != 0)
            continue;
        try {
            position = static_cast<uintmax_t>(std::stoull(line.substr(4)));
            return true;
        } catch (const std::exception &) {
            return false;
        }
    }
    return false;
}

std::filesystem::path findRelayWriter(const std::filesystem::path &relay_file) {
    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path target = fs::absolute(relay_file, error).lexically_normal();
    if (error)
        return {};

    fs::directory_iterator processes(
        "/proc", fs::directory_options::skip_permission_denied, error);
    if (error)
        return {};

    for (const fs::directory_entry &process : processes) {
        const std::string pid = process.path().filename().string();
        if (pid.empty() ||
            !std::all_of(pid.begin(), pid.end(),
                         [](unsigned char c) { return std::isdigit(c) != 0; }))
            continue;

        std::ifstream command_line(process.path() / "cmdline",
                                   std::ios::binary);
        if (!command_line)
            continue;
        const std::string command(
            (std::istreambuf_iterator<char>(command_line)),
            std::istreambuf_iterator<char>());
        if (command.find("cfr_test_app") == std::string::npos)
            continue;

        fs::directory_iterator descriptors(
            process.path() / "fd",
            fs::directory_options::skip_permission_denied, error);
        if (error) {
            error.clear();
            continue;
        }
        for (const fs::directory_entry &descriptor : descriptors) {
            const fs::path link = fs::read_symlink(descriptor.path(), error);
            if (error) {
                error.clear();
                continue;
            }
            if (link.lexically_normal() != target)
                continue;

            const fs::path fdinfo =
                process.path() / "fdinfo" / descriptor.path().filename();
            uintmax_t position = 0;
            if (readWriterPosition(fdinfo, position))
                return fdinfo;
        }
    }
    return {};
}

bool replayFile(const std::string &path, WebServer &server) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[Replay] Cannot open " << path << '\n';
        return false;
    }

    CSIParser parser;
    uint8_t buffer[16384];
    std::vector<CSIData> frames;
    uint64_t previous_timestamp = 0;
    while (file) {
        file.read(reinterpret_cast<char *>(buffer), sizeof(buffer));
        const std::streamsize count = file.gcount();
        if (count <= 0)
            break;
        if (!parser.feed(buffer, static_cast<size_t>(count), frames)) {
            std::cerr << "[Replay] Invalid or oversized relay stream in "
                      << path << '\n';
            return false;
        }
        for (size_t i = 0; i < frames.size(); ++i) {
            if (previous_timestamp != 0) {
                const uint64_t current = frames[i].timestamp_ns;
                if (current > previous_timestamp) {
                    const auto delay =
                        std::chrono::nanoseconds(std::min<uint64_t>(
                            current - previous_timestamp, 500000000ULL));
                    std::this_thread::sleep_for(delay);
                }
            }
            previous_timestamp = frames[i].timestamp_ns;
            server.publish(frames[i]);
        }
    }
    return true;
}

void followRelayDirectory(const Options &options, WebServer &server, CFRController &cfr_controller) {
    namespace fs = std::filesystem;
    fs::path active_file;
    fs::path writer_fdinfo;
    uintmax_t offset = 0;
    CSIParser parser;
    std::vector<uint8_t> buffer(65536);
    auto last_writer_search = std::chrono::steady_clock::now();
    auto last_dir_scan = std::chrono::steady_clock::time_point{};
    int fdinfo_fail_count = 0;
    std::cout << "[Relay] Watching " << options.watch_directory
              << " for files beginning with " << options.filename_prefix
              << '\n';

    while (!stop_requested) {
        // Chỉ đọc và phát tán dữ liệu khi CFRController đang thu (hoặc đang replay file)
        if (!cfr_controller.isRunning() && options.replay_file.empty()) {
            if (!active_file.empty()) {
                active_file.clear();
                writer_fdinfo.clear();
                offset = 0;
                parser.reset();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        if (active_file.empty() || (now - last_dir_scan > std::chrono::seconds(2))) {
            last_dir_scan = now;
            fs::path newest_file;
            fs::file_time_type newest_time{};
            std::error_code error;
            fs::directory_iterator iterator(
                options.watch_directory,
                fs::directory_options::skip_permission_denied, error);
            if (error) {
                std::cerr << "[Relay] Cannot read directory "
                          << options.watch_directory << ": " << error.message()
                          << '\n';
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            for (const fs::directory_entry &entry : iterator) {
                if (!entry.is_regular_file(error) || error) {
                    error.clear();
                    continue;
                }
                const std::string filename = entry.path().filename().string();
                if (filename.compare(0, options.filename_prefix.size(),
                                     options.filename_prefix) != 0 ||
                    entry.path().extension() != ".bin")
                    continue;
                const fs::file_time_type modified = entry.last_write_time(error);
                if (error) {
                    error.clear();
                    continue;
                }
                if (newest_file.empty() || modified > newest_time) {
                    newest_file = entry.path();
                    newest_time = modified;
                }
            }

            if (!newest_file.empty() && newest_file != active_file) {
                active_file = newest_file;
                offset = 0;
                writer_fdinfo = findRelayWriter(active_file);
                parser.reset();
                uintmax_t writer_position = 0;
                if (!writer_fdinfo.empty() &&
                    readWriterPosition(writer_fdinfo, writer_position)) {
                    // Khởi đầu stream trực tiếp gần vị trí hiện tại của writer để tránh dồn ứ hàng MB dữ liệu cũ
                    offset = (writer_position > 32768) ? (writer_position - 32768) : 0;
                    std::cerr << "[Relay] Following live cfr_test_app ring "
                              << active_file.string()
                              << " from offset " << offset
                              << " through writer pos " << writer_position << '\n';
                } else {
                    writer_fdinfo.clear();
                    // Nếu là file cũ tồn đọng khi daemon khởi động và chưa có writer nào ghi:
                    // Đặt offset bằng kích thước file để KHÔNG tự động replay dữ liệu cũ vào luồng live!
                    std::error_code sz_err;
                    offset = fs::file_size(active_file, sz_err);
                    std::cerr << "[Relay] Waiting for live cfr_test_app writer on "
                              << active_file.string() << '\n';
                }
            }
        }

        if (!active_file.empty()) {
            const auto now = std::chrono::steady_clock::now();
            if (writer_fdinfo.empty() && (now - last_writer_search > std::chrono::seconds(1))) {
                last_writer_search = now;
                writer_fdinfo = findRelayWriter(active_file);
                if (!writer_fdinfo.empty()) {
                    uintmax_t pos = 0;
                    if (readWriterPosition(writer_fdinfo, pos)) {
                        offset = (pos > 65536) ? (pos - 65536) : 0;
                        parser.reset();
                    }
                    std::cerr << "[Relay] Re-attached to live cfr_test_app writer\n";
                }
            }

            std::error_code error;
            const uintmax_t file_size = fs::file_size(active_file, error);
            if (!error) {
                uintmax_t available_size = file_size;
                if (!writer_fdinfo.empty()) {
                    uintmax_t current_pos = 0;
                    if (!readWriterPosition(writer_fdinfo, current_pos)) {
                        if (++fdinfo_fail_count >= 10) {
                            std::cerr << "[Relay] Live writer closed; waiting for "
                                         "file appends\n";
                            writer_fdinfo.clear();
                            offset = file_size;
                            parser.reset();
                            fdinfo_fail_count = 0;
                        }
                    } else {
                        fdinfo_fail_count = 0;
                        available_size = std::min(current_pos, file_size);
                    }
                }

                auto readRange = [&](uintmax_t begin, uintmax_t end) {
                    if (end <= begin)
                        return;
                    std::ifstream file(active_file, std::ios::binary);
                    if (!file) {
                        std::cerr << "[Relay] Cannot read "
                                  << active_file.string() << '\n';
                        return;
                    }
                    file.seekg(static_cast<std::streamoff>(begin));
                    uintmax_t remaining = end - begin;
                    while (remaining > 0 && !stop_requested) {
                        const size_t requested = static_cast<size_t>(
                            std::min<uintmax_t>(buffer.size(), remaining));
                        file.read(reinterpret_cast<char *>(buffer.data()),
                                  static_cast<std::streamsize>(requested));
                        const std::streamsize count = file.gcount();
                        if (count <= 0)
                            break;
                        publishBytes(parser, server, buffer.data(),
                                     static_cast<size_t>(count));
                        remaining -= static_cast<uintmax_t>(count);
                    }
                };

                if (writer_fdinfo.empty()) {
                    offset = file_size;
                    parser.reset();
                } else {
                    constexpr uintmax_t kMaxBacklog = 65536;
                    uintmax_t backlog = 0;
                    if (available_size >= offset) {
                        backlog = available_size - offset;
                    } else {
                        backlog = (file_size > offset ? file_size - offset : 0) + available_size;
                    }

                    if (backlog > kMaxBacklog) {
                        offset = (available_size > 32768) ? (available_size - 32768) : 0;
                        parser.reset();
                    }

                    if (available_size < offset) {
                        readRange(offset, file_size);
                        offset = 0;
                        readRange(offset, available_size);
                        offset = available_size;
                    } else if (available_size > offset) {
                        readRange(offset, available_size);
                        offset = available_size;
                    }
                }
            } else {
                error.clear();
                active_file.clear();
                offset = 0;
                parser.reset();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
} // namespace

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    Options options;
    if (!parseOptions(argc, argv, options))
        return 2;

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    ActiveProber prober(options.probe_iface);
    if (options.probe_enabled || !options.probe_ip.empty()) {
        std::vector<std::string> targets;
        if (!options.probe_ip.empty())
            targets.push_back(options.probe_ip);
        prober.setConfig(true, "single", targets, options.probe_interval);
    }
    prober.start();

    CFRController cfr_controller;
    WebServer server(options.web_port, options.ingest_port, options.web_root,
                     &prober, &cfr_controller);
    if (!server.start()) {
        cfr_controller.stopCapture();
        prober.stop();
        return 1;
    }

    if (!options.replay_file.empty() &&
        !replayFile(options.replay_file, server))
        return 1;

    std::thread watcher;
    if (!options.watch_directory.empty()) {
        watcher = std::thread(followRelayDirectory, std::cref(options),
                              std::ref(server), std::ref(cfr_controller));
    }

    auto last_target_check = std::chrono::steady_clock::now();
    while (!stop_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Kiểm tra định kỳ kết nối thiết bị mục tiêu nếu đang thu CFR
        const auto now = std::chrono::steady_clock::now();
        if (now - last_target_check >= std::chrono::milliseconds(1000)) {
            last_target_check = now;
            if (cfr_controller.isRunning()) {
                CFRCaptureStatus cstat = cfr_controller.getStatus();
                if (!cstat.running) {
                    server.resetMotionCalibration();
                    if (options.probe_enabled) {
                        prober.setConfig(false, "single", {}, options.probe_interval);
                    }
                    std::cerr << "[Main] Capture stopped automatically: "
                              << cstat.last_error << '\n';
                }
            }
        }
    }

    cfr_controller.stopCapture();
    server.stop();
    prober.stop();
    if (watcher.joinable())
        watcher.join();
    return 0;
}
