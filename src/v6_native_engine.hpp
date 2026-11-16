#ifndef V6_NATIVE_ENGINE_HPP
#define V6_NATIVE_ENGINE_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "wakeup_detector.hpp"

struct V6ScoreSnapshot {
    bool running = false;
    std::string mac;
    std::string ip;
    std::string model = "M5R";
    std::string state = "READY"; // READY, DOWN, BOTTOM, UP, PASS, FAIL
    double score = 0.0;
    int stroke_progress = 0;      // 0=READY, 1=DOWN, 2=BOTTOM, 3=UP
    double stroke_fraction = 0.0; // 0.0 .. 1.0
    int label = 0;
    std::string reason;
    std::string result_reason;
    std::string error;
    int accepted = 0;
    int rejected = 0;
    uint64_t frame_count = 0;
    uint64_t last_update_ms = 0;
    double threshold_high = 0.80;
    double threshold_low = 0.65;
};

class V6NativeEngine {
  public:
    V6NativeEngine();
    ~V6NativeEngine();

    bool start(const std::string &mac, const std::string &ip,
               std::string *error_msg = nullptr);
    bool stop(const std::string &reason = "");
    bool isRunning() const;

    void setSensitivity(double high_thresh, double low_thresh);
    double getThresholdHigh() const;
    double getThresholdLow() const;

    V6ScoreSnapshot getSnapshot() const;
    std::string getSnapshotJson() const;

  private:
    void readerWorker(int stdout_fd, pid_t inference_pid, pid_t ping_pid);
    void parseJsonLine(const std::string &line);
    static bool writeSysfs(const std::string &path, const std::string &value);
    static std::string findBinaryPath();
    static std::string findModelPath();

    mutable std::mutex mutex_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};

    pid_t inference_pid_{-1};
    pid_t ping_pid_{-1};
    int pipe_fd_{-1};

    std::thread reader_thread_;
    V6ScoreSnapshot snapshot_;
    ScoreSession scores_;

    std::string active_mac_;
    std::string active_ip_;
    std::string ready_file_;

    double motion_threshold_high_{0.80};
    double motion_threshold_low_{0.65};
    int motion_debounce_frames_{2};
    int current_streak_{0};
    int current_motion_state_{0};
};

#endif // V6_NATIVE_ENGINE_HPP
