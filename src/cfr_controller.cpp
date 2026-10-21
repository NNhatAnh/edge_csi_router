#include "cfr_controller.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
std::string toLower(std::string str) {
    for (char &c : str) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return str;
}

std::string toUpper(std::string str) {
    for (char &c : str) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return str;
}

bool findNewestDumpInDir(const std::string &dir, std::string &out_path,
                         size_t &out_size) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(dir, ec))
        return false;

    fs::path newest_path;
    fs::file_time_type newest_time{};
    uintmax_t newest_size = 0;

    for (const auto &entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec))
            continue;
        const std::string filename = entry.path().filename().string();
        if (filename.find("cfr_dump_phy00_") == 0 &&
            entry.path().extension() == ".bin") {
            const auto mtime = entry.last_write_time(ec);
            if (newest_path.empty() || mtime > newest_time) {
                newest_path = entry.path();
                newest_time = mtime;
                newest_size = entry.file_size(ec);
            }
        }
    }

    if (newest_path.empty())
        return false;
    out_path = newest_path.string();
    out_size = static_cast<size_t>(newest_size);
    return true;
}
} // namespace

CFRController::CFRController() { pci_path_ = findAth12kPciPath(); }

CFRController::~CFRController() { stopCapture(); }

std::string CFRController::findAth12kPciPath() {
    namespace fs = std::filesystem;
    const std::string default_path =
        "/sys/kernel/debug/ath12k/pci-0000:01:00.0";
    std::error_code ec;
    if (fs::is_directory(default_path, ec)) {
        return default_path;
    }

    const std::string ath12k_dir = "/sys/kernel/debug/ath12k";
    if (fs::is_directory(ath12k_dir, ec)) {
        for (const auto &entry : fs::directory_iterator(ath12k_dir, ec)) {
            if (entry.is_directory(ec)) {
                const std::string name = entry.path().filename().string();
                if (name.find("pci-") == 0) {
                    return entry.path().string();
                }
            }
        }
    }
    return default_path;
}

std::string CFRController::getPhyFromIface(const std::string &iface) {
    size_t dot = iface.find('.');
    if (dot != std::string::npos && dot > 0) {
        return iface.substr(0, dot);
    }
    return "phy00";
}

std::string CFRController::getHwFromIface(const std::string &iface) {
    if (iface.find(".0-") != std::string::npos ||
        iface.find("mac0") != std::string::npos ||
        iface.find("5G") != std::string::npos) {
        return "ath12k_hw0";
    }
    if (iface.find(".1-") != std::string::npos ||
        iface.find("mac1") != std::string::npos ||
        iface.find("2.4G") != std::string::npos) {
        return "ath12k_hw1";
    }
    return "ath12k_hw1";
}

std::string CFRController::getMacDevFromIface(const std::string &iface) {
    if (iface.find(".0-") != std::string::npos ||
        iface.find("mac0") != std::string::npos ||
        iface.find("5G") != std::string::npos) {
        return "mac0";
    }
    if (iface.find(".1-") != std::string::npos ||
        iface.find("mac1") != std::string::npos ||
        iface.find("2.4G") != std::string::npos) {
        return "mac1";
    }
    return "mac1";
}

bool CFRController::writeSysfs(const std::string &path,
                               const std::string &value) {
    int fd = open(path.c_str(), O_WRONLY);
    if (fd < 0) {
        std::cerr << "[CFR] Cannot open " << path
                  << " for writing: " << std::strerror(errno) << '\n';
        return false;
    }
    const std::string payload = value + "\n";
    ssize_t written = write(fd, payload.c_str(), payload.size());
    close(fd);
    if (written < 0 || static_cast<size_t>(written) < payload.size()) {
        std::cerr << "[CFR] Incomplete write to " << path << ": "
                  << std::strerror(errno) << '\n';
        return false;
    }
    return true;
}

void CFRController::cleanupDumpFiles(size_t keep_latest) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const std::string tmp_dir = "/tmp";
    if (!fs::is_directory(tmp_dir, ec))
        return;

    struct DumpEntry {
        fs::path path;
        fs::file_time_type mtime;
    };
    std::vector<DumpEntry> dumps;

    for (const auto &entry : fs::directory_iterator(tmp_dir, ec)) {
        if (!entry.is_regular_file(ec))
            continue;
        const std::string filename = entry.path().filename().string();
        if (filename.find("cfr_dump_phy00_") == 0 &&
            entry.path().extension() == ".bin") {
            dumps.push_back({entry.path(), entry.last_write_time(ec)});
        }
    }

    if (dumps.size() <= keep_latest)
        return;

    std::sort(dumps.begin(), dumps.end(),
              [](const DumpEntry &a, const DumpEntry &b) {
                  return a.mtime > b.mtime;
              });

    for (size_t i = keep_latest; i < dumps.size(); ++i) {
        fs::remove(dumps[i].path, ec);
    }
}

void CFRController::checkProcessAliveLocked() {
    if (running_ && pid_ > 0) {
        int status = 0;
        pid_t res = waitpid(pid_, &status, WNOHANG);
        if (res == pid_ || (kill(pid_, 0) != 0 && errno == ESRCH)) {
            running_ = false;
            pid_ = -1;
            last_error_ = "cfr_test_app exited unexpectedly";
            std::cerr << "[CFR] Process died unexpectedly\n";
        }
    }
}

bool CFRController::startCapture(const std::string &mac,
                                 const std::string &iface,
                                 const std::string &ip, int period,
                                 int bandwidth, int method) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Dừng capture cũ nếu đang chạy
    if (running_) {
        stopCaptureLocked();
    }

    std::string mac_raw = mac;
    std::string mac_lower = toLower(mac_raw);
    std::string mac_upper = toUpper(mac_raw);
    std::string chosen_iface = iface;

    // Tự động tìm interface nếu không được chỉ định
    if (chosen_iface.empty()) {
        namespace fs = std::filesystem;
        std::error_code ec;
        const std::string base = "/sys/kernel/debug/ieee80211/phy00";
        if (fs::is_directory(base, ec)) {
            for (const auto &entry : fs::directory_iterator(base, ec)) {
                const std::string dirname = entry.path().filename().string();
                if (dirname.find("netdev:") == 0) {
                    std::string test_path = entry.path().string() +
                                            "/stations/" + mac_lower +
                                            "/link-0/cfr_capture";
                    if (fs::exists(test_path, ec)) {
                        chosen_iface = dirname.substr(7); // Bỏ prefix "netdev:"
                        break;
                    }
                }
            }
        }
        if (chosen_iface.empty()) {
            if (fs::exists("/sys/kernel/debug/ieee80211/phy00/netdev:phy00.2-0", ec)) {
                chosen_iface = "phy00.2-0"; // Ưu tiên 5GHz (4-5 anten, 80MHz)
            } else if (fs::exists("/sys/kernel/debug/ieee80211/phy00/netdev:phy00.0-0", ec)) {
                chosen_iface = "phy00.0-0";
            } else {
                chosen_iface = "phy00.1-0"; // Fallback 2.4GHz
            }
        }
    }

    phy_ = getPhyFromIface(chosen_iface);
    hw_ = getHwFromIface(chosen_iface);
    mac_dev_ = getMacDevFromIface(chosen_iface);
    pci_path_ = findAth12kPciPath();
    iface_ = chosen_iface;
    target_mac_ = mac_upper;
    target_ip_ = ip;

    // Tự động kiểm tra và điều chỉnh băng thông khả dụng để tránh lỗi -EINVAL của ath12k
    std::vector<int> avail = getAvailableBandwidths(iface_, target_mac_);
    if (!avail.empty()) {
        if (std::find(avail.begin(), avail.end(), bandwidth) == avail.end()) {
            int adjusted = avail.back();
            std::cout << "[CFR] Requested BW code " << bandwidth << " not supported for "
                      << target_mac_ << " on " << iface_
                      << ". Auto-adjusting to max available: " << adjusted << '\n';
            bandwidth = adjusted;
        }
    }

    bandwidth_ = bandwidth;
    period_ = period;

    std::cout << "[CFR] Preparing capture: Station=" << target_mac_
              << " IFACE=" << iface_ << " PHY=" << phy_ << " HW=" << hw_
              << " MAC_DEV=" << mac_dev_ << " BW=" << bandwidth_ << '\n';

    // Dọn dẹp file dump cũ trong /tmp nhưng giữ lại 1 file gần nhất để tải về
    cleanupDumpFiles(1);

    // Kích hoạt CFR trên phần cứng ath12k PCI MAC
    const std::string enable_cfr_path =
        pci_path_ + "/" + mac_dev_ + "/enable_cfr";
    if (!writeSysfs(enable_cfr_path, "1")) {
        last_error_ = "Failed to write 1 to " + enable_cfr_path;
        return false;
    }

    // Cấu hình thu thập CFR cho station cụ thể trong debugfs
    std::string sta_cfr_path = "/sys/kernel/debug/ieee80211/" + phy_ +
                               "/netdev:" + iface_ + "/stations/" + mac_lower +
                               "/link-0/cfr_capture";
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(sta_cfr_path, ec)) {
        // Dự phòng nếu không có thư mục link-0
        sta_cfr_path = "/sys/kernel/debug/ieee80211/" + phy_ +
                       "/netdev:" + iface_ + "/stations/" + mac_lower +
                       "/cfr_capture";
    }

    const std::string cfr_val = "1 " + std::to_string(bandwidth) + " " +
                                std::to_string(period) + " " +
                                std::to_string(method);
    if (!writeSysfs(sta_cfr_path, cfr_val)) {
        last_error_ = "Failed to configure cfr_capture at " + sta_cfr_path;
        writeSysfs(enable_cfr_path, "0");
        return false;
    }

    // Tiêu diệt tiến trình cfr_test_app còn tồn đọng trước khi khởi chạy mới
    int kill_res = system("killall -9 cfr_test_app 2>/dev/null");
    (void)kill_res;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Fork tiến trình con chạy cfr_test_app
    pid_t cpid = fork();
    if (cpid < 0) {
        last_error_ = "fork() failed: " + std::string(std::strerror(errno));
        writeSysfs(sta_cfr_path, "0 0 0 0");
        writeSysfs(enable_cfr_path, "0");
        return false;
    }

    if (cpid == 0) {
        // Trong tiến trình con
        int dev_null = open("/dev/null", O_RDONLY);
        if (dev_null >= 0) {
            dup2(dev_null, STDIN_FILENO);
            close(dev_null);
        }

        int log_fd =
            open("/tmp/cfr_test_app.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }

        // Đóng toàn bộ file descriptors kế thừa từ tiến trình cha để không
        // chiếm giữ socket Web
        for (int fd = 3; fd < 256; ++fd) {
            close(fd);
        }

        setsid();
        execl("/usr/sbin/cfr_test_app", "cfr_test_app", "-i", phy_.c_str(),
              "-c", hw_.c_str(), static_cast<char *>(nullptr));
        execlp("cfr_test_app", "cfr_test_app", "-i", phy_.c_str(), "-c",
               hw_.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }

    // Trong tiến trình cha: đợi 100ms để kiểm tra xem tiến trình con 
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int status = 0;
    pid_t res = waitpid(cpid, &status, WNOHANG);
    if (res == cpid) {
        last_error_ = "cfr_test_app exited immediately (code " +
                      std::to_string(WEXITSTATUS(status)) + ")";
        writeSysfs(sta_cfr_path, "0 0 0 0");
        writeSysfs(enable_cfr_path, "0");
        return false;
    }

    pid_ = cpid;
    running_ = true;
    start_time_ = std::chrono::steady_clock::now();
    last_error_.clear();

    std::cout << "[CFR] Capture daemon started successfully (PID: " << pid_
              << " for " << target_mac_ << " on " << iface_ << ")\n";
    return true;
}

bool CFRController::stopCapture() {
    std::lock_guard<std::mutex> lock(mutex_);
    return stopCaptureLocked();
}

bool CFRController::stopCaptureLocked() {
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        for (int i = 0; i < 5; ++i) {
            int status = 0;
            pid_t res = waitpid(pid_, &status, WNOHANG);
            if (res == pid_ || (kill(pid_, 0) != 0 && errno == ESRCH)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (kill(pid_, 0) == 0) {
            kill(pid_, SIGKILL);
            waitpid(pid_, nullptr, WNOHANG);
        }
        pid_ = -1;
    }

    int kill_res = system("killall cfr_test_app 2>/dev/null");
    (void)kill_res;

    // Tắt cfr_capture trên station debugfs
    if (!target_mac_.empty() && !iface_.empty()) {
        std::string mac_lower = toLower(target_mac_);
        std::string sta_cfr_path = "/sys/kernel/debug/ieee80211/" + phy_ +
                                   "/netdev:" + iface_ + "/stations/" +
                                   mac_lower + "/link-0/cfr_capture";
        namespace fs = std::filesystem;
        std::error_code ec;
        if (!fs::exists(sta_cfr_path, ec)) {
            sta_cfr_path = "/sys/kernel/debug/ieee80211/" + phy_ +
                           "/netdev:" + iface_ + "/stations/" + mac_lower +
                           "/cfr_capture";
        }
        writeSysfs(sta_cfr_path, "0 0 0 0");
    }

    // Tắt enable_cfr trên mac dev
    if (!mac_dev_.empty() && !pci_path_.empty()) {
        const std::string enable_cfr_path =
            pci_path_ + "/" + mac_dev_ + "/enable_cfr";
        writeSysfs(enable_cfr_path, "0");
    }

    // Ghi nhận file vừa thu thập
    std::string dump_path;
    size_t dump_size = 0;
    if (findNewestDumpInDir("/tmp", dump_path, dump_size)) {
        last_captured_file_ = dump_path;
        last_captured_size_ = dump_size;
    }

    running_ = false;
    std::cout << "[CFR] Capture stopped\n";
    return true;
}

CFRCaptureStatus CFRController::getStatus() {
    std::lock_guard<std::mutex> lock(mutex_);
    checkProcessAliveLocked();

    CFRCaptureStatus status;
    status.running = running_;
    status.target_mac = target_mac_;
    status.target_ip = target_ip_;
    status.iface = iface_;
    status.phy = phy_;
    status.hw = hw_;
    status.mac_dev = mac_dev_;
    status.pci_path = pci_path_;
    status.pid = pid_;
    status.bandwidth = bandwidth_;
    status.period = period_;
    if (running_) {
        const auto now = std::chrono::steady_clock::now();
        status.uptime_sec = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time_)
                .count());
    } else {
        status.uptime_sec = 0;
    }
    status.last_error = last_error_;

    std::string dump_path;
    size_t dump_size = 0;
    if (findNewestDumpInDir("/tmp", dump_path, dump_size)) {
        last_captured_file_ = dump_path;
        last_captured_size_ = dump_size;
    }

    if (!last_captured_file_.empty()) {
        namespace fs = std::filesystem;
        status.latest_file = fs::path(last_captured_file_).filename().string();
        status.latest_file_size = last_captured_size_;
    }

    status.available_bandwidths = getAvailableBandwidths(iface_, target_mac_);

    return status;
}

std::vector<int> CFRController::getAvailableBandwidths(const std::string &iface,
                                                      const std::string &mac) const {
    int iface_max_bw = 0; // 0: 20MHz, 1: 40MHz, 2: 80MHz, 3: 160MHz, 4: 320MHz

    std::string test_iface = iface.empty() ? "phy00.1-0" : iface;

    // 1. Kiểm tra băng thông hoạt động thực tế của interface qua `iw dev <iface> info`
    std::string cmd = "iw dev " + test_iface + " info 2>/dev/null";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe)) {
            std::string line(buf);
            size_t pos = line.find("width:");
            if (pos != std::string::npos) {
                if (line.find("320 MHz", pos) != std::string::npos)
                    iface_max_bw = 4;
                else if (line.find("160 MHz", pos) != std::string::npos)
                    iface_max_bw = 3;
                else if (line.find("80 MHz", pos) != std::string::npos)
                    iface_max_bw = 2;
                else if (line.find("40 MHz", pos) != std::string::npos)
                    iface_max_bw = 1;
                else if (line.find("20 MHz", pos) != std::string::npos)
                    iface_max_bw = 0;
                break;
            }
        }
        pclose(pipe);
    } else {
        if (test_iface.find(".0-") != std::string::npos || test_iface.find("5G") != std::string::npos) {
            iface_max_bw = 2; // 80 MHz cho 5GHz
        } else {
            iface_max_bw = 0; // 20 MHz cho 2.4GHz
        }
    }

    // 2. Nếu có MAC cụ thể, đối chiếu năng lực hỗ trợ của client qua debugfs
    int station_max_bw = iface_max_bw;
    if (!mac.empty()) {
        std::string mac_lower = toLower(mac);
        std::string phy = getPhyFromIface(test_iface);
        std::string base = "/sys/kernel/debug/ieee80211/" + phy + "/netdev:" + test_iface + "/stations/" + mac_lower + "/link-0/";
        namespace fs = std::filesystem;
        std::error_code ec;
        if (!fs::is_directory(base, ec)) {
            base = "/sys/kernel/debug/ieee80211/" + phy + "/netdev:" + test_iface + "/stations/" + mac_lower + "/";
        }

        if (fs::is_directory(base, ec)) {
            auto readCap = [&](const std::string &fname) -> std::string {
                std::ifstream f(base + fname);
                if (!f) return "";
                std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                return s;
            };

            std::string eht = readCap("eht_capa");
            std::string he = readCap("he_capa");
            std::string vht = readCap("vht_capa");
            std::string ht = readCap("ht_capa");

            if (!eht.empty() && eht.find("not supported") == std::string::npos) {
                station_max_bw = (eht.find("320") != std::string::npos) ? 4 : 3;
            } else if (!he.empty() && he.find("not supported") == std::string::npos) {
                station_max_bw = (he.find("160") != std::string::npos) ? 3 : 2;
            } else if (!vht.empty() && vht.find("not supported") == std::string::npos) {
                station_max_bw = (vht.find("160") != std::string::npos) ? 3 : 2;
            } else if (!ht.empty()) {
                if (ht.find("HT40") != std::string::npos && ht.find("No DSSS/CCK HT40") == std::string::npos) {
                    station_max_bw = 1;
                } else {
                    station_max_bw = 0;
                }
            } else {
                station_max_bw = 0;
            }
        }
    }

    int effective_max = std::min(iface_max_bw, station_max_bw);
    std::vector<int> result;
    for (int i = 0; i <= effective_max; ++i) {
        result.push_back(i);
    }
    return result.empty() ? std::vector<int>{0} : result;
}

std::string CFRController::getLatestCaptureFilePath(size_t *file_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string dump_path;
    size_t dump_size = 0;
    if (findNewestDumpInDir("/tmp", dump_path, dump_size)) {
        last_captured_file_ = dump_path;
        last_captured_size_ = dump_size;
    }

    if (!last_captured_file_.empty()) {
        namespace fs = std::filesystem;
        std::error_code ec;
        if (fs::exists(last_captured_file_, ec)) {
            if (file_size)
                *file_size = last_captured_size_;
            return last_captured_file_;
        }
    }
    return "";
}
