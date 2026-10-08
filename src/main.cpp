#include "csi_parser.hpp"
#include "web_server.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
volatile std::sig_atomic_t stop_requested = 0;

void handleSignal(int)
{
    stop_requested = 1;
}

struct Options
{
    int web_port = 8080;
    int ingest_port = 10000;
    std::string web_root = "web";
    std::string watch_directory = "/tmp";
    std::string filename_prefix = "cfr_dump_phy00_";
    std::string replay_file;
};

void printUsage(const char *program)
{
    std::cout << "Usage: " << program << " [options]\n"
              << "  --web-port PORT       HTTP and SSE port (default: 8080)\n"
              << "  --ingest-port PORT    TCP relay ingest port (default: 10000)\n"
              << "  --web-root PATH       Directory containing index.html, style.css and script.js\n"
              << "  --watch-dir PATH      Follow cfr_test_app relay files (default: /tmp)\n"
              << "  --prefix PREFIX       Relay filename prefix (default: cfr_dump_phy00_)\n"
              << "  --replay FILE         Replay a captured relay file for testing\n";
}

bool parseOptions(int argc, char **argv, Options &options)
{
    bool watch_directory_explicit = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h")
        {
            printUsage(argv[0]);
            return false;
        }
        if (i + 1 >= argc)
        {
            std::cerr << "Missing value for " << argument << '\n';
            return false;
        }

        const std::string value = argv[++i];
        try
        {
            if (argument == "--web-port")
                options.web_port = std::stoi(value);
            else if (argument == "--ingest-port")
                options.ingest_port = std::stoi(value);
            else if (argument == "--web-root")
                options.web_root = value;
            else if (argument == "--watch-dir")
            {
                options.watch_directory = value;
                watch_directory_explicit = true;
            }
            else if (argument == "--prefix")
                options.filename_prefix = value;
            else if (argument == "--replay")
                options.replay_file = value;
            else
            {
                std::cerr << "Unknown option: " << argument << '\n';
                return false;
            }
        }
        catch (const std::exception &error)
        {
            std::cerr << "Invalid value for " << argument << ": " << error.what() << '\n';
            return false;
        }
    }

    if (options.web_port < 1 || options.web_port > 65535 ||
        options.ingest_port < 1 || options.ingest_port > 65535 ||
        options.web_port == options.ingest_port)
    {
        std::cerr << "Ports must be in 1..65535 and must be different\n";
        return false;
    }
    if (!options.replay_file.empty() && !watch_directory_explicit)
        options.watch_directory.clear();
    if (!options.watch_directory.empty() && !options.replay_file.empty())
    {
        std::cerr << "--watch-dir and --replay cannot be used together\n";
        return false;
    }
    return true;
}

void publishBytes(CSIParser &parser, WebServer &server, const uint8_t *bytes, size_t size)
{
    std::vector<CSIData> frames;
    if (!parser.feed(bytes, size, frames))
    {
        std::cerr << "[Relay] Invalid or oversized relay stream\n";
        return;
    }
    for (const CSIData &frame : frames)
        server.publish(frame);
}

bool readWriterPosition(const std::filesystem::path &fdinfo_path, uintmax_t &position)
{
    std::ifstream fdinfo(fdinfo_path);
    std::string line;
    while (std::getline(fdinfo, line))
    {
        if (line.compare(0, 4, "pos:") != 0)
            continue;
        try
        {
            position = static_cast<uintmax_t>(std::stoull(line.substr(4)));
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }
    return false;
}

std::filesystem::path findRelayWriter(const std::filesystem::path &relay_file)
{
    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path target = fs::absolute(relay_file, error).lexically_normal();
    if (error)
        return {};

    fs::directory_iterator processes("/proc", fs::directory_options::skip_permission_denied, error);
    if (error)
        return {};

    for (const fs::directory_entry &process : processes)
    {
        const std::string pid = process.path().filename().string();
        if (pid.empty() || !std::all_of(pid.begin(), pid.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
            continue;

        std::ifstream command_line(process.path() / "cmdline", std::ios::binary);
        if (!command_line)
            continue;
        const std::string command((std::istreambuf_iterator<char>(command_line)),
                                  std::istreambuf_iterator<char>());
        if (command.find("cfr_test_app") == std::string::npos)
            continue;

        fs::directory_iterator descriptors(process.path() / "fd", fs::directory_options::skip_permission_denied, error);
        if (error)
        {
            error.clear();
            continue;
        }
        for (const fs::directory_entry &descriptor : descriptors)
        {
            const fs::path link = fs::read_symlink(descriptor.path(), error);
            if (error)
            {
                error.clear();
                continue;
            }
            if (link.lexically_normal() != target)
                continue;

            const fs::path fdinfo = process.path() / "fdinfo" / descriptor.path().filename();
            uintmax_t position = 0;
            if (readWriterPosition(fdinfo, position))
                return fdinfo;
        }
    }
    return {};
}

bool replayFile(const std::string &path, WebServer &server)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        std::cerr << "[Replay] Cannot open " << path << '\n';
        return false;
    }

    CSIParser parser;
    uint8_t buffer[16384];
    std::vector<CSIData> frames;
    uint64_t previous_timestamp = 0;
    while (file)
    {
        file.read(reinterpret_cast<char *>(buffer), sizeof(buffer));
        const std::streamsize count = file.gcount();
        if (count <= 0)
            break;
        if (!parser.feed(buffer, static_cast<size_t>(count), frames))
        {
            std::cerr << "[Replay] Invalid or oversized relay stream in " << path << '\n';
            return false;
        }
        for (size_t i = 0; i < frames.size(); ++i)
        {
            if (previous_timestamp != 0)
            {
                const uint64_t current = frames[i].timestamp_ns;
                if (current > previous_timestamp)
                {
                    const auto delay = std::chrono::nanoseconds(
                        std::min<uint64_t>(current - previous_timestamp, 500000000ULL));
                    std::this_thread::sleep_for(delay);
                }
            }
            previous_timestamp = frames[i].timestamp_ns;
            server.publish(frames[i]);
        }
    }
    return true;
}

void followRelayDirectory(const Options &options, WebServer &server)
{
    namespace fs = std::filesystem;
    fs::path active_file;
    fs::path writer_fdinfo;
    uintmax_t offset = 0;
    CSIParser parser;
    std::vector<uint8_t> buffer(65536);
    std::cout << "[Relay] Watching " << options.watch_directory
              << " for files beginning with " << options.filename_prefix << '\n';

    while (!stop_requested)
    {
        fs::path newest_file;
        fs::file_time_type newest_time{};
        std::error_code error;
        fs::directory_iterator iterator(options.watch_directory, fs::directory_options::skip_permission_denied, error);
        if (error)
        {
            std::cerr << "[Relay] Cannot read directory " << options.watch_directory << ": " << error.message() << '\n';
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        for (const fs::directory_entry &entry : iterator)
        {
            if (!entry.is_regular_file(error) || error)
            {
                error.clear();
                continue;
            }
            const std::string filename = entry.path().filename().string();
            if (filename.compare(0, options.filename_prefix.size(), options.filename_prefix) != 0 ||
                entry.path().extension() != ".bin")
                continue;
            const fs::file_time_type modified = entry.last_write_time(error);
            if (error)
            {
                error.clear();
                continue;
            }
            if (newest_file.empty() || modified > newest_time)
            {
                newest_file = entry.path();
                newest_time = modified;
            }
        }

        if (!newest_file.empty() && newest_file != active_file)
        {
            active_file = newest_file;
            offset = 0;
            writer_fdinfo = findRelayWriter(active_file);
            parser.reset();
            uintmax_t writer_position = 0;
            if (!writer_fdinfo.empty() && readWriterPosition(writer_fdinfo, writer_position))
            {
                offset = writer_position;
                std::cerr << "[Relay] Following live cfr_test_app ring " << active_file.string()
                          << " at offset " << offset << '\n';
            }
            else
            {
                writer_fdinfo.clear();
                std::cerr << "[Relay] Following append-only relay file " << active_file.string() << '\n';
            }
        }
        if (!active_file.empty())
        {
            const uintmax_t file_size = fs::file_size(active_file, error);
            if (!error)
            {
                uintmax_t available_size = file_size;
                if (!writer_fdinfo.empty())
                {
                    if (!readWriterPosition(writer_fdinfo, available_size))
                    {
                        std::cerr << "[Relay] Live writer closed; waiting for file appends\n";
                        writer_fdinfo.clear();
                        offset = file_size;
                        parser.reset();
                    }
                    else
                    {
                        available_size = std::min(available_size, file_size);
                    }
                }

                auto readRange = [&](uintmax_t begin, uintmax_t end) {
                    if (end <= begin)
                        return;
                    std::ifstream file(active_file, std::ios::binary);
                    if (!file)
                    {
                        std::cerr << "[Relay] Cannot read " << active_file.string() << '\n';
                        return;
                    }
                    file.seekg(static_cast<std::streamoff>(begin));
                    uintmax_t remaining = end - begin;
                    while (remaining > 0 && !stop_requested)
                    {
                        const size_t requested = static_cast<size_t>(
                            std::min<uintmax_t>(buffer.size(), remaining));
                        file.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(requested));
                        const std::streamsize count = file.gcount();
                        if (count <= 0)
                            break;
                        publishBytes(parser, server, buffer.data(), static_cast<size_t>(count));
                        remaining -= static_cast<uintmax_t>(count);
                    }
                };

                if (writer_fdinfo.empty())
                {
                    if (file_size < offset)
                    {
                        offset = 0;
                        parser.reset();
                    }
                    readRange(offset, file_size);
                    offset = file_size;
                }
                else if (available_size < offset)
                {
                    readRange(offset, file_size);
                    offset = 0;
                    readRange(offset, available_size);
                    offset = available_size;
                }
                else if (available_size > offset)
                {
                    readRange(offset, available_size);
                    offset = available_size;
                }
            }
            else
            {
                error.clear();
                active_file.clear();
                offset = 0;
                parser.reset();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h")
        {
            printUsage(argv[0]);
            return 0;
        }
    }

    Options options;
    if (!parseOptions(argc, argv, options))
        return 2;

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    WebServer server(options.web_port, options.ingest_port, options.web_root);
    if (!server.start())
        return 1;

    if (!options.replay_file.empty() && !replayFile(options.replay_file, server))
        return 1;

    std::thread watcher;
    if (!options.watch_directory.empty())
    {
        watcher = std::thread(followRelayDirectory, std::cref(options), std::ref(server));
    }

    while (!stop_requested)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

    server.stop();
    if (watcher.joinable())
        watcher.join();
    return 0;
}
