#include "prober.hpp"

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
#pragma pack(pop)

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

void ActiveProber::setConfig(bool enabled, const std::string &mode,
                             const std::vector<std::string> &target_ips,
                             int interval_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enabled;
    mode_ = (mode == "multi") ? "multi" : "single";
    target_ips_ = target_ips;
    interval_ms_ = std::max(10, std::min(interval_ms, 2000));

    std::cout << "[Prober] Updated: " << (enabled_ ? "ENABLED" : "DISABLED")
              << " | Mode: " << mode_ << " | Targets: " << target_ips_.size()
              << " | Interval: " << interval_ms_ << " ms ("
              << (1000 / interval_ms_) << " Hz)\n";
    cv_.notify_all();
}

ProberStatus ActiveProber::getStatus(bool scan_network) {
    ProberStatus status;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status.enabled = enabled_;
        status.mode = mode_;
        status.target_ips = target_ips_;
        status.target_ip = target_ips_.empty() ? "" : target_ips_.front();
        status.interval_ms = interval_ms_;
        status.interface_name = interface_name_;
        status.packets_sent = packets_sent_;
        status.active = enabled_ && is_running_;
    }

    const auto now = std::chrono::steady_clock::now();
    if (!scan_network) {
        std::lock_guard<std::mutex> lock(scan_cache_mutex_);
        if (!cached_cards_.empty() &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_scan_time_).count() < 15000) {
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

    // 3. Quét các Card Wi-Fi và Station kết nối qua hostapd ubus (Userspace AP daemon)
    // An toàn tuyệt đối: Không truy vấn Netlink nl80211 vào driver ath12k, không chạm vào debugfs dentry
    std::string cmd =
        "for iface in $(ubus list 'hostapd.*' 2>/dev/null); do "
        "  echo \"===HOSTAPD:$iface===\"; "
        "  ubus call \"$iface\" get_status 2>/dev/null; "
        "  echo \"===CLIENTS===\"; "
        "  ubus call \"$iface\" get_clients 2>/dev/null; "
        "done";

    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[1024];
        std::string cur_hostapd_iface;
        std::string cur_card;
        std::string cur_ssid = "Wi-Fi Network";
        std::string cur_band = "5GHz";
        int cur_max_bw = 2; // 80 MHz mặc định cho 5GHz
        std::string cur_width_str = "80 MHz";
        std::vector<int> cur_supp_bw = {0, 1, 2};
        bool in_clients_section = false;
        ClientInfo cur_client;

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

                    if (cur_client.signal_dbm.empty()) {
                        cur_client.signal_dbm = "Online";
                    }
                    if (cur_client.snr.empty()) {
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

            if (line.compare(0, 11, "===HOSTAPD:") == 0) {
                flushCurrentCard();
                in_clients_section = false;
                size_t end = line.rfind("===");
                if (end != std::string::npos && end > 11) {
                    cur_hostapd_iface = line.substr(11, end - 11);
                    size_t dot = cur_hostapd_iface.find('.');
                    cur_card = (dot != std::string::npos) ? cur_hostapd_iface.substr(dot + 1) : cur_hostapd_iface;
                } else {
                    cur_card = "phy00";
                }
                cur_ssid = "Wi-Fi Network";
                if (cur_card.find(".0-") != std::string::npos || cur_card.find("5g") != std::string::npos) {
                    cur_band = "5GHz";
                    cur_max_bw = 2; // 80 MHz
                    cur_width_str = "80 MHz";
                    cur_supp_bw = {0, 1, 2};
                } else {
                    cur_band = "2.4GHz";
                    cur_max_bw = 0; // 20 MHz
                    cur_width_str = "20 MHz";
                    cur_supp_bw = {0};
                }
            } else if (line == "===CLIENTS===") {
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
                        if (freq > 4000) {
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
                size_t quote1 = line.find('\"');
                if (quote1 != std::string::npos && line.size() >= quote1 + 19) {
                    std::string potential_mac = line.substr(quote1 + 1, 17);
                    if (isValidMac(potential_mac) && line.find('{', quote1 + 18) != std::string::npos) {
                        flushCurrentClient();
                        cur_client.mac = potential_mac;
                        cur_client.max_bandwidth = cur_max_bw;
                        cur_client.width_str = cur_width_str;
                        cur_client.supported_bandwidths = cur_supp_bw;
                        continue;
                    }
                }

                if (!cur_client.mac.empty()) {
                    if (line.find("\"he\": true") != std::string::npos) {
                        cur_client.max_bandwidth = cur_max_bw;
                        cur_client.width_str = cur_width_str;
                        cur_client.supported_bandwidths = cur_supp_bw;
                    } else if (line.find("\"vht\": true") != std::string::npos && cur_max_bw >= 2) {
                        cur_client.max_bandwidth = 2;
                        cur_client.width_str = "80 MHz";
                        cur_client.supported_bandwidths = {0, 1, 2};
                    } else if (line.find("\"ht\": true") != std::string::npos && cur_client.max_bandwidth == 0) {
                        cur_client.max_bandwidth = (cur_band == "5GHz") ? 1 : 0;
                        cur_client.width_str = (cur_band == "5GHz") ? "40 MHz" : "20 MHz";
                        cur_client.supported_bandwidths = (cur_band == "5GHz") ? std::vector<int>{0, 1} : std::vector<int>{0};
                    }

                    if (line.find("\"tx\": ") != std::string::npos && cur_client.bitrate.empty()) {
                        size_t pos = line.find("\"tx\": ");
                        try {
                            uint64_t rate_bps = std::stoull(line.substr(pos + 6));
                            if (rate_bps > 0) {
                                float mbps = static_cast<float>(rate_bps) / 1000000.0f;
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

    // 4. Fallback cho máy dev test (không có card Wi-Fi), hiển thị từ DHCP/ARP để test UI
    if (cards.empty() && !dhcp_map.empty()) {
        WifiCardInfo test_card;
        test_card.iface = "br-lan";
        test_card.ssid = "LAN / WLAN Devices";
        test_card.band = "LAN";
        test_card.max_bandwidth = 0;
        test_card.width_str = "20 MHz";
        test_card.supported_bandwidths = {0};
        test_card.station_count = 0;

        for (const auto &pair : dhcp_map) {
            std::string upper_mac = normalizeMacUpper(pair.first);
            if (seen_macs.find(upper_mac) == seen_macs.end() && seen_ips.find(pair.second.first) == seen_ips.end()) {
                ClientInfo info;
                info.mac = upper_mac;
                info.ip = pair.second.first;
                info.hostname = pair.second.second;
                info.iface = "br-lan";
                info.ssid = "LAN / WLAN Devices";
                info.band = "LAN";
                info.signal_dbm = "Online";
                info.snr = "Online";
                info.bitrate = "Active";
                info.inactive_ms = "LAN";
                info.is_wireless = false;
                info.max_bandwidth = 0;
                info.width_str = "20 MHz";
                info.supported_bandwidths = {0};

                seen_macs.insert(upper_mac);
                seen_ips.insert(info.ip);
                clients.push_back(info);
                test_card.station_count++;
            }
        }
        cards.push_back(test_card);
    }
}

std::vector<ClientInfo> ActiveProber::scanClients() {
    std::vector<WifiCardInfo> dummy_cards;
    std::vector<ClientInfo> clients;
    scanNetwork(dummy_cards, clients);
    return clients;
}

std::vector<std::string> ActiveProber::getActiveTargets() {
    std::vector<std::string> targets;

    if (mode_ == "single") {
        if (!target_ips_.empty() && !target_ips_.front().empty() && target_ips_.front() != "auto") {
            targets.push_back(target_ips_.front());
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
            if (c.ip.empty() || c.ip == "127.0.0.1") continue;
            int score = -100;
            if (!c.snr.empty()) {
                try {
                    score = std::stoi(c.snr);
                } catch (...) {}
            } else if (!c.signal_dbm.empty()) {
                try {
                    score = std::stoi(c.signal_dbm);
                } catch (...) {}
            }
            if (score > best_score || best_ip.empty()) {
                best_score = score;
                best_ip = c.ip;
            }
        }
        if (!best_ip.empty()) {
            targets.push_back(best_ip);
        }
    } else { // Multi mode
        if (!target_ips_.empty()) {
            for (const auto &ip : target_ips_) {
                if (!ip.empty() && ip != "auto")
                    targets.push_back(ip);
            }
            return targets;
        }

        // Nếu danh sách rỗng ở multi mode: lấy từ cached_clients_
        std::vector<ClientInfo> clients;
        {
            std::lock_guard<std::mutex> lock(scan_cache_mutex_);
            clients = cached_clients_;
        }

        for (const auto &c : clients) {
            if (!c.ip.empty() && c.ip != "127.0.0.1") {
                targets.push_back(c.ip);
            }
        }
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
    raw_arp_fd_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
    if (raw_arp_fd_ >= 0) {
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
                  << std::strerror(errno) << ", using UDP probing\n";
    }
#endif

    return (udp_fd_ >= 0 || raw_arp_fd_ >= 0);
}

void ActiveProber::closeSockets() {
    if (udp_fd_ >= 0) {
        close(udp_fd_);
        udp_fd_ = -1;
    }
    if (raw_arp_fd_ >= 0) {
        close(raw_arp_fd_);
        raw_arp_fd_ = -1;
    }
}

bool ActiveProber::sendUdpProbe(const std::string &ip) {
    if (udp_fd_ < 0 || ip.empty())
        return false;

    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(9999);
    if (inet_pton(AF_INET, ip.c_str(), &target.sin_addr) <= 0)
        return false;

    static const char payload[] = "CSI_PING_PROBE";
    ssize_t sent = sendto(udp_fd_, payload, sizeof(payload) - 1, 0,
                          reinterpret_cast<sockaddr *>(&target), sizeof(target));
    return (sent > 0);
}

bool ActiveProber::sendArpProbe(const std::string &ip) {
#ifdef __linux__
    if (raw_arp_fd_ < 0 || ifindex_ <= 0 || ip.empty())
        return false;

    uint32_t target_ip = 0;
    if (inet_pton(AF_INET, ip.c_str(), &target_ip) <= 0)
        return false;

    ArpPacket packet{};
    std::memset(packet.dst_mac, 0xFF, 6);
    std::memcpy(packet.src_mac, src_mac_, 6);
    packet.ethertype = htons(ETH_P_ARP);

    packet.hw_type = htons(1);           // Ethernet
    packet.proto_type = htons(ETH_P_IP); // IPv4
    packet.hw_size = 6;
    packet.proto_size = 4;
    packet.opcode = htons(ARPOP_REQUEST);
    std::memcpy(packet.sender_mac, src_mac_, 6);
    packet.sender_ip = src_ip_;
    std::memset(packet.target_mac, 0x00, 6);
    packet.target_ip = target_ip;

    sockaddr_ll sll{};
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifindex_;
    sll.sll_protocol = htons(ETH_P_ARP);
    sll.sll_halen = 6;
    std::memset(sll.sll_addr, 0xFF, 6);

    ssize_t sent = sendto(raw_arp_fd_, &packet, sizeof(packet), 0,
                          reinterpret_cast<sockaddr *>(&sll), sizeof(sll));
    return (sent > 0);
#else
    (void)ip;
    return false;
#endif
}

void ActiveProber::run() {
    initSockets();

    while (is_running_) {
        std::vector<std::string> targets;
        int interval = 50;
        bool should_probe = false;

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
            interval = std::max(10, std::min(interval_ms_, 2000));
            should_probe = enabled_ && !targets.empty();
        }

        if (should_probe) {
            uint64_t sent_count = 0;
            for (const auto &target : targets) {
                bool sent = false;
                sent |= sendArpProbe(target);
                sent |= sendUdpProbe(target);
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
            cv_.wait_for(lock, std::chrono::milliseconds(interval),
                         [this] { return !is_running_; });
        }
    }

    closeSockets();
}
