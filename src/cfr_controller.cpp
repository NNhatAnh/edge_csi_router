#include "cfr_controller.hpp"
#include "driver_audit_logger.hpp"

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

CFRController::CFRController() {
    pci_path_ = findAth12kPciPath();
    DriverAuditLogger::instance().log("STARTUP", "CLEANUP_BEGIN", "all", "Resetting hardware registers and stale processes");

    // Dọn dẹp trạng thái mồ côi khi khởi động:
    // TUÂN THỦ NGUYÊN TẮC: Driver First, Process Second
    // 1. Đưa toàn bộ thanh ghi cfr_capture của các station đã kết nối về "0 0 0 0" TRƯỚC HẾT
    namespace fs = std::filesystem;
    std::error_code ec;
    const std::string ieee80211_dir = "/sys/kernel/debug/ieee80211";
    if (fs::is_directory(ieee80211_dir, ec)) {
        for (const auto &phy_entry : fs::directory_iterator(ieee80211_dir, ec)) {
            if (!phy_entry.is_directory(ec)) continue;
            for (const auto &dev_entry : fs::directory_iterator(phy_entry.path(), ec)) {
                if (!dev_entry.is_directory(ec)) continue;
                std::string stas_dir = dev_entry.path().string() + "/stations";
                if (!fs::is_directory(stas_dir, ec)) continue;
                for (const auto &sta_entry : fs::directory_iterator(stas_dir, ec)) {
                    if (!sta_entry.is_directory(ec)) continue;
                    std::string cpath1 = sta_entry.path().string() + "/link-0/cfr_capture";
                    std::string cpath2 = sta_entry.path().string() + "/cfr_capture";
                    if (fs::exists(cpath1, ec)) writeSysfs(cpath1, "0 0 0 0");
                    if (fs::exists(cpath2, ec)) writeSysfs(cpath2, "0 0 0 0");
                }
            }
        }
    }

    // 2. Đưa toàn bộ thanh ghi enable_cfr của các mac device trên card ath12k về 0
    const std::string ath12k_dir = "/sys/kernel/debug/ath12k";
    if (fs::is_directory(ath12k_dir, ec)) {
        for (const auto &pci_entry : fs::directory_iterator(ath12k_dir, ec)) {
            if (pci_entry.is_directory(ec)) {
                std::string pci_name = pci_entry.path().filename().string();
                if (pci_name.rfind("pci-", 0) == 0) {
                    for (const auto &mac_entry : fs::directory_iterator(pci_entry.path(), ec)) {
                        if (mac_entry.is_directory(ec)) {
                            std::string mac_name = mac_entry.path().filename().string();
                            if (mac_name.rfind("mac", 0) == 0) {
                                std::string enable_path = mac_entry.path().string() + "/enable_cfr";
                                if (fs::exists(enable_path, ec)) {
                                    writeSysfs(enable_path, "0");
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Chờ 50ms để driver kernel ổn định
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 3. Dừng êm dịu tiến trình cfr_test_app cũ nếu còn sót lại từ phiên trước (SIGTERM -> SIGKILL)
    int term_res = system("killall -SIGTERM cfr_test_app 2>/dev/null");
    (void)term_res;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int kill_res = system("killall -SIGKILL cfr_test_app 2>/dev/null");
    (void)kill_res;

    DriverAuditLogger::instance().log("STARTUP", "CLEANUP_DONE", "all", "Hardware registers reset, stale processes cleaned");
}

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
    namespace fs = std::filesystem;
    std::error_code ec;

    // 1. Phân tích cấu trúc tên giao diện Linux mac80211 chuẩn: "phy00.<radio_id>-<sub_id>"
    // Ví dụ trên Dual-Band:
    //   phy00.0-0 -> radio 0 -> ath12k_hw0 (5GHz)
    //   phy00.1-0 -> radio 1 -> ath12k_hw1 (2.4GHz)
    // Ví dụ trên Tri-Band (IPQ9670 / IPQ9574):
    //   phy00.0-0 -> radio 0 -> ath12k_hw0 (6GHz)
    //   phy00.1-0 -> radio 1 -> ath12k_hw1 (2.4GHz)
    //   phy00.2-0 -> radio 2 -> ath12k_hw2 (5GHz)
    //   phy00.2-1 -> radio 2 -> ath12k_hw2 (5GHz guest)
    size_t dot_pos = iface.find('.');
    size_t dash_pos = iface.find('-', dot_pos);
    if (dot_pos != std::string::npos && dash_pos != std::string::npos && dash_pos > dot_pos + 1) {
        std::string radio_idx_str = iface.substr(dot_pos + 1, dash_pos - dot_pos - 1);
        try {
            int radio_idx = std::stoi(radio_idx_str);
            std::string candidate_hw = "ath12k_hw" + std::to_string(radio_idx);
            std::string phy = getPhyFromIface(iface);
            std::string hw_path = "/sys/kernel/debug/ieee80211/" + phy + "/" + candidate_hw;
            if (fs::exists(hw_path, ec)) {
                return candidate_hw;
            }
        } catch (...) {}
    }

    // 2. Fallback heuristic theo từ khóa băng tần nếu không có symlink
    if (iface.find(".2-") != std::string::npos) {
        return "ath12k_hw2";
    }
    if (iface.find(".1-") != std::string::npos ||
        iface.find("2.4G") != std::string::npos ||
        iface.find("2g") != std::string::npos ||
        iface.find("24G") != std::string::npos) {
        return "ath12k_hw1";
    }
    return "ath12k_hw0";
}

std::string CFRController::getMacDevFromIface(const std::string &iface) {
    namespace fs = std::filesystem;
    std::error_code ec;
    std::string phy = getPhyFromIface(iface);
    std::string hw = getHwFromIface(iface);
    std::string hw_link = "/sys/kernel/debug/ieee80211/" + phy + "/" + hw;
    if (fs::exists(hw_link, ec)) {
        fs::path resolved = fs::canonical(hw_link, ec);
        if (!ec && !resolved.empty()) {
            return resolved.filename().string();
        }
    }
    if (hw == "ath12k_hw1" && !fs::exists("/sys/kernel/debug/ath12k/pci-0002:01:00.0", ec)) {
        return "mac1";
    }
    return "mac0";
}

bool CFRController::writeSysfs(const std::string &path,
                               const std::string &value) {
    int fd = open(path.c_str(), O_WRONLY);
    if (fd < 0) {
        int err = errno;
        DriverAuditLogger::instance().logSysfsWrite(path, value, false, err);
        std::cerr << "[CFR] Cannot open " << path
                  << " for writing: " << std::strerror(err) << '\n';
        return false;
    }
    const std::string payload = value + "\n";
    ssize_t written = write(fd, payload.c_str(), payload.size());
    int err = (written < 0) ? errno : 0;
    close(fd);
    if (written < 0 || static_cast<size_t>(written) < payload.size()) {
        DriverAuditLogger::instance().logSysfsWrite(path, value, false, err);
        std::cerr << "[CFR] Incomplete write to " << path << ": "
                  << std::strerror(err) << '\n';
        return false;
    }
    DriverAuditLogger::instance().logSysfsWrite(path, value, true, 0);
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
            std::string mac = target_mac_;
            stopCaptureLocked();
            last_error_ = "Tiến trình cfr_test_app đã kết thúc. Đã dọn sạch thanh ghi CFR.";
            std::cerr << "[CFR] Process died unexpectedly, cleaned up CFR registers for " << mac << "\n";
        }
    }
}

void CFRController::checkSafetyGuardLocked() {
    if (!running_) return;

    // Cập nhật dung lượng file dump mới nhất nếu có
    std::string dump_path;
    size_t dump_size = 0;
    if (findNewestDumpInDir("/tmp", dump_path, dump_size)) {
        last_captured_file_ = dump_path;
        last_captured_size_ = dump_size;
    }

    // Cơ chế Xoay vòng file (File Rotation): Qualcomm cfr_test_app có giới hạn vòng tròn 8MB (8,388,608 bytes).
    // Đặt ngưỡng xoay vòng là 6MB để Daemon luôn chủ động mở file mới trước khi cfr_test_app bị wrap-around,
    // đảm bảo stream liên tục không giới hạn thời gian, không bao giờ đầy /tmp và không bị đứt đoạn frame.
    constexpr size_t kRotationDumpBytes = 6 * 1024 * 1024; // 6 MB (< 8 MB ring limit)
    if (last_captured_size_ >= kRotationDumpBytes) {
        std::cerr << "[CFR] File rotation triggered: dump reached "
                  << (last_captured_size_ / (1024 * 1024)) << " MB, rotating smoothly...\n";
        rotateCaptureLocked();
    }
}

bool CFRController::rotateCaptureLocked() {
    if (!running_ || pid_ <= 0) return false;

    pid_t old_pid = pid_;
    pid_t cpid = fork();
    if (cpid < 0) {
        std::cerr << "[CFR] Failed to fork new cfr_test_app for rotation: " << std::strerror(errno) << '\n';
        return false;
    }

    if (cpid == 0) {
        int dev_null = open("/dev/null", O_RDONLY);
        if (dev_null >= 0) {
            dup2(dev_null, STDIN_FILENO);
            close(dev_null);
        }
        int log_fd = open("/tmp/cfr_test_app.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }
        for (int fd = 3; fd < 256; ++fd) close(fd);
        setsid();
        execl("/usr/sbin/cfr_test_app", "cfr_test_app", "-i", phy_.c_str(),
              "-c", hw_.c_str(), static_cast<char *>(nullptr));
        execlp("cfr_test_app", "cfr_test_app", "-i", phy_.c_str(), "-c",
               hw_.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    int status = 0;
    if (waitpid(cpid, &status, WNOHANG) == cpid) {
        std::cerr << "[CFR] Rotated cfr_test_app exited immediately\n";
        return false;
    }

    pid_ = cpid;

    // Dừng tiến trình cũ êm dịu
    kill(old_pid, SIGTERM);
    waitpid(old_pid, nullptr, WNOHANG);

    // Dọn dẹp file dump cũ để giải phóng dung lượng, giữ file mới nhất
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cleanupDumpFiles(1);
    last_captured_size_ = 0;

    DriverAuditLogger::instance().log("CFR", "FILE_ROTATION", target_mac_,
        "Rotated dump file at 6MB, new pid=" + std::to_string(pid_));
    std::cout << "[CFR] File rotation complete: New cfr_test_app PID " << pid_ << ", old dump purged\n";
    return true;
}

bool CFRController::isTargetStationConnectedLocked() const {
    if (target_mac_.empty())
        return true;

    std::string mac_lower = toLower(target_mac_);
    namespace fs = std::filesystem;
    std::error_code ec;

    // 1. Kiểm tra debugfs của driver vô tuyến Linux mac80211 / ath12k
    const std::string base_debugfs = "/sys/kernel/debug/ieee80211";
    if (fs::is_directory(base_debugfs, ec)) {
        if (!phy_.empty() && !iface_.empty()) {
            std::string sta_dir = base_debugfs + "/" + phy_ + "/netdev:" + iface_ + "/stations/" + mac_lower;
            if (fs::exists(sta_dir, ec)) return true;

            std::string sta_link0 = sta_dir + "/link-0";
            if (fs::exists(sta_link0, ec)) return true;
        }

        // Quét tìm trên bất kỳ netdev nào của phy này (đề phòng client roam băng tần)
        std::string phy_base = base_debugfs + "/" + (phy_.empty() ? "phy00" : phy_);
        if (fs::is_directory(phy_base, ec)) {
            for (const auto &entry : fs::directory_iterator(phy_base, ec)) {
                const std::string name = entry.path().filename().string();
                if (name.find("netdev:") == 0) {
                    std::string test_dir = entry.path().string() + "/stations/" + mac_lower;
                    if (fs::exists(test_dir, ec)) return true;
                }
            }
        }
    }

    // Tuyệt đối không gọi system("iw dev ... station get") vì sẽ fork hàng trăm process/s làm nghẽn kernel driver
    return false;
}

void CFRController::checkTargetConnectionLocked() {
    if (!running_ || target_mac_.empty()) {
        target_disconnect_count_ = 0;
        return;
    }

    auto now = std::chrono::steady_clock::now();
    // Cho phép thời gian ân hạn 10 giây sau khi bắt đầu thu để driver & station hoàn tất bắt tay CFR
    if (std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count() < 10) {
        return;
    }

    // Giới hạn tần suất kiểm tra tối đa 1 lần mỗi 2000ms để tránh false-trigger từ vòng lặp đọc CFR
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_connection_check_).count() < 2000) {
        return;
    }
    last_connection_check_ = now;

    if (!isTargetStationConnectedLocked()) {
        ++target_disconnect_count_;
        std::cerr << "[CFR] Target station " << target_mac_
                  << " connection check failed (" << target_disconnect_count_ << "/2)\n";
        // Cần 2 lần kiểm tra liên tiếp (~3-4s) không thấy thiết bị để ngắt CFR kịp thời
        if (target_disconnect_count_ >= 2) {
            std::string mac = target_mac_;
            std::string ip = target_ip_;
            stopCaptureLocked();
            // Xóa sạch toàn bộ dữ liệu tồn đọng trong /tmp khi thiết bị ngắt kết nối
            cleanupDumpFiles(0);
            last_captured_file_.clear();
            last_captured_size_ = 0;
            last_error_ = "Thiết bị (" + mac + (ip.empty() ? "" : (" / " + ip)) +
                          ") đã ngắt kết nối. Đã dừng thu và dọn sạch dữ liệu tồn đọng.";
            DriverAuditLogger::instance().log("CFR", "TARGET_DISCONNECTED_AUTO_STOP", mac,
                "Station disconnected. Stopped capture and purged all residual dump files.");
            std::cerr << "[CFR] Target station " << mac
                      << " disconnected. Auto-stopped capture and cleaned up all residual data.\n";
            target_disconnect_count_ = 0;
        }
    } else {
        target_disconnect_count_ = 0;
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
                chosen_iface = "phy00.2-0"; // Mặc định 5GHz trên Tri-Band
            } else if (fs::exists("/sys/kernel/debug/ieee80211/phy00/netdev:phy00.0-0", ec)) {
                chosen_iface = "phy00.0-0"; // Mặc định 5GHz trên Dual-Band
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

    // Phân giải chính xác pci_path_ và mac_dev_ từ symlink của Linux mac80211 nếu có
    namespace fs = std::filesystem;
    std::error_code ec;
    std::string hw_link = "/sys/kernel/debug/ieee80211/" + phy_ + "/" + hw_;
    if (fs::exists(hw_link, ec)) {
        fs::path resolved = fs::canonical(hw_link, ec);
        if (!ec && !resolved.empty()) {
            mac_dev_ = resolved.filename().string();
            pci_path_ = resolved.parent_path().string();
        }
    }

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
    method_ = method;

    std::cout << "[CFR] Preparing capture: Station=" << target_mac_
              << " IFACE=" << iface_ << " PHY=" << phy_ << " HW=" << hw_
              << " MAC_DEV=" << mac_dev_ << " BW=" << bandwidth_ << '\n';

    DriverAuditLogger::instance().log("CFR", "START_REQUEST", target_mac_,
        "iface=" + iface_ + " phy=" + phy_ + " hw=" + hw_ + " bw=" + std::to_string(bandwidth_) +
        " period=" + std::to_string(period_));

    // Kích hoạt cơ chế tự phục hồi firmware của ath12k thay vì gọi BUG_ON() gây kernel panic
    if (!pci_path_.empty()) {
        writeSysfs(pci_path_ + "/set_fw_recovery", "1");
    }
    writeSysfs("/sys/kernel/debug/ath12k/pci-0000:01:00.0/set_fw_recovery", "1");
    refreshFrequencyCacheLocked(iface_);

    // Dọn dẹp file dump cũ trong /tmp nhưng giữ lại 1 file gần nhất để tải về
    cleanupDumpFiles(1);

    // Xác định đường dẫn enable_cfr trên radio
    std::string enable_cfr_path = "/sys/kernel/debug/ieee80211/" + phy_ + "/" + hw_ + "/enable_cfr";
    if (!fs::exists(enable_cfr_path, ec)) {
        enable_cfr_path = pci_path_ + "/" + mac_dev_ + "/enable_cfr";
    }

    // Đưa thanh ghi về 0 trước khi khởi chạy reader mới
    if (fs::exists(enable_cfr_path, ec)) {
        writeSysfs(enable_cfr_path, "0");
    }

    // Cấu hình đường dẫn thu thập CFR cho station cụ thể trong debugfs
    std::string sta_cfr_path = "/sys/kernel/debug/ieee80211/" + phy_ +
                               "/netdev:" + iface_ + "/stations/" + mac_lower +
                               "/link-0/cfr_capture";
    if (!fs::exists(sta_cfr_path, ec)) {
        // Dự phòng nếu không có thư mục link-0
        sta_cfr_path = "/sys/kernel/debug/ieee80211/" + phy_ +
                       "/netdev:" + iface_ + "/stations/" + mac_lower +
                       "/cfr_capture";
    }

    // Luôn đưa cfr_capture của station này về 0 trước để xóa sạch struct peer trong driver ath12k,
    // loại bỏ hoàn toàn cơ chế skip lệnh WMI của ath12k_dbg_sta_write_cfr_capture khi tham số trùng
    if (fs::exists(sta_cfr_path, ec)) {
        writeSysfs(sta_cfr_path, "0 0 0 0");
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    // 2. KHỞI CHẠY cfr_test_app TRƯỚC:
    // Đảm bảo tiến trình reader đã mở relay /sys/.../cfr_capture0 và tạo file dump mới
    // trước khi bật thanh ghi driver, triệt tiêu 100% tình trạng tràn 1MB+ metadata rác sinh non.
    DriverAuditLogger::instance().logProcess("FORK_BEGIN", 0, "Forking cfr_test_app");
    pid_t cpid = fork();
    if (cpid < 0) {
        int err = errno;
        last_error_ = "fork() failed: " + std::string(std::strerror(err));
        DriverAuditLogger::instance().logProcess("FORK_FAIL", -1, last_error_);
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

    DriverAuditLogger::instance().logProcess("FORK_SUCCESS", cpid, "Child spawned");

    // 3. Trong tiến trình cha: đợi 150ms để cfr_test_app tạo file dump và chờ select()
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    int status = 0;
    pid_t res = waitpid(cpid, &status, WNOHANG);
    if (res == cpid) {
        last_error_ = "cfr_test_app exited immediately (code " +
                      std::to_string(WEXITSTATUS(status)) + ")";
        DriverAuditLogger::instance().logProcess("IMMEDIATE_EXIT", cpid, last_error_);
        return false;
    }

    // 4. BÂY GIỜ TIẾN HÀNH KÍCH HOẠT THANH GHI DRIVER ATH12K
    if (!writeSysfs(enable_cfr_path, "1")) {
        last_error_ = "Failed to write 1 to " + enable_cfr_path;
        kill(cpid, SIGTERM);
        return false;
    }
    // Chờ 50ms để lệnh WMI set param của ath12k hoàn tất và cập nhật cờ thanh ghi nội bộ
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const std::string cfr_val = "1 " + std::to_string(bandwidth) + " " +
                                std::to_string(period) + " " +
                                std::to_string(method);
    if (!writeSysfs(sta_cfr_path, cfr_val)) {
        // Tự động thử lại với băng thông 20MHz (BW code 0) nếu card mạng hoặc trạm từ chối BW hiện tại
        std::cerr << "[CFR] Warning: write " << cfr_val << " to " << sta_cfr_path
                  << " failed, retrying with BW 0...\n";
        const std::string fallback_cfr_val = "1 0 " + std::to_string(period) + " " +
                                             std::to_string(method);
        if (!writeSysfs(sta_cfr_path, fallback_cfr_val)) {
            last_error_ = "Failed to configure cfr_capture at " + sta_cfr_path;
            writeSysfs(enable_cfr_path, "0");
            kill(cpid, SIGTERM);
            return false;
        }
        bandwidth_ = 0;
    }

    pid_ = cpid;
    running_ = true;
    start_time_ = std::chrono::steady_clock::now();
    last_error_.clear();
    target_disconnect_count_ = 0;

    DriverAuditLogger::instance().log("CFR", "START_COMPLETE", target_mac_,
        "pid=" + std::to_string(pid_) + " bw=" + std::to_string(bandwidth_));

    std::cout << "[CFR] Capture daemon started successfully (PID: " << pid_
              << " for " << target_mac_ << " on " << iface_ << ")\n";
    return true;
}

bool CFRController::updatePeriod(int period) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) return false;
    if (period < 10 || period > 2000) return false;
    period_ = period;

    std::string mac_lower = toLower(target_mac_);
    namespace fs = std::filesystem;
    std::error_code ec;
    std::string sta_cfr_path = "/sys/kernel/debug/ieee80211/" + phy_ +
                               "/netdev:" + iface_ + "/stations/" + mac_lower +
                               "/link-0/cfr_capture";
    if (!fs::exists(sta_cfr_path, ec)) {
        sta_cfr_path = "/sys/kernel/debug/ieee80211/" + phy_ +
                       "/netdev:" + iface_ + "/stations/" + mac_lower +
                       "/cfr_capture";
    }
    if (fs::exists(sta_cfr_path, ec)) {
        const std::string cfr_val = "1 " + std::to_string(bandwidth_) + " " +
                                    std::to_string(period_) + " " +
                                    std::to_string(method_);
        writeSysfs(sta_cfr_path, cfr_val);
        DriverAuditLogger::instance().log("CFR", "UPDATE_PERIOD", target_mac_, "val=" + cfr_val);
        return true;
    }
    return false;
}

bool CFRController::stopCapture(const std::string &reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool ret = stopCaptureLocked();
    if (!reason.empty()) {
        last_error_ = reason;
    }
    return ret;
}

bool CFRController::isRunning() {
    std::lock_guard<std::mutex> lock(mutex_);
    checkProcessAliveLocked();
    checkTargetConnectionLocked();
    checkSafetyGuardLocked();
    return running_;
}

bool CFRController::isCapturingLive() const {
    return running_ && pid_ > 0 && !target_mac_.empty();
}

std::string CFRController::getCaptureIface() const {
    return iface_;
}

std::string CFRController::getTargetMac() const {
    return target_mac_;
}

float CFRController::getFrequencyFromIface(const std::string &iface) {
    if (iface.empty()) return 5210.0f;

    std::string cmd = "iw dev " + iface + " info 2>/dev/null";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[256];
        float found_freq = 0.0f;
        while (fgets(buf, sizeof(buf), pipe)) {
            std::string line(buf);
            size_t cpos = line.find("center1:");
            if (cpos != std::string::npos) {
                try {
                    found_freq = std::stof(line.substr(cpos + 8));
                    break;
                } catch (...) {}
            }
            size_t mpos = line.find(" MHz)");
            if (mpos != std::string::npos) {
                size_t lparen = line.rfind('(', mpos);
                if (lparen != std::string::npos && mpos > lparen + 1) {
                    try {
                        found_freq = std::stof(line.substr(lparen + 1, mpos - lparen - 1));
                    } catch (...) {}
                }
            }
        }
        pclose(pipe);
        if (found_freq > 1000.0f) {
            return found_freq;
        }
    }

    // Heuristic fallback
    if (iface.find(".1-") != std::string::npos || iface.find("2.4") != std::string::npos ||
        iface.find("2g") != std::string::npos) {
        return 2412.0f;
    } else if (iface.find("6g") != std::string::npos || iface.find("6G") != std::string::npos) {
        return 6130.0f;
    }
    return 5210.0f;
}

void CFRController::refreshFrequencyCacheLocked(const std::string &iface) const {
    std::string target_iface = iface.empty() ? iface_ : iface;
    if (target_iface.empty()) target_iface = "phy00.0-0";
    if (target_iface == cached_iface_for_freq_ && cached_center_freq_mhz_ > 1000.0f) {
        return;
    }
    cached_iface_for_freq_ = target_iface;
    cached_center_freq_mhz_ = getFrequencyFromIface(target_iface);
    if (cached_center_freq_mhz_ < 3000.0f) {
        cached_band_name_ = "2.4 GHz";
    } else if (cached_center_freq_mhz_ >= 5925.0f) {
        cached_band_name_ = "6 GHz";
    } else {
        cached_band_name_ = "5.2 GHz";
    }
}

float CFRController::getCenterFreqMHz() const {
    if (cached_iface_for_freq_ != iface_ || cached_center_freq_mhz_ <= 0.0f) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(mutex_));
        refreshFrequencyCacheLocked(iface_);
    }
    return cached_center_freq_mhz_;
}

std::string CFRController::getBandName() const {
    if (cached_iface_for_freq_ != iface_ || cached_band_name_.empty()) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(mutex_));
        refreshFrequencyCacheLocked(iface_);
    }
    return cached_band_name_;
}

bool CFRController::isTargetConnected() {
    std::lock_guard<std::mutex> lock(mutex_);
    return isTargetStationConnectedLocked();
}

bool CFRController::stopCaptureLocked() {
    DriverAuditLogger::instance().log("CFR", "STOP_BEGIN", target_mac_,
        "iface=" + iface_ + " pid=" + std::to_string(pid_));

    // =========================================================================
    // NGUYÊN TẮC BẢO VỆ DRIVER ATH12K & KERNEL KHI DỪNG:
    // DRIVER FIRST, PROCESS SECOND (PHẦN CỨNG TRƯỚC, TIẾN TRÌNH SAU)
    // =========================================================================

    // 1. Tắt cfr_capture trên station debugfs ĐẦU TIÊN để driver ath12k / mac80211
    // ngưng phát sinh yêu cầu trích xuất CSI trên các khung vô tuyến của client này.
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
        if (fs::exists(sta_cfr_path, ec)) {
            writeSysfs(sta_cfr_path, "0 0 0 0");
        }
    }

    // 2. Chờ 50ms để DMA và ngắt phần cứng của chip Wi-Fi hoàn tất các frame dở dang
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 3. Tắt enable_cfr trên card vô tuyến TIẾP THEO để tắt hoàn toàn CFR engine
    namespace fs = std::filesystem;
    std::error_code ec;
    std::string hw_enable_path = "/sys/kernel/debug/ieee80211/" + phy_ + "/" + hw_ + "/enable_cfr";
    if (fs::exists(hw_enable_path, ec)) {
        writeSysfs(hw_enable_path, "0");
    }
    if (!pci_path_.empty() && !mac_dev_.empty()) {
        std::string pci_enable_path = pci_path_ + "/" + mac_dev_ + "/enable_cfr";
        if (fs::exists(pci_enable_path, ec)) {
            writeSysfs(pci_enable_path, "0");
        }
    }

    // 4. Chờ 50ms để relayfs channel xả hết buffer còn đọng và unmap an toàn
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 5. BÂY GIỜ MỚI DỪNG TIẾN TRÌNH cfr_test_app BẰNG SIGTERM ÊM DỊU:
    // Tuyệt đối không dùng killall -9 đột ngột vì sẽ làm đứt kết nối relayfs
    // khi driver còn đang ghi, gây crash HLOS Panic.
    if (pid_ > 0) {
        DriverAuditLogger::instance().logProcess("SIGTERM", pid_, "Graceful termination requested");
        kill(pid_, SIGTERM);
        bool terminated = false;
        for (int i = 0; i < 5; ++i) {
            int p_status = 0;
            pid_t res = waitpid(pid_, &p_status, WNOHANG);
            if (res == pid_ || (kill(pid_, 0) != 0 && errno == ESRCH)) {
                terminated = true;
                DriverAuditLogger::instance().logProcess("EXITED", pid_,
                    "Process terminated gracefully with status " + std::to_string(p_status));
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!terminated && kill(pid_, 0) == 0) {
            DriverAuditLogger::instance().logProcess("SIGKILL", pid_, "Process timeout 500ms, sending SIGKILL");
            kill(pid_, SIGKILL);
            waitpid(pid_, nullptr, WNOHANG);
        }
        pid_ = -1;
    }

    // Ghi nhận file vừa thu thập
    std::string dump_path;
    size_t dump_size = 0;
    if (findNewestDumpInDir("/tmp", dump_path, dump_size)) {
        last_captured_file_ = dump_path;
        last_captured_size_ = dump_size;
    }

    running_ = false;
    DriverAuditLogger::instance().log("CFR", "STOP_COMPLETE", target_mac_,
        "last_file=" + last_captured_file_ + " size=" + std::to_string(last_captured_size_));
    std::cout << "[CFR] Capture stopped\n";
    return true;
}

CFRCaptureStatus CFRController::getStatus() {
    std::lock_guard<std::mutex> lock(mutex_);
    checkProcessAliveLocked();
    checkTargetConnectionLocked();
    checkSafetyGuardLocked();

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

    static auto last_dump_check = std::chrono::steady_clock::time_point{};
    const auto now_check = std::chrono::steady_clock::now();
    if (!running_ || (now_check - last_dump_check > std::chrono::seconds(3))) {
        last_dump_check = now_check;
        std::string dump_path;
        size_t dump_size = 0;
        if (findNewestDumpInDir("/tmp", dump_path, dump_size)) {
            last_captured_file_ = dump_path;
            last_captured_size_ = dump_size;
        }
    }

    if (!last_captured_file_.empty()) {
        namespace fs = std::filesystem;
        status.latest_file = fs::path(last_captured_file_).filename().string();
        status.latest_file_size = last_captured_size_;
    }

    status.available_bandwidths = getAvailableBandwidths(iface_, target_mac_);
    status.center_freq_mhz = getCenterFreqMHz();
    status.band_name = getBandName();

    return status;
}

std::vector<int> CFRController::getAvailableBandwidths(const std::string &iface,
                                                      const std::string &mac) const {
    std::string test_iface = iface.empty() ? (iface_.empty() ? "phy00.0-0" : iface_) : iface;
    std::string cache_key = test_iface + "_" + mac;
    auto now = std::chrono::steady_clock::now();
    if (cache_key == cached_iface_for_bw_ && !cached_available_bandwidths_.empty() &&
        std::chrono::duration_cast<std::chrono::seconds>(now - last_bw_query_time_).count() < 10) {
        return cached_available_bandwidths_;
    }

    int iface_max_bw = 0; // 0: 20MHz, 1: 40MHz, 2: 80MHz, 3: 160MHz, 4: 320MHz

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
        if (test_iface.find(".2-") != std::string::npos ||
            test_iface.find(".0-") != std::string::npos ||
            test_iface.find("5G") != std::string::npos ||
            test_iface.find("6G") != std::string::npos) {
            iface_max_bw = 2; // 80 MHz cho 5GHz/6GHz
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
    std::vector<int> final_res = result.empty() ? std::vector<int>{0} : result;
    cached_iface_for_bw_ = cache_key;
    cached_available_bandwidths_ = final_res;
    last_bw_query_time_ = now;
    return final_res;
}

std::string CFRController::getLatestCaptureFilePath(size_t *file_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    static auto last_dump_check = std::chrono::steady_clock::time_point{};
    const auto now_check = std::chrono::steady_clock::now();
    if (!running_ || (now_check - last_dump_check > std::chrono::seconds(3))) {
        last_dump_check = now_check;
        std::string dump_path;
        size_t dump_size = 0;
        if (findNewestDumpInDir("/tmp", dump_path, dump_size)) {
            last_captured_file_ = dump_path;
            last_captured_size_ = dump_size;
        }
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
