#ifndef CFR_CONTROLLER_HPP
#define CFR_CONTROLLER_HPP

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct CFRCaptureStatus {
    bool running = false;
    std::string target_mac;
    std::string target_ip;
    std::string iface;
    std::string phy = "phy00";
    std::string hw = "ath12k_hw1";
    std::string mac_dev = "mac1";
    std::string pci_path;
    int pid = -1;
    uint64_t uptime_sec = 0;
    std::string last_error;
    int bandwidth = 0;
    int period = 10;
    std::string latest_file;
    size_t latest_file_size = 0;
    std::vector<int> available_bandwidths;
};

class CFRController {
  public:
    CFRController();
    ~CFRController();

    bool startCapture(const std::string &mac, const std::string &iface,
                      const std::string &ip = "", int period = 10,
                      int bandwidth = 0, int method = 0);
    bool stopCapture();
    CFRCaptureStatus getStatus();
    std::string getLatestCaptureFilePath(size_t *file_size = nullptr);
    std::vector<int> getAvailableBandwidths(const std::string &iface,
                                           const std::string &mac = "") const;

    static std::string findAth12kPciPath();
    static std::string getPhyFromIface(const std::string &iface);
    static std::string getHwFromIface(const std::string &iface);
    static std::string getMacDevFromIface(const std::string &iface);

  private:
    std::mutex mutex_;
    bool running_ = false;
    std::string target_mac_;
    std::string target_ip_;
    std::string iface_;
    std::string phy_ = "phy00";
    std::string hw_ = "ath12k_hw1";
    std::string mac_dev_ = "mac1";
    std::string pci_path_;
    int pid_ = -1;
    std::chrono::steady_clock::time_point start_time_;
    std::string last_error_;
    int bandwidth_ = 0;
    int period_ = 10;
    std::string last_captured_file_;
    size_t last_captured_size_ = 0;

    void checkProcessAliveLocked();
    bool stopCaptureLocked();
    static bool writeSysfs(const std::string &path, const std::string &value);
    static void cleanupDumpFiles(size_t keep_latest = 1);
};

#endif
