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
    std::string hw = "ath12k_hw2";
    std::string mac_dev = "mac0";
    std::string pci_path;
    int pid = -1;
    uint64_t uptime_sec = 0;
    std::string last_error;
    int bandwidth = 0;
    int period = 50;
    std::string latest_file;
    size_t latest_file_size = 0;
    std::vector<int> available_bandwidths;
    float center_freq_mhz = 5210.0f;
    std::string band_name = "5.2 GHz";
};

class CFRController {
  public:
    CFRController();
    ~CFRController();

    bool startCapture(const std::string &mac, const std::string &iface,
                      const std::string &ip = "", int period = 50,
                      int bandwidth = 0, int method = 0);
    bool updatePeriod(int period);
    bool stopCapture(const std::string &reason = "");
    bool isRunning();
    bool isCapturingLive() const;
    std::string getCaptureIface() const;
    std::string getTargetMac() const;
    float getCenterFreqMHz() const;
    std::string getBandName() const;
    bool isTargetConnected();
    CFRCaptureStatus getStatus();
    std::string getLatestCaptureFilePath(size_t *file_size = nullptr);
    std::vector<int> getAvailableBandwidths(const std::string &iface,
                                           const std::string &mac = "") const;

    static std::string findAth12kPciPath();
    static std::string getPhyFromIface(const std::string &iface);
    static std::string getHwFromIface(const std::string &iface);
    static std::string getMacDevFromIface(const std::string &iface);
    static float getFrequencyFromIface(const std::string &iface);

  private:
    std::mutex mutex_;
    bool running_ = false;
    std::string target_mac_;
    std::string target_ip_;
    std::string iface_;
    std::string phy_ = "phy00";
    std::string hw_ = "ath12k_hw2";
    std::string mac_dev_ = "mac0";
    std::string pci_path_;
    int pid_ = -1;
    std::chrono::steady_clock::time_point start_time_;
    std::string last_error_;
    int bandwidth_ = 0;
    int period_ = 50;
    int method_ = 0;
    std::string last_captured_file_;
    size_t last_captured_size_ = 0;
    int target_disconnect_count_ = 0;
    std::chrono::steady_clock::time_point last_connection_check_{};

    // Bộ nhớ đệm tham số vô tuyến để triệt tiêu hoàn toàn popen("iw") trong vòng lặp thu
    mutable float cached_center_freq_mhz_ = 5210.0f;
    mutable std::string cached_band_name_ = "5.2 GHz";
    mutable std::string cached_iface_for_freq_;
    mutable std::vector<int> cached_available_bandwidths_;
    mutable std::string cached_iface_for_bw_;
    mutable std::chrono::steady_clock::time_point last_bw_query_time_{};

    void refreshFrequencyCacheLocked(const std::string &iface) const;
    void checkProcessAliveLocked();
    void checkTargetConnectionLocked();
    void checkSafetyGuardLocked();
    bool rotateCaptureLocked();
    bool isTargetStationConnectedLocked() const;
    bool stopCaptureLocked();
    static bool writeSysfs(const std::string &path, const std::string &value);
    static void cleanupDumpFiles(size_t keep_latest = 1);
};

#endif
