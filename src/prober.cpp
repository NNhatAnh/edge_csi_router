#include "prober.hpp"
#include "driver_audit_logger.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#endif

namespace {
#pragma pack(push, 1)
struct ArpPacket {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_size;
    uint8_t proto_size;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
};

struct IcmpEchoPacket {
    uint8_t type;        // 8 = ICMP_ECHO
    uint8_t code;        // 0
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
    char payload[32];
};
#pragma pack(pop)

uint16_t calculateIcmpChecksum(const void *data, size_t len) {
    const uint16_t *buf = static_cast<const uint16_t *>(data);
    uint32_t sum = 0;
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    if (len == 1) {
        sum += *reinterpret_cast<const uint8_t *>(buf);
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

std::string normalizeMacLower(std::string mac) {
    std::transform(mac.begin(), mac.end(), mac.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return mac;
}

std::string normalizeMacUpper(std::string mac) {
    std::transform(mac.begin(), mac.end(), mac.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return mac;
}

bool isValidMac(const std::string &mac) {
    if (mac.size() != 17) return false;
    int colons = 0;
    for (size_t i = 0; i < mac.size(); ++i) {
        if (i % 3 == 2) {
            if (mac[i] != ':') return false;
            colons++;
        } else {
            if (!std::isxdigit(static_cast<unsigned char>(mac[i]))) return false;
        }
    }
    return colons == 5;
}

bool isValidIpv4(const std::string &ip) {
    if (ip.find(':') != std::string::npos) return false; // Ignore IPv6
    int dots = 0;
    for (char c : ip) {
        if (c == '.') dots++;
        else if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return dots == 3 && ip != "0.0.0.0";
}

[[maybe_unused]] std::string trim(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n'))
        s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
        s.pop_back();
    return s;
}
} // namespace

ActiveProber::ActiveProber(std::string interface_name)
    : interface_name_(std::move(interface_name)) {}

ActiveProber::~ActiveProber() { stop(); }

bool ActiveProber::start() {
    if (is_running_)
        return true;
    is_running_ = true;
    worker_thread_ = std::thread(&ActiveProber::run, this);
    return true;
}

void ActiveProber::stop() {
    if (!is_running_)
        return;
    is_running_ = false;
    cv_.notify_all();
    if (worker_thread_.joinable())
        worker_thread_.join();
    closeSockets();
}

void ActiveProber::setEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enabled;
    if (!enabled_) {
        warmup_counter_ = 0;
    }
    std::cout << "[Prober] State updated: " << (enabled_ ? "ENABLED" : "DISABLED (stopped completely)") << '\n';
    cv_.notify_all();
}

void ActiveProber::setConfig(bool enabled, const std::string &target_ip,
                             int interval_ms,
                             const std::string &target_mac_hint) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool was_enabled = enabled_;
    bool target_changed = (target_ip_ != target_ip);
    enabled_ = enabled;
    target_ip_ = target_ip;
    if (!target_mac_hint.empty()) {
        target_mac_hint_ = target_mac_hint;
    }
    interval_ms_ = std::max(10, std::min(interval_ms, 2000));

    // Kích hoạt chu kỳ nạp nhanh (Fast Warmup Burst) khi bật thu hoặc đổi mục tiêu
    if (enabled_ && (!was_enabled || target_changed)) {
        warmup_counter_ = 10; // 10 gói đầu gửi siêu tốc 15ms để nạp nhanh cửa sổ 40 gói
    }

    DriverAuditLogger::instance().log("PROBER", enabled_ ? "ENABLE" : "DISABLE", target_ip_,
        "interval_ms=" + std::to_string(interval_ms_) + " hint_mac=" + target_mac_hint_);

    std::cout << "[Prober] Updated: " << (enabled_ ? "ENABLED" : "DISABLED")
              << " | Target: " << (target_ip_.empty() ? "auto" : target_ip_)
              << " | Hint MAC: " << (target_mac_hint_.empty() ? "N/A" : target_mac_hint_)
              << " | Fast Warmup: " << warmup_counter_.load()
              << " | Interval: " << interval_ms_ << " ms ("
              << (1000 / interval_ms_) << " Hz)\n";
    cv_.notify_all();
}

void ActiveProber::setConfig(bool enabled, const std::string &/*mode*/,
                             const std::vector<std::string> &target_ips,
                             int interval_ms,
                             const std::string &target_mac_hint) {
    std::string ip = target_ips.empty() ? "" : target_ips.front();
    setConfig(enabled, ip, interval_ms, target_mac_hint);
}

void ActiveProber::triggerBurst(const std::string &ip, const std::string &target_mac_hint, int count) {
    if (ip.empty() || count <= 0) return;
    if (udp_fd_ < 0 && raw_arp_fd_ < 0 && icmp_fd_ < 0) {
        initSockets();
    }
    std::string hint = target_mac_hint;
    if (hint.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        hint = target_mac_hint_;
    }
    uint64_t sent_count = 0;
    for (int i = 0; i < count; ++i) {
        bool sent_icmp = sendIcmpProbe(ip);
        if (sent_icmp) {
            ++sent_count;
        }
        bool sent_arp = sendArpProbe(ip, hint);
        if (sent_arp && !sent_icmp) {
            ++sent_count;
        }
        // Gửi xen kẽ gói UDP 512 bytes nếu cần kích thích hàng đợi QoS
        if (udp_fd_ >= 0 && (i % 2 == 1 || (!sent_icmp && !sent_arp))) {
            if (sendUdpProbe(ip)) {
                ++sent_count;
            }
        }
        if (i + 1 < count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    if (sent_count > 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        packets_sent_ += sent_count;
    }
    DriverAuditLogger::instance().log("PROBER", "BURST", ip,
        "sent=" + std::to_string(sent_count) + "/" + std::to_string(count) + " hint_mac=" + hint);
    std::cout << "[Prober] Burst pre-warm sent " << sent_count << "/" << count
              << " packets to " << ip << " (MAC: " << (hint.empty() ? "N/A" : hint) << ")\n";
}

ProberStatus ActiveProber::getStatus(bool scan_network) {
    ProberStatus status;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status.enabled = enabled_;
        status.mode = "single";
        status.target_ip = target_ip_;
        if (!target_ip_.empty()) {
            status.target_ips = {target_ip_};
        }
        status.interval_ms = interval_ms_;
        status.interface_name = interface_name_;
        status.packets_sent = packets_sent_;
        status.active = enabled_ && is_running_;
    }

    const auto now = std::chrono::steady_clock::now();
    if (!scan_network) {
        std::lock_guard<std::mutex> lock(scan_cache_mutex_);
        if (!cached_cards_.empty() &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_scan_time_).count() < 6000) {
            status.cards = cached_cards_;
            status.clients = cached_clients_;
            return status;
        }
    }

    std::vector<WifiCardInfo> new_cards;
    std::vector<ClientInfo> new_clients;
    scanNetwork(new_cards, new_clients);

    {
        std::lock_guard<std::mutex> lock(scan_cache_mutex_);
        cached_cards_ = std::move(new_cards);
        cached_clients_ = std::move(new_clients);
        last_scan_time_ = now;
        status.cards = cached_cards_;
        status.clients = cached_clients_;
    }

    return status;
}

void ActiveProber::scanNetwork(std::vector<WifiCardInfo> &cards,
                               std::vector<ClientInfo> &clients) {
    cards.clear();
    clients.clear();

    std::unordered_map<std::string, std::pair<std::string, std::string>> dhcp_map; // lowercase mac -> {ip, hostname}
    std::unordered_set<std::string> seen_macs; // uppercase macs
    std::unordered_set<std::string> seen_ips;

    // 1. Đọc bảng cấp phát DHCP (/tmp/dhcp.leases trên OpenWrt)
    std::ifstream dhcp_leases("/tmp/dhcp.leases");
    if (!dhcp_leases.is_open()) {
        dhcp_leases.open("/var/lib/misc/dnsmasq.leases");
    }
    if (dhcp_leases.is_open()) {
        std::string line;
        while (std::getline(dhcp_leases, line)) {
            std::istringstream iss(line);
            std::string expiry, mac, ip, hostname;
            if (iss >> expiry >> mac >> ip >> hostname) {
                if (isValidMac(mac) && isValidIpv4(ip)) {
                    if (hostname == "*") {
                        hostname = "Client-" + ip.substr(ip.rfind('.') + 1);
                    }
                    dhcp_map[normalizeMacLower(mac)] = {ip, hostname};
                }
            }
        }
    }

    // 2. Đọc bảng ARP kernel (/proc/net/arp) bổ sung cho thiết bị IP tĩnh
    std::ifstream arp_file("/proc/net/arp");
    if (arp_file.is_open()) {
        std::string line;
        std::getline(arp_file, line); // bỏ header
        while (std::getline(arp_file, line)) {
            std::istringstream iss(line);
            std::string ip, hw_type, flags, mac, mask, dev;
            if (iss >> ip >> hw_type >> flags >> mac >> mask >> dev) {
                if (isValidMac(mac) && isValidIpv4(ip) && mac != "00:00:00:00:00:00") {
                    std::string norm = normalizeMacLower(mac);
                    if (dhcp_map.find(norm) == dhcp_map.end()) {
                        dhcp_map[norm] = {ip, "Client-" + ip.substr(ip.rfind('.') + 1)};
                    }
                }
            }
        }
    }

    // 3. Quét các Card Wi-Fi và Station kết nối qua hostapd_cli & hostapd ubus (Userspace AP daemon)
    // An toàn tuyệt đối: 100% userspace IPC qua Unix domain socket, KHÔNG truy vấn Netlink nl80211 vào driver ath12k,
    // KHÔNG chạm vào debugfs dentry nên TUYỆT ĐỐI không gây lock contention hay kernel panic.
    std::string cmd =
        "for h in $(ubus list 'hostapd.*' 2>/dev/null); do "
        "  dev=$(echo \"$h\" | sed 's/^hostapd\\.//'); "
        "  echo \"===AP:$dev===\"; "
        "  ubus -t 2 call \"$h\" get_status 2>/dev/null; "
        "  echo \"===STA:$dev===\"; "
        "  hostapd_cli -p /var/run/hostapd -i \"$dev\" all_sta 2>/dev/null; "
        "done";

    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[1024];
        std::string cur_card;
        std::string cur_ssid = "Wi-Fi Network";
        std::string cur_band = "5GHz";
        int cur_max_bw = 2; // 80 MHz mặc định cho 5GHz
        std::string cur_width_str = "80 MHz";
        std::vector<int> cur_supp_bw = {0, 1, 2};
        bool in_clients_section = false;
        ClientInfo cur_client;

        int cur_ack_sig = 0;
        int cur_max_rssi = 0;
        int cur_min_rssi = 0;
        int cur_mgmt_sig = 0;

        auto flushCurrentClient = [&]() {
            if (!cur_client.mac.empty()) {
                std::string upper_mac = normalizeMacUpper(cur_client.mac);
                if (seen_macs.find(upper_mac) == seen_macs.end()) {
                    cur_client.mac = upper_mac;
                    cur_client.iface = cur_card;
                    cur_client.ssid = cur_ssid;
                    cur_client.band = cur_band;
                    cur_client.is_wireless = true;

                    // Ghép nối IP & Hostname từ bảng DHCP / ARP
                    std::string lower_mac = normalizeMacLower(upper_mac);
                    if (dhcp_map.find(lower_mac) != dhcp_map.end()) {
                        cur_client.ip = dhcp_map[lower_mac].first;
                        cur_client.hostname = dhcp_map[lower_mac].second;
                    } else {
                        cur_client.ip = "";
                        cur_client.hostname = "Wi-Fi Station";
                    }

                    // Xác định mức RSSI thực tế (dBm) chính xác nhất từ hostapd
                    // Ưu tiên cur_max_rssi từ anten thu tốt nhất (tránh suy hao do lệch phân cực anten)
                    int best_rssi = 0;
                    if (cur_max_rssi < 0) {
                        best_rssi = cur_max_rssi;
                    } else if (cur_ack_sig < 0 && cur_ack_sig > -100) {
                        best_rssi = cur_ack_sig;
                    } else if (cur_min_rssi < 0) {
                        best_rssi = cur_min_rssi;
                    } else if (cur_mgmt_sig < 0) {
                        best_rssi = cur_mgmt_sig;
                    }

                    if (best_rssi < 0) {
                        cur_client.signal_dbm = std::to_string(best_rssi) + " dBm";
                        cur_client.snr = std::to_string(std::max(1, 95 + best_rssi));
                    } else if (cur_client.signal_dbm.empty()) {
                        cur_client.signal_dbm = "Online";
                        cur_client.snr = "Online";
                    }

                    if (cur_client.bitrate.empty()) {
                        cur_client.bitrate = "Active";
                    }
                    if (cur_client.inactive_ms.empty()) {
                        cur_client.inactive_ms = "Active";
                    }

                    seen_macs.insert(upper_mac);
                    if (!cur_client.ip.empty()) {
                        seen_ips.insert(cur_client.ip);
                    }
                    clients.push_back(cur_client);
                }
            }
            cur_client = ClientInfo{};
            cur_ack_sig = 0;
            cur_max_rssi = 0;
            cur_min_rssi = 0;
            cur_mgmt_sig = 0;
        };

        auto flushCurrentCard = [&]() {
            flushCurrentClient();
            if (!cur_card.empty()) {
                WifiCardInfo card;
                card.iface = cur_card;
                card.ssid = cur_ssid;
                card.band = cur_band;
                card.max_bandwidth = cur_max_bw;
                card.width_str = cur_width_str;
                card.supported_bandwidths = cur_supp_bw;
                card.station_count = 0;
                for (const auto &c : clients) {
                    if (c.iface == cur_card) card.station_count++;
                }
                cards.push_back(card);
            }
        };

        while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();

            if (line.rfind("===AP:", 0) == 0) {
                flushCurrentCard();
                in_clients_section = false;
                size_t end = line.rfind("===");
                if (end != std::string::npos && end > 6) {
                    cur_card = line.substr(6, end - 6);
                } else {
                    cur_card = "phy00.0-0";
                }
                cur_ssid = "Wi-Fi Network";
                if (cur_card.find(".1-") != std::string::npos || cur_card.find("2g") != std::string::npos) {
                    cur_band = "2.4GHz";
                    cur_max_bw = 0;
                    cur_width_str = "20 MHz";
                    cur_supp_bw = {0};
                } else {
                    cur_band = "5GHz";
                    cur_max_bw = 2;
                    cur_width_str = "80 MHz";
                    cur_supp_bw = {0, 1, 2};
                }
            } else if (line.rfind("===STA:", 0) == 0) {
                in_clients_section = true;
            } else if (!in_clients_section) {
                size_t ssid_pos = line.find("\"ssid\": \"");
                if (ssid_pos != std::string::npos) {
                    size_t start = ssid_pos + 9;
                    size_t end = line.find('\"', start);
                    if (end != std::string::npos) {
                        cur_ssid = line.substr(start, end - start);
                    }
                }
                size_t freq_pos = line.find("\"freq\": ");
                if (freq_pos != std::string::npos) {
                    try {
                        int freq = std::stoi(line.substr(freq_pos + 8));
                        if (freq >= 5925) {
                            cur_band = "6GHz";
                            cur_max_bw = 4;
                            cur_width_str = "320 MHz";
                            cur_supp_bw = {0, 1, 2, 3, 4};
                        } else if (freq >= 4900) {
                            cur_band = "5GHz";
                            cur_max_bw = 2;
                            cur_width_str = "80 MHz";
                            cur_supp_bw = {0, 1, 2};
                        } else {
                            cur_band = "2.4GHz";
                            cur_max_bw = 0;
                            cur_width_str = "20 MHz";
                            cur_supp_bw = {0};
                        }
                    } catch (...) {}
                }
            } else {
                if (isValidMac(line)) {
                    flushCurrentClient();
                    cur_client.mac = line;
                    cur_client.max_bandwidth = cur_max_bw;
                    cur_client.width_str = cur_width_str;
                    cur_client.supported_bandwidths = cur_supp_bw;
                    continue;
                }

                if (!cur_client.mac.empty()) {
                    if (line.rfind("flags=", 0) == 0) {
                        if (line.find("[EHT]") != std::string::npos) {
                            cur_client.max_bandwidth = cur_max_bw;
                            cur_client.width_str = cur_width_str;
                            cur_client.supported_bandwidths = cur_supp_bw;
                        } else if (line.find("[HE]") != std::string::npos) {
                            cur_client.max_bandwidth = cur_max_bw;
                            cur_client.width_str = cur_width_str;
                            cur_client.supported_bandwidths = cur_supp_bw;
                        } else if (line.find("[VHT]") != std::string::npos && cur_max_bw >= 2) {
                            cur_client.max_bandwidth = 2;
                            cur_client.width_str = "80 MHz";
                            cur_client.supported_bandwidths = {0, 1, 2};
                        } else if (line.find("[HT]") != std::string::npos && cur_client.max_bandwidth == 0) {
                            cur_client.max_bandwidth = (cur_band == "5GHz") ? 1 : 0;
                            cur_client.width_str = (cur_band == "5GHz") ? "40 MHz" : "20 MHz";
                            cur_client.supported_bandwidths = (cur_band == "5GHz") ? std::vector<int>{0, 1} : std::vector<int>{0};
                        }
                    } else if (line.rfind("last_ack_signal=", 0) == 0) {
                        try { cur_ack_sig = std::stoi(line.substr(16)); } catch (...) {}
                    } else if (line.rfind("max_rssi=", 0) == 0) {
                        try { cur_max_rssi = std::stoi(line.substr(9)); } catch (...) {}
                    } else if (line.rfind("min_rssi=", 0) == 0) {
                        try { cur_min_rssi = std::stoi(line.substr(9)); } catch (...) {}
                    } else if (line.rfind("mgmt_signal=", 0) == 0) {
                        try { cur_mgmt_sig = std::stoi(line.substr(12)); } catch (...) {}
                    } else if (line.rfind("inactive_msec=", 0) == 0) {
                        try {
                            uint64_t ms = std::stoull(line.substr(14));
                            cur_client.inactive_ms = (ms < 1000) ? "0 ms" : std::to_string(ms / 1000) + "s";
                        } catch (...) {}
                    } else if (line.rfind("tx_rate_info=", 0) == 0 && cur_client.bitrate.empty()) {
                        std::string raw = line.substr(13);
                        size_t sp = raw.find(' ');
                        std::string num_str = (sp != std::string::npos) ? raw.substr(0, sp) : raw;
                        try {
                            int r = std::stoi(num_str);
                            if (r > 0) {
                                float mbps = static_cast<float>(r) / 10.0f;
                                std::ostringstream oss;
                                oss << std::fixed << std::setprecision(1) << mbps << " MBit/s";
                                cur_client.bitrate = oss.str();
                            }
                        } catch (...) {}
                    } else if (line.rfind("maxphyrate=", 0) == 0 && cur_client.bitrate.empty()) {
                        try {
                            uint64_t kbps = std::stoull(line.substr(11));
                            if (kbps > 0) {
                                float mbps = static_cast<float>(kbps) / 1000.0f;
                                std::ostringstream oss;
                                oss << std::fixed << std::setprecision(1) << mbps << " MBit/s";
                                cur_client.bitrate = oss.str();
                            }
                        } catch (...) {}
                    }
                }
            }
        }
        flushCurrentCard();
        pclose(pipe);
    }

    // 4. Nếu không phát hiện card hoặc trạm nào từ hostapd (ví dụ khi khởi động hoặc chạy kiểm thử):
    // Giữ danh sách cards và clients rỗng, tuyệt đối KHÔNG tự ý giả lập trạm ma từ lease cũ
    // để tránh 100% tình trạng chọn nhầm thiết bị offline làm tê liệt thu thập CFR.
}

std::vector<ClientInfo> ActiveProber::scanClients() {
    std::vector<WifiCardInfo> dummy_cards;
    std::vector<ClientInfo> clients;
    scanNetwork(dummy_cards, clients);
    return clients;
}

std::vector<std::string> ActiveProber::getActiveTargets() {
    std::vector<std::string> targets;

    if (!target_ip_.empty() && target_ip_ != "auto") {
        targets.push_back(target_ip_);
        return targets;
    }

    // Tự động tìm thiết bị Wi-Fi có tín hiệu tốt nhất: CHỈ lấy từ cached_clients_
    std::vector<ClientInfo> clients;
    {
        std::lock_guard<std::mutex> lock(scan_cache_mutex_);
        clients = cached_clients_;
    }

    std::string best_ip;
    int best_score = -999;
    for (const auto &c : clients) {
        if (c.ip.empty() || c.ip == "127.0.0.1" || c.ip == "N/A") continue;
        if (!c.is_wireless) continue;

        int score = -100;
        if (!c.snr.empty() && c.snr != "—") {
            try {
                score = std::stoi(c.snr);
            } catch (...) {}
        } else if (!c.signal_dbm.empty() && c.signal_dbm != "—") {
            try {
                score = std::stoi(c.signal_dbm);
            } catch (...) {}
        }
        // Ưu tiên trạm 5GHz hoặc 6GHz vì hỗ trợ băng thông lớn và model cử chỉ
        if (c.band == "5GHz" || c.band == "6GHz" || c.iface.find(".0-") != std::string::npos || c.iface.find(".2-") != std::string::npos) {
            score += 50;
        }
        // Ưu tiên trạm vừa có hoạt động
        if (c.inactive_ms == "0 ms" || c.inactive_ms == "Active") {
            score += 20;
        }

        if (score > best_score || best_ip.empty()) {
            best_score = score;
            best_ip = c.ip;
        }
    }
    if (!best_ip.empty()) {
        targets.push_back(best_ip);
    }
    return targets;
}

bool ActiveProber::initSockets() {
    udp_fd_ = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (udp_fd_ >= 0) {
        int broadcast = 1;
        setsockopt(udp_fd_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    }

#ifdef __linux__
    // 1. Khởi tạo ICMP Raw Socket (IPPROTO_ICMP) để gửi ICMP Echo Request (Ping chuẩn)
    icmp_fd_ = socket(AF_INET, SOCK_RAW | SOCK_NONBLOCK, IPPROTO_ICMP);
    if (icmp_fd_ < 0) {
        // Fallback thử SOCK_DGRAM IPPROTO_ICMP
        icmp_fd_ = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_ICMP);
    }
    if (icmp_fd_ >= 0) {
        struct timeval snd_tv{};
        snd_tv.tv_sec = 0;
        snd_tv.tv_usec = 20000; // 20ms timeout
        setsockopt(icmp_fd_, SOL_SOCKET, SO_SNDTIMEO, (const char *)&snd_tv, sizeof(snd_tv));
    } else {
        std::cerr << "[Prober] ICMP raw socket not available: " << std::strerror(errno) << '\n';
    }

    // 2. Khởi tạo Raw ARP Socket có cờ NONBLOCK và timeout chống nghẽn Send-Q
    raw_arp_fd_ = socket(AF_PACKET, SOCK_RAW | SOCK_NONBLOCK, htons(ETH_P_ARP));
    if (raw_arp_fd_ >= 0) {
        struct timeval snd_tv{};
        snd_tv.tv_sec = 0;
        snd_tv.tv_usec = 20000;
        setsockopt(raw_arp_fd_, SOL_SOCKET, SO_SNDTIMEO, (const char *)&snd_tv, sizeof(snd_tv));

        struct ifreq ifr {};
        std::strncpy(ifr.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
        if (ioctl(raw_arp_fd_, SIOCGIFINDEX, &ifr) == 0) {
            ifindex_ = ifr.ifr_ifindex;
        } else {
            std::cerr << "[Prober] Cannot get ifindex for " << interface_name_
                      << ": " << std::strerror(errno) << '\n';
        }
        if (ioctl(raw_arp_fd_, SIOCGIFHWADDR, &ifr) == 0) {
            std::memcpy(src_mac_, ifr.ifr_hwaddr.sa_data, 6);
        }
        if (ioctl(raw_arp_fd_, SIOCGIFADDR, &ifr) == 0) {
            src_ip_ = reinterpret_cast<sockaddr_in *>(&ifr.ifr_addr)->sin_addr.s_addr;
        }
    } else {
        std::cerr << "[Prober] Raw ARP socket not available: "
                  << std::strerror(errno) << ", using UDP/ICMP probing\n";
    }
#endif

    return (icmp_fd_ >= 0 || udp_fd_ >= 0 || raw_arp_fd_ >= 0);
}

void ActiveProber::closeSockets() {
    if (icmp_fd_ >= 0) {
        close(icmp_fd_);
        icmp_fd_ = -1;
    }
    if (udp_fd_ >= 0) {
        close(udp_fd_);
        udp_fd_ = -1;
    }
    if (raw_arp_fd_ >= 0) {
        close(raw_arp_fd_);
        raw_arp_fd_ = -1;
    }
}

bool ActiveProber::sendIcmpProbe(const std::string &ip) {
    if (icmp_fd_ < 0 || ip.empty())
        return false;

    sockaddr_in target{};
    target.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip.c_str(), &target.sin_addr) <= 0)
        return false;

    IcmpEchoPacket packet{};
    packet.type = 8; // ICMP_ECHO
    packet.code = 0;
    packet.id = htons(static_cast<uint16_t>(getpid() & 0xFFFF));
    packet.sequence = htons(++icmp_seq_);
    std::memcpy(packet.payload, "EDGECSI_M5R_WAKE", 16);
    packet.checksum = 0;
    packet.checksum = calculateIcmpChecksum(&packet, sizeof(packet));

    ssize_t sent = sendto(icmp_fd_, &packet, sizeof(packet), 0,
                          reinterpret_cast<sockaddr *>(&target), sizeof(target));
    return (sent > 0);
}

bool ActiveProber::sendUdpProbe(const std::string &ip) {
    if (udp_fd_ < 0 || ip.empty())
        return false;

    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(9999);
    if (inet_pton(AF_INET, ip.c_str(), &target.sin_addr) <= 0)
        return false;

    static char payload[512];
    static bool payload_init = false;
    if (!payload_init) {
        std::memset(payload, 'X', sizeof(payload));
        std::memcpy(payload, "CSI_PING_PROBE_RATE_WAKEUP", 26);
        payload_init = true;
    }
    ssize_t sent = sendto(udp_fd_, payload, sizeof(payload), 0,
                          reinterpret_cast<sockaddr *>(&target), sizeof(target));
    return (sent > 0);
}

std::string ActiveProber::findMacForIp(const std::string &ip) {
    if (ip.empty())
        return "";

    // 1. Tìm trong cached_clients_
    {
        std::lock_guard<std::mutex> lock(scan_cache_mutex_);
        for (const auto &c : cached_clients_) {
            if (c.ip == ip && !c.mac.empty()) {
                return c.mac;
            }
        }
    }

    // 2. Tra cứu từ /proc/net/arp
    std::ifstream arp_file("/proc/net/arp");
    if (arp_file.is_open()) {
        std::string line;
        std::getline(arp_file, line); // Bỏ qua dòng tiêu đề
        while (std::getline(arp_file, line)) {
            std::istringstream iss(line);
            std::string entry_ip, hw_type, flags, mac;
            if (iss >> entry_ip >> hw_type >> flags >> mac) {
                if (entry_ip == ip && isValidMac(mac)) {
                    return mac;
                }
            }
        }
    }
    return "";
}

std::string ActiveProber::findIpForMac(const std::string &mac) {
    if (mac.empty())
        return "";
    const std::string lower = normalizeMacLower(mac);
    const std::string upper = normalizeMacUpper(mac);

    // 1. Tìm trong cached_clients_
    {
        std::lock_guard<std::mutex> lock(scan_cache_mutex_);
        for (const auto &c : cached_clients_) {
            if ((c.mac == upper || c.mac == lower) && !c.ip.empty() && c.ip != "N/A") {
                return c.ip;
            }
        }
    }

    // 2. Tra cứu từ /proc/net/arp
    std::ifstream arp_file("/proc/net/arp");
    if (arp_file.is_open()) {
        std::string line;
        std::getline(arp_file, line);
        while (std::getline(arp_file, line)) {
            std::istringstream iss(line);
            std::string entry_ip, hw_type, flags, entry_mac;
            if (iss >> entry_ip >> hw_type >> flags >> entry_mac) {
                if ((normalizeMacLower(entry_mac) == lower) && isValidIpv4(entry_ip)) {
                    return entry_ip;
                }
            }
        }
    }

    // 3. Tra cứu từ /tmp/dhcp.leases
    std::ifstream dhcp_leases("/tmp/dhcp.leases");
    if (!dhcp_leases.is_open()) {
        dhcp_leases.open("/var/lib/misc/dnsmasq.leases");
    }
    if (dhcp_leases.is_open()) {
        std::string line;
        while (std::getline(dhcp_leases, line)) {
            std::istringstream iss(line);
            std::string expiry, entry_mac, entry_ip;
            if (iss >> expiry >> entry_mac >> entry_ip) {
                if (normalizeMacLower(entry_mac) == lower && isValidIpv4(entry_ip)) {
                    return entry_ip;
                }
            }
        }
    }

    return "";
}

bool ActiveProber::sendArpProbe(const std::string &ip, const std::string &hint_mac) {
#ifdef __linux__
    if (raw_arp_fd_ < 0 || ifindex_ <= 0 || ip.empty())
        return false;

    uint32_t target_ip = 0;
    if (inet_pton(AF_INET, ip.c_str(), &target_ip) <= 0)
        return false;

    std::string mac_str = hint_mac;
    if (mac_str.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!target_mac_hint_.empty() && (target_ip_ == ip || target_ip_.empty())) {
            mac_str = target_mac_hint_;
        }
    }
    if (mac_str.empty()) {
        mac_str = findMacForIp(ip);
    }

    uint8_t dst_mac[6];
    bool is_unicast = false;
    if (!mac_str.empty()) {
        unsigned int b[6];
        if (std::sscanf(mac_str.c_str(), "%x:%x:%x:%x:%x:%x",
                        &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
            for (int i = 0; i < 6; ++i) {
                dst_mac[i] = static_cast<uint8_t>(b[i]);
            }
            is_unicast = true;
        }
    }
    if (!is_unicast) {
        std::memset(dst_mac, 0xFF, 6);
    }

    ArpPacket packet{};
    std::memcpy(packet.dst_mac, dst_mac, 6);
    std::memcpy(packet.src_mac, src_mac_, 6);
    packet.ethertype = htons(ETH_P_ARP);

    packet.hw_type = htons(1);           // Ethernet
    packet.proto_type = htons(ETH_P_IP); // IPv4
    packet.hw_size = 6;
    packet.proto_size = 4;
    packet.opcode = htons(ARPOP_REQUEST);
    std::memcpy(packet.sender_mac, src_mac_, 6);
    packet.sender_ip = src_ip_;
    if (is_unicast) {
        std::memcpy(packet.target_mac, dst_mac, 6);
    } else {
        std::memset(packet.target_mac, 0x00, 6);
    }
    packet.target_ip = target_ip;

    sockaddr_ll sll{};
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifindex_;
    sll.sll_protocol = htons(ETH_P_ARP);
    sll.sll_halen = 6;
    std::memcpy(sll.sll_addr, dst_mac, 6);

    ssize_t sent = sendto(raw_arp_fd_, &packet, sizeof(packet), 0,
                          reinterpret_cast<sockaddr *>(&sll), sizeof(sll));
    return (sent > 0);
#else
    (void)ip;
    (void)hint_mac;
    return false;
#endif
}

void ActiveProber::run() {
    initSockets();

    while (is_running_) {
        std::vector<std::string> targets;
        int interval = 50;
        bool should_probe = false;
        std::string target_mac_hint;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (!enabled_ || interval_ms_ <= 0) {
                cv_.wait(lock, [this] {
                    return !is_running_ || (enabled_ && interval_ms_ > 0);
                });
                if (!is_running_)
                    break;
            }

            targets = getActiveTargets();
            target_mac_hint = target_mac_hint_;
            interval = std::max(10, std::min(interval_ms_, 2000));
            should_probe = enabled_ && !targets.empty();
        }

        int sleep_ms = interval;
        int cur_warmup = warmup_counter_.load();
        if (cur_warmup > 0) {
            // Tăng tốc độ khởi động để nạp đầy 40 gói trong < 0.4s (10ms/gói)
            sleep_ms = std::min(sleep_ms, 10);
            warmup_counter_.fetch_sub(1);
        }

        static uint32_t probe_cycle = 0;
        if (should_probe) {
            ++probe_cycle;
            uint64_t sent_count = 0;
            for (const auto &target : targets) {
                bool sent = false;
                // 1. Ưu tiên cao nhất: Gửi gói ICMP Echo Request (Ping chuẩn)
                // Buộc kernel điện thoại (Android/iOS) phải lập tức trả lời bằng gói tin ICMP Echo Reply (802.11 Data frame) ở tần số 100 Hz
                if (icmp_fd_ >= 0) {
                    sent = sendIcmpProbe(target);
                }

                // 2. Gửi ARP định kỳ (~1 giây / lần) hoặc khi ICMP ping thất bại để duy trì bảng ARP L2
                if (!sent || probe_cycle % 50 == 0) {
                    if (sendArpProbe(target, (targets.size() == 1 ? target_mac_hint : ""))) {
                        sent = true;
                    }
                }

                // 3. Dự phòng UDP 512 bytes nếu cần kích thích hàng đợi QoS
                if (udp_fd_ >= 0 && (!sent || probe_cycle % 100 == 0)) {
                    if (sendUdpProbe(target)) {
                        sent = true;
                    }
                }
                if (sent)
                    ++sent_count;
            }
            if (sent_count > 0) {
                std::lock_guard<std::mutex> lock(mutex_);
                packets_sent_ += sent_count;
            }
        }

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(sleep_ms),
                         [this] { return !is_running_; });
        }
    }

    closeSockets();
}
