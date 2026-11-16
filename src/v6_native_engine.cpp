#include "v6_native_engine.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

std::string extractJsonString(const std::string &json, const std::string &key) {
    const std::string pattern = "\"" + key + "\":\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    pos += pattern.length();
    size_t end = json.find('\"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

double extractJsonDouble(const std::string &json, const std::string &key, double def_val = 0.0) {
    const std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return def_val;
    pos += pattern.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    char *endptr = nullptr;
    double val = std::strtod(json.c_str() + pos, &endptr);
    if (endptr == json.c_str() + pos) return def_val;
    return val;
}

[[maybe_unused]] int extractJsonInt(const std::string &json, const std::string &key, int def_val = 0) {
    const std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return def_val;
    pos += pattern.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    char *endptr = nullptr;
    long val = std::strtol(json.c_str() + pos, &endptr, 10);
    if (endptr == json.c_str() + pos) return def_val;
    return static_cast<int>(val);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

uint64_t nowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // namespace

V6NativeEngine::V6NativeEngine() {
    snapshot_.running = false;
    snapshot_.state = "IDLE";
    snapshot_.model = "M5R";
}

V6NativeEngine::~V6NativeEngine() {
    stop("Destructor");
}

bool V6NativeEngine::isRunning() const {
    return is_running_.load();
}

V6ScoreSnapshot V6NativeEngine::getSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

std::string V6NativeEngine::getSnapshotJson() const {
    V6ScoreSnapshot s = getSnapshot();
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{"
       << "\"running\":" << (s.running ? "true" : "false")
       << ",\"mac\":\"" << s.mac << "\""
       << ",\"ip\":\"" << s.ip << "\""
       << ",\"model\":\"" << s.model << "\""
       << ",\"state\":\"" << s.state << "\""
       << ",\"score\":" << s.score
       << ",\"stroke_progress\":" << s.stroke_progress
       << ",\"stroke_fraction\":" << s.stroke_fraction
       << ",\"label\":" << s.label
       << ",\"reason\":\"" << s.reason << "\""
       << ",\"result_reason\":\"" << s.result_reason << "\""
       << ",\"error\":\"" << s.error << "\""
       << ",\"accepted\":" << s.accepted
       << ",\"rejected\":" << s.rejected
       << ",\"frame_count\":" << s.frame_count
       << ",\"last_update_ms\":" << s.last_update_ms
       << ",\"threshold_high\":" << s.threshold_high
       << ",\"threshold_low\":" << s.threshold_low
       << "}";
    return ss.str();
}

void V6NativeEngine::setSensitivity(double high_thresh, double low_thresh) {
    std::lock_guard<std::mutex> lock(mutex_);
    motion_threshold_high_ = std::clamp(high_thresh, 0.50, 0.95);
    motion_threshold_low_ = std::clamp(low_thresh, 0.40, motion_threshold_high_ - 0.05);
    snapshot_.threshold_high = motion_threshold_high_;
    snapshot_.threshold_low = motion_threshold_low_;
    current_streak_ = 0;
    std::cout << "[V6Engine] Sensitivity updated: High=" << motion_threshold_high_
              << ", Low=" << motion_threshold_low_ << '\n';
}

double V6NativeEngine::getThresholdHigh() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return motion_threshold_high_;
}

double V6NativeEngine::getThresholdLow() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return motion_threshold_low_;
}

bool V6NativeEngine::writeSysfs(const std::string &path, const std::string &value) {
    int fd = open(path.c_str(), O_WRONLY);
    if (fd < 0) return false;
    ssize_t written = write(fd, value.c_str(), value.length());
    close(fd);
    return written == static_cast<ssize_t>(value.length());
}

std::string V6NativeEngine::findBinaryPath() {
    const std::vector<std::string> candidates = {
        "/root/build_demo/wake_native",
        "/root/wake_8082_v6_bundle/wake_native",
        "/root/build_demo/build_demo/wake_native",
        "./wake_native",
        "/tmp/wake-8082-v6-bundle/wake_native"
    };
    for (const auto &p : candidates) {
        if (access(p.c_str(), X_OK) == 0) return p;
    }
    return "";
}

std::string V6NativeEngine::findModelPath() {
    const std::vector<std::string> candidates = {
        "/root/build_demo/models/M5R_logistic.txt",
        "/root/wake_8082_v6_bundle/models/M5R_logistic.txt",
        "./models/M5R_logistic.txt"
    };
    for (const auto &p : candidates) {
        if (access(p.c_str(), R_OK) == 0) return p;
    }
    return "";
}

bool V6NativeEngine::start(const std::string &mac, const std::string &ip,
                           std::string *error_msg) {
    if (is_running_.load()) {
        stop("Restarting with new target");
    }

    std::string clean_mac = toLower(mac);
    std::string clean_ip = ip;

    if (clean_mac.empty()) {
        if (error_msg) *error_msg = "Địa chỉ MAC của thiết bị 5 GHz không được để trống";
        return false;
    }

    std::string bin_path = findBinaryPath();
    if (bin_path.empty()) {
        std::string err = "Không tìm thấy binary wake_native trên router";
        if (error_msg) *error_msg = err;
        std::cerr << "[V6Engine] " << err << '\n';
        return false;
    }

    std::string model_path = findModelPath();
    if (model_path.empty()) {
        std::string err = "Không tìm thấy mô hình M5R_logistic.txt trên router";
        if (error_msg) *error_msg = err;
        std::cerr << "[V6Engine] " << err << '\n';
        return false;
    }

    std::cout << "[V6Engine] Starting wake_native: bin=" << bin_path
              << ", model=" << model_path << ", sta=" << clean_mac
              << ", ip=" << clean_ip << '\n';

    // 1. Dọn dẹp triệt để phần cứng & lock cũ trước khi chạy
    system("killall -SIGKILL wake_native 2>/dev/null");
    system("rm -f /tmp/cfr_spool.lock /tmp/wake_ready_*");
    writeSysfs("/sys/kernel/debug/ieee80211/phy00/ath12k_hw0/enable_cfr", "0");
    writeSysfs("/sys/kernel/debug/ieee80211/phy00/netdev:phy00.0-0/stations/" + clean_mac + "/link-0/cfr_capture", "0 0 0 0");
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // 2. Tạo pipe đọc stdout từ wake_native
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) {
        if (error_msg) *error_msg = "Không thể tạo pipe đọc dữ liệu: " + std::string(strerror(errno));
        return false;
    }

    ready_file_ = "/tmp/wake_ready_" + std::to_string(getpid()) + "_" + std::to_string(nowMs());
    unlink(ready_file_.c_str());

    // 3. Fork tiến trình suy luận wake_native
    pid_t cpid = fork();
    if (cpid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        if (error_msg) *error_msg = "Fork wake_native thất bại: " + std::string(strerror(errno));
        return false;
    }

    if (cpid == 0) {
        // Tiến trình con (Child)
        close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) _exit(1);
        close(pipe_fds[1]);

        int log_fd = open("/tmp/wake_native.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }

        const char *relay_path = "/sys/kernel/debug/ieee80211/phy00/ath12k_hw0/cfr_capture0";
        execl(bin_path.c_str(), "wake_native",
              "--format", "json",
              "--step-packets", "10",
              "--model", model_path.c_str(),
              "--sta", clean_mac.c_str(),
              "--relay", relay_path,
              "--ready", ready_file_.c_str(),
              static_cast<char *>(nullptr));
        _exit(127);
    }

    // Tiến trình cha (Parent)
    close(pipe_fds[1]);
    inference_pid_ = cpid;
    pipe_fd_ = pipe_fds[0];

    // 4. Chờ tín hiệu sẵn sàng từ wake_native
    bool ready = false;
    for (int i = 0; i < 80; ++i) { // Chờ tối đa 4 giây (80 * 50ms)
        int status = 0;
        pid_t res = waitpid(cpid, &status, WNOHANG);
        if (res == cpid) {
            close(pipe_fd_);
            pipe_fd_ = -1;
            inference_pid_ = -1;
            unlink(ready_file_.c_str());
            std::string err = "wake_native thoát đột ngột (code " + std::to_string(WEXITSTATUS(status)) + "). Kiểm tra /tmp/wake_native.log";
            if (error_msg) *error_msg = err;
            std::cerr << "[V6Engine] " << err << '\n';
            return false;
        }
        if (access(ready_file_.c_str(), F_OK) == 0) {
            ready = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (!ready) {
        kill(cpid, SIGKILL);
        waitpid(cpid, nullptr, 0);
        close(pipe_fd_);
        pipe_fd_ = -1;
        inference_pid_ = -1;
        unlink(ready_file_.c_str());
        std::string err = "wake_native khởi động quá thời gian chờ (timeout)";
        if (error_msg) *error_msg = err;
        std::cerr << "[V6Engine] " << err << '\n';
        return false;
    }

    // 5. Kích hoạt thanh ghi CFR driver Qualcomm
    writeSysfs("/sys/kernel/debug/ieee80211/phy00/ath12k_hw0/enable_cfr", "1");
    writeSysfs("/sys/kernel/debug/ieee80211/phy00/netdev:phy00.0-0/stations/" + clean_mac + "/link-0/cfr_capture", "1 2 10 0");

    // 6. Khởi chạy tiến trình ICMP ping nội tại 10ms nếu có IP
    if (!clean_ip.empty()) {
        pid_t ping_pid = fork();
        if (ping_pid == 0) {
            int dev_null = open("/dev/null", O_WRONLY);
            if (dev_null >= 0) {
                dup2(dev_null, STDOUT_FILENO);
                dup2(dev_null, STDERR_FILENO);
                close(dev_null);
            }
            execl(bin_path.c_str(), "wake_native",
                  "--ping", clean_ip.c_str(),
                  "--interval-ms", "10",
                  static_cast<char *>(nullptr));
            _exit(127);
        }
        ping_pid_ = ping_pid;
    }

    active_mac_ = clean_mac;
    active_ip_ = clean_ip;
    is_running_.store(true);
    should_stop_.store(false);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        scores_.start(ScoreConfig{});
        snapshot_.running = true;
        snapshot_.mac = clean_mac;
        snapshot_.ip = clean_ip;
        snapshot_.state = "WAIT_READY";
        snapshot_.score = 0.0;
        snapshot_.stroke_progress = 0;
        snapshot_.stroke_fraction = 0.0;
        snapshot_.label = 0;
        snapshot_.accepted = 0;
        snapshot_.rejected = 0;
        snapshot_.reason = "";
        snapshot_.result_reason = "";
        snapshot_.error = "";
        snapshot_.last_update_ms = nowMs();
        current_streak_ = 0;
        current_motion_state_ = 0;
    }

    // 7. Khởi động reader thread đọc stdout liên tục
    reader_thread_ = std::thread(&V6NativeEngine::readerWorker, this, pipe_fd_, inference_pid_, ping_pid_);

    std::cout << "[V6Engine] wake_native successfully started (PID " << inference_pid_
              << ", Ping PID " << ping_pid_ << ")\n";
    return true;
}

bool V6NativeEngine::stop(const std::string &reason) {
    if (!is_running_.exchange(false)) return true;
    should_stop_.store(true);

    std::cout << "[V6Engine] Stopping wake_native (" << (reason.empty() ? "user request" : reason) << ")...\n";

    // 1. Dừng các tiến trình con
    if (inference_pid_ > 0) {
        kill(inference_pid_, SIGTERM);
    }
    if (ping_pid_ > 0) {
        kill(ping_pid_, SIGTERM);
    }

    // 2. Đóng pipe để ngắt luồng đọc
    if (pipe_fd_ >= 0) {
        close(pipe_fd_);
        pipe_fd_ = -1;
    }

    // 3. Chờ reader thread kết thúc
    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }

    // 4. Chờ tiến trình con thoát hẳn (tối đa 150ms, nếu không SIGKILL)
    if (inference_pid_ > 0) {
        for (int i = 0; i < 5; ++i) {
            int status = 0;
            if (waitpid(inference_pid_, &status, WNOHANG) == inference_pid_) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        kill(inference_pid_, SIGKILL);
        waitpid(inference_pid_, nullptr, 0);
        inference_pid_ = -1;
    }

    if (ping_pid_ > 0) {
        kill(ping_pid_, SIGKILL);
        waitpid(ping_pid_, nullptr, 0);
        ping_pid_ = -1;
    }

    // 5. Trả lại thanh ghi phần cứng cho hệ thống
    writeSysfs("/sys/kernel/debug/ieee80211/phy00/ath12k_hw0/enable_cfr", "0");
    if (!active_mac_.empty()) {
        writeSysfs("/sys/kernel/debug/ieee80211/phy00/netdev:phy00.0-0/stations/" + active_mac_ + "/link-0/cfr_capture", "0 0 0 0");
    }
    system("rm -f /tmp/cfr_spool.lock");
    if (!ready_file_.empty()) {
        unlink(ready_file_.c_str());
        ready_file_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.running = false;
        snapshot_.state = "READY";
        snapshot_.score = 0.0;
        snapshot_.stroke_progress = 0;
        snapshot_.stroke_fraction = 0.0;
        snapshot_.last_update_ms = nowMs();
        scores_.clear();
        current_streak_ = 0;
        current_motion_state_ = 0;
    }

    std::cout << "[V6Engine] wake_native stopped and hardware cleaned up\n";
    return true;
}

void V6NativeEngine::parseJsonLine(const std::string &line) {
    if (line.empty() || line.front() != '{') return;

    double received_ms = static_cast<double>(nowMs());
    double received_s = received_ms / 1000.0;

    std::string status = extractJsonString(line, "status");
    if (status == "unavailable") {
        std::lock_guard<std::mutex> lock(mutex_);
        scores_.gap();
        snapshot_.reason = extractJsonString(line, "reason");
        return;
    }

    double score = extractJsonDouble(line, "score", -1.0);
    if (score < 0.0) return;

    double timestamp_s = extractJsonDouble(line, "timestamp_s", -1.0);
    double source_time = (timestamp_s > 0.0) ? timestamp_s : received_s;

    double max_gap_s = extractJsonDouble(line, "max_gap_s", -1.0);
    double window_span_s = extractJsonDouble(line, "window_span_s", -1.0);

    bool quality_poor = (line.find("\"quality_ok\":false") != std::string::npos ||
                         line.find("\"quality_ok\": false") != std::string::npos);
    bool retained_gap = (line.find("\"retained_gap_ok\":true") != std::string::npos ||
                         line.find("\"retained_gap_ok\": true") != std::string::npos);

    bool continuous = !quality_poor && (max_gap_s < 0.0 || max_gap_s <= ScoreWake::gapLimit);
    bool valid = continuous || retained_gap;

    std::lock_guard<std::mutex> lock(mutex_);
    if (continuous && window_span_s > 0.0) {
        scores_.gapTimeout = std::clamp(1.5 * window_span_s * 10.0 / 39.0, ScoreWake::gapLimit, 2.4);
    }

    // Cơ chế Ngưỡng kép (Hysteresis) & Khử nhiễu đột biến (Debounce) chống nhiễu người ở gần:
    int label = current_motion_state_;
    if (current_motion_state_ == 0) {
        if (score >= motion_threshold_high_) {
            current_streak_++;
            if (current_streak_ >= motion_debounce_frames_) {
                current_motion_state_ = 1;
                label = 1;
            }
        } else {
            current_streak_ = 0;
            label = 0;
        }
    } else {
        // Đang trong trạng thái ĐỘNG (1):
        // Chỉ quay lại TĨNH (0) khi score tụt xuống dưới ngưỡng thấp
        if (score < motion_threshold_low_) {
            current_motion_state_ = 0;
            current_streak_ = 0;
            label = 0;
        } else {
            label = 1;
        }
    }

    scores_.push(source_time, received_ms, label, valid);

    std::string prevState = snapshot_.state;
    std::string currentState = scores_.wake.state();

    snapshot_.score = score;
    snapshot_.label = label;
    snapshot_.state = currentState;
    snapshot_.stroke_progress = scores_.wake.progress;
    snapshot_.stroke_fraction = scores_.wake.strokeFraction(scores_.observedTime);
    snapshot_.accepted = scores_.accepted;
    snapshot_.rejected = scores_.rejected;
    snapshot_.result_reason = scores_.resultReason;
    snapshot_.reason = valid ? "" : "csi_gap";
    snapshot_.frame_count++;
    snapshot_.last_update_ms = nowMs();

    if (currentState == "PASS" && prevState != "PASS") {
        std::cout << "[V6Engine] Gesture DETECTED (PASS)!\n";
    } else if (currentState == "FAIL" && prevState != "FAIL") {
        std::cout << "[V6Engine] Gesture failed (" << scores_.resultReason << ")\n";
    }
}

void V6NativeEngine::readerWorker(int stdout_fd, pid_t inference_pid, pid_t ping_pid) {
    (void)ping_pid;
    char buf[1024];
    std::string accumulator;

    while (!should_stop_.load()) {
        ssize_t n = read(stdout_fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            break; // EOF hoặc pipe đóng
        }
        buf[n] = '\0';
        accumulator.append(buf, static_cast<size_t>(n));

        size_t newline_pos;
        while ((newline_pos = accumulator.find('\n')) != std::string::npos) {
            std::string line = accumulator.substr(0, newline_pos);
            accumulator.erase(0, newline_pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            parseJsonLine(line);
        }
    }

    // Nếu tiến trình tự thoát đột ngột mà không phải do lệnh stop chủ động
    if (!should_stop_.load()) {
        std::cout << "[V6Engine] wake_native ended; gracefully terminating child processes and resetting hardware...\n";
        if (inference_pid > 0) {
            kill(inference_pid, SIGKILL);
            waitpid(inference_pid, nullptr, 0);
        }
        if (ping_pid > 0) {
            kill(ping_pid, SIGKILL);
            waitpid(ping_pid, nullptr, 0);
        }

        // Trả lại thanh ghi phần cứng Qualcomm sạch sẽ
        writeSysfs("/sys/kernel/debug/ieee80211/phy00/ath12k_hw0/enable_cfr", "0");
        if (!active_mac_.empty()) {
            writeSysfs("/sys/kernel/debug/ieee80211/phy00/netdev:phy00.0-0/stations/" + active_mac_ + "/link-0/cfr_capture", "0 0 0 0");
        }
        unlink(ready_file_.c_str());

        is_running_.store(false);
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.running = false;
        snapshot_.state = "READY";
        snapshot_.error = ""; // Xóa sạch lỗi để không hiển thị trước mặt người dùng
        snapshot_.score = 0.0;
        snapshot_.stroke_progress = 0;
        snapshot_.stroke_fraction = 0.0;
    }
}
