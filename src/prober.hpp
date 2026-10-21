#ifndef PROBER_HPP
#define PROBER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct WifiCardInfo {
    std::string iface;        // Ví dụ: phy00.0-0
    std::string ssid;         // Ví dụ: 10. VNPT Wifi 8 5GHz
    std::string band;         // Ví dụ: 5GHz hoặc 2.4GHz
    int station_count = 0;
    int max_bandwidth = 0;    // 0: 20MHz, 1: 40MHz, 2: 80MHz, 3: 160MHz, 4: 320MHz
    std::string width_str = "20 MHz";
    std::vector<int> supported_bandwidths = {0};
};

struct ClientInfo {
    std::string ip;
    std::string mac;          // Chuẩn hóa chữ hoa (ví dụ: 6E:CD:00:06:8F:2C)
    std::string hostname;     // Tên thiết bị (ví dụ: realme-6i)
    std::string iface;        // phy00.1-0
    std::string ssid;         // Tên mạng Wi-Fi (SSID)
    std::string band;         // 5GHz hoặc 2.4GHz
    std::string signal_dbm;   // -99 dBm
    std::string snr;          // 99
    std::string bitrate;      // 96.3 MBit/s
    std::string inactive_ms;  // 180 ms
    bool is_wireless = true;
    int max_bandwidth = 0;
    std::string width_str = "20 MHz";
    std::vector<int> supported_bandwidths = {0};
};

struct ProberStatus {
    bool enabled = false;
    std::string mode = "single"; // "single" hoặc "multi"
    std::string target_ip;       // Cho single mode
    std::vector<std::string> target_ips; // Cho multi mode
    int interval_ms = 50;
    std::string interface_name;
    uint64_t packets_sent = 0;
    bool active = false;
    std::vector<WifiCardInfo> cards;
    std::vector<ClientInfo> clients;
};

class ActiveProber {
  public:
    ActiveProber(std::string interface_name = "br-lan");
    ~ActiveProber();

    bool start();
    void stop();

    void setConfig(bool enabled, const std::string &mode,
                   const std::vector<std::string> &target_ips, int interval_ms);
    ProberStatus getStatus(bool scan_network = false);
    void scanNetwork(std::vector<WifiCardInfo> &cards, std::vector<ClientInfo> &clients);
    std::vector<ClientInfo> scanClients();

  private:
    std::string interface_name_;
    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;

    std::mutex scan_cache_mutex_;
    std::vector<WifiCardInfo> cached_cards_;
    std::vector<ClientInfo> cached_clients_;
    std::chrono::steady_clock::time_point last_scan_time_{};

    std::mutex mutex_;
    std::condition_variable cv_;
    bool enabled_ = false;
    std::string mode_ = "single";
    std::vector<std::string> target_ips_;
    int interval_ms_ = 50;
    uint64_t packets_sent_ = 0;

    int udp_fd_ = -1;
    int raw_arp_fd_ = -1;
    int ifindex_ = 0;
    uint8_t src_mac_[6]{};
    uint32_t src_ip_ = 0;

    void run();
    bool initSockets();
    void closeSockets();
    bool sendUdpProbe(const std::string &ip);
    bool sendArpProbe(const std::string &ip);
    std::vector<std::string> getActiveTargets();
};

#endif // PROBER_HPP
