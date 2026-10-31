#include "prober.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
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

std::string trim(std::string s) {
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
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_scan_time_).count() < 3500) {
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

    // 3. Quét các Card Wi-Fi và Station kết nối (Ưu tiên iwinfo khớp checkwifi script, fallback iw dev)
    std::string cmd =
        "if which iwinfo >/dev/null 2>&1; then "
        "  for dev in $(iwinfo 2>/dev/null | grep -oE \"phy[0-9]+\\.[0-9]+-[0-9]+\"); do "
        "    ssid=$(iwinfo \"$dev\" info 2>/dev/null | grep -oE 'ESSID: \"[^\"]+\"' | cut -d'\"' -f2); "
        "    echo \"===CARD:$dev:$ssid===\"; "
        "    iwinfo \"$dev\" assoclist 2>/dev/null; "
        "  done; "
        "else "
        "  for iface in $(iw dev 2>/dev/null | awk '$1==\"Interface\"{print $2}'); do "
        "    echo \"===CARD:$iface:Wi-Fi===\"; "
        "    iw dev \"$iface\" station dump 2>/dev/null; "
        "  done; "
        "fi";

    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[1024];
        std::string cur_card;
        std::string cur_ssid;
        std::string cur_band;
        size_t last_client_idx = static_cast<size_t>(-1);
        bool in_iw_dev_station = false;
        ClientInfo iw_dev_client;

        auto flushIwDevStation = [&]() {
            if (in_iw_dev_station && !iw_dev_client.mac.empty()) {
                std::string upper_mac = normalizeMacUpper(iw_dev_client.mac);
                if (seen_macs.find(upper_mac) == seen_macs.end()) {
                    std::string lower_mac = normalizeMacLower(upper_mac);
                    if (dhcp_map.find(lower_mac) != dhcp_map.end()) {
                        iw_dev_client.ip = dhcp_map[lower_mac].first;
                        iw_dev_client.hostname = dhcp_map[lower_mac].second;
                    } else {
                        iw_dev_client.ip = "";
                        iw_dev_client.hostname = "Wi-Fi Station";
                    }
                    iw_dev_client.mac = upper_mac;
                    iw_dev_client.iface = cur_card;
                    iw_dev_client.ssid = cur_ssid;
                    iw_dev_client.band = cur_band;
                    iw_dev_client.is_wireless = true;

                    seen_macs.insert(upper_mac);
                    if (!iw_dev_client.ip.empty()) {
                        seen_ips.insert(iw_dev_client.ip);
                    }
                    clients.push_back(iw_dev_client);
                    if (!cards.empty()) {
                        cards.back().station_count++;
                    }
                }
            }
            iw_dev_client = ClientInfo{};
            in_iw_dev_station = false;
        };

        while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();

            if (line.compare(0, 8, "===CARD:") == 0) {
                flushIwDevStation();
                last_client_idx = static_cast<size_t>(-1);
                // Parse dev và ssid: "===CARD:phy00.0-0:10. VNPT Wifi 8 5GHz==="
                size_t end = line.rfind("===");
                if (end != std::string::npos && end > 8) {
                    std::string middle = line.substr(8, end - 8);
                    size_t colon = middle.find(':');
                    if (colon != std::string::npos) {
                        cur_card = middle.substr(0, colon);
                        cur_ssid = middle.substr(colon + 1);
                    } else {
                        cur_card = middle;
                        cur_ssid = "Wi-Fi Network";
                    }
                }
                // Xác định băng tần 5GHz hoặc 2.4GHz
                if (cur_card.find(".0-") != std::string::npos || cur_ssid.find("5G") != std::string::npos || cur_card.find("5g") != std::string::npos) {
                    cur_band = "5GHz";
                } else if (cur_card.find(".1-") != std::string::npos || cur_ssid.find("2.4G") != std::string::npos || cur_card.find("2g") != std::string::npos) {
                    cur_band = "2.4GHz";
                } else {
                    cur_band = "Wi-Fi";
                }

                WifiCardInfo card;
                card.iface = cur_card;
                card.ssid = cur_ssid;
                card.band = cur_band;
                card.station_count = 0;
                cards.push_back(card);
            }
            // Format iwinfo assoclist: B6:07:33:18:8E:E2  unknown / -99 dBm (SNR 99)  60 ms ago
            else if (line.size() >= 17 && line[2] == ':' && line[5] == ':' && line[8] == ':' && line[11] == ':' && line[14] == ':') {
                flushIwDevStation();
                std::string raw_mac = line.substr(0, 17);
                std::string upper_mac = normalizeMacUpper(raw_mac);
                if (seen_macs.find(upper_mac) != seen_macs.end()) {
                    // Trùng MAC -> tuyệt đối không lặp lại
                    continue;
                }

                ClientInfo c;
                c.mac = upper_mac;
                c.iface = cur_card;
                c.ssid = cur_ssid;
                c.band = cur_band;
                c.is_wireless = true;

                // Ghép nối DHCP lease / ARP
                std::string lower_mac = normalizeMacLower(upper_mac);
                if (dhcp_map.find(lower_mac) != dhcp_map.end()) {
                    c.ip = dhcp_map[lower_mac].first;
                    c.hostname = dhcp_map[lower_mac].second;
                } else {
                    c.ip = "";
                    c.hostname = "Wi-Fi Station";
                }

                // Trích xuất SNR: (SNR 99)
                size_t snr_pos = line.find("(SNR ");
                if (snr_pos != std::string::npos) {
                    size_t snr_end = line.find(')', snr_pos + 5);
                    if (snr_end != std::string::npos) {
                        c.snr = line.substr(snr_pos + 5, snr_end - (snr_pos + 5));
                    }
                }

                // Trích xuất Signal dBm
                size_t dbm_pos = line.find("dBm");
                if (dbm_pos != std::string::npos) {
                    size_t slash_pos = line.find('/');
                    if (slash_pos != std::string::npos && slash_pos < dbm_pos) {
                        size_t start = line.rfind(' ', dbm_pos - 1);
                        if (start != std::string::npos) {
                            c.signal_dbm = trim(line.substr(start + 1, dbm_pos + 3 - (start + 1)));
                        } else {
                            c.signal_dbm = trim(line.substr(slash_pos + 1, dbm_pos + 3 - (slash_pos + 1)));
                        }
                    } else {
                        size_t start = line.rfind(' ', dbm_pos - 1);
                        if (start != std::string::npos) {
                            c.signal_dbm = trim(line.substr(start + 1, dbm_pos + 3 - (start + 1)));
                        } else {
                            c.signal_dbm = trim(line.substr(0, dbm_pos + 3));
                        }
                    }
                } else if (!c.snr.empty()) {
                    c.signal_dbm = "SNR " + c.snr;
                } else {
                    c.signal_dbm = "—";
                }

                // Trích xuất Inactive: vd 60 ms ago
                size_t ago_pos = line.find(" ago");
                if (ago_pos != std::string::npos) {
                    size_t start = line.rfind(')', ago_pos);
                    if (start == std::string::npos) {
                        start = line.rfind("dBm", ago_pos);
                        if (start != std::string::npos) start += 3;
                    }
                    if (start != std::string::npos && start < ago_pos) {
                        c.inactive_ms = trim(line.substr(start + 1, ago_pos - (start + 1)));
                    }
                }
                if (c.inactive_ms.empty()) {
                    c.inactive_ms = "Active";
                }

                seen_macs.insert(upper_mac);
                if (!c.ip.empty()) {
                    seen_ips.insert(c.ip);
                }
                clients.push_back(c);
                last_client_idx = clients.size() - 1;
                if (!cards.empty()) {
                    cards.back().station_count++;
                }
            }
            // Trích xuất iwinfo TX bitrate: TX: 648.5 MBit/s, ...
            else if (last_client_idx < clients.size() && (line.find("TX:") != std::string::npos || line.find("\tTX:") != std::string::npos)) {
                size_t tx_pos = line.find("TX:");
                if (tx_pos != std::string::npos) {
                    size_t comma = line.find(',', tx_pos + 3);
                    if (comma != std::string::npos) {
                        clients[last_client_idx].bitrate = trim(line.substr(tx_pos + 3, comma - (tx_pos + 3)));
                    } else {
                        clients[last_client_idx].bitrate = trim(line.substr(tx_pos + 3));
                    }
                }
            }
            // Fallback iw dev station dump: Station XX:XX:XX...
            else if (line.compare(0, 8, "Station ") == 0) {
                flushIwDevStation();
                std::istringstream iss(line);
                std::string tag, mac;
                if (iss >> tag >> mac && isValidMac(mac)) {
                    in_iw_dev_station = true;
                    iw_dev_client.mac = mac;
                }
            } else if (in_iw_dev_station) {
                if (line.find("signal avg:") != std::string::npos) {
                    iw_dev_client.signal_dbm = trim(line.substr(line.find("signal avg:") + 11));
                } else if (line.find("tx bitrate:") != std::string::npos) {
                    iw_dev_client.bitrate = trim(line.substr(line.find("tx bitrate:") + 11));
                } else if (line.find("inactive time:") != std::string::npos) {
                    iw_dev_client.inactive_ms = trim(line.substr(line.find("inactive time:") + 14));
                }
            }
        }
        flushIwDevStation();
        pclose(pipe);
    }

    // 4. Fallback cho máy dev test (không có card Wi-Fi), hiển thị từ DHCP/ARP để test UI
    if (cards.empty() && !dhcp_map.empty()) {
        WifiCardInfo test_card;
        test_card.iface = "br-lan";
        test_card.ssid = "LAN / WLAN Devices";
        test_card.band = "LAN";
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
                info.signal_dbm = "—";
                info.snr = "—";
                info.bitrate = "—";
                info.inactive_ms = "LAN";
                info.is_wireless = false;

                seen_macs.insert(upper_mac);
                seen_ips.insert(info.ip);
                clients.push_back(info);
                test_card.station_count++;
            }
        }
        cards.push_back(test_card);
    }

    // 4. Xác định băng thông hoạt động thực tế (Width) cho từng Card Wi-Fi
    for (auto &card : cards) {
        int max_bw = (card.band == "5GHz") ? 2 : 0;
        std::string w_str = (card.band == "5GHz") ? "80 MHz" : "20 MHz";
        std::vector<int> supp = (card.band == "5GHz") ? std::vector<int>{0, 1, 2} : std::vector<int>{0};

        std::string iw_cmd = "iw dev " + card.iface + " info 2>/dev/null";
        FILE *iw_p = popen(iw_cmd.c_str(), "r");
        if (iw_p) {
            char iw_buf[256];
            while (fgets(iw_buf, sizeof(iw_buf), iw_p)) {
                std::string iw_line(iw_buf);
                size_t w_pos = iw_line.find("width:");
                if (w_pos != std::string::npos) {
                    if (iw_line.find("320 MHz", w_pos) != std::string::npos) {
                        max_bw = 4; w_str = "320 MHz"; supp = {0, 1, 2, 3, 4};
                    } else if (iw_line.find("160 MHz", w_pos) != std::string::npos) {
                        max_bw = 3; w_str = "160 MHz"; supp = {0, 1, 2, 3};
                    } else if (iw_line.find("80 MHz", w_pos) != std::string::npos) {
                        max_bw = 2; w_str = "80 MHz"; supp = {0, 1, 2};
                    } else if (iw_line.find("40 MHz", w_pos) != std::string::npos) {
                        max_bw = 1; w_str = "40 MHz"; supp = {0, 1};
                    } else if (iw_line.find("20 MHz", w_pos) != std::string::npos) {
                        max_bw = 0; w_str = "20 MHz"; supp = {0};
                    }
                    break;
                }
            }
            pclose(iw_p);
        }
        card.max_bandwidth = max_bw;
        card.width_str = w_str;
        card.supported_bandwidths = supp;
    }

    // 5. Xác định băng thông khả dụng cho từng thiết bị Client dựa trên năng lực card & station
    for (auto &client : clients) {
        int card_max = 0;
        for (const auto &card : cards) {
            if (card.iface == client.iface) {
                card_max = card.max_bandwidth;
                break;
            }
        }
        if (card_max == 0 && client.band == "5GHz")
            card_max = 2;

        int client_max = card_max;
        if (!client.mac.empty()) {
            std::string mac_lower = normalizeMacLower(client.mac);
            std::string phy = client.iface.substr(0, client.iface.find('.'));
            if (phy.empty()) phy = "phy00";
            std::string base = "/sys/kernel/debug/ieee80211/" + phy + "/netdev:" + client.iface + "/stations/" + mac_lower + "/link-0/";
            namespace fs = std::filesystem;
            std::error_code ec;
            if (!fs::is_directory(base, ec)) {
                base = "/sys/kernel/debug/ieee80211/" + phy + "/netdev:" + client.iface + "/stations/" + mac_lower + "/";
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

                int sta_cap = 0;
                if (!eht.empty() && eht.find("not supported") == std::string::npos) {
                    sta_cap = (eht.find("320") != std::string::npos) ? 4 : 3;
                } else if (!he.empty() && he.find("not supported") == std::string::npos) {
                    sta_cap = (he.find("160") != std::string::npos) ? 3 : 2;
                } else if (!vht.empty() && vht.find("not supported") == std::string::npos) {
                    sta_cap = (vht.find("160") != std::string::npos) ? 3 : 2;
                } else if (!ht.empty()) {
                    sta_cap = (ht.find("HT40") != std::string::npos && ht.find("No DSSS/CCK HT40") == std::string::npos) ? 1 : 0;
                }
                client_max = std::min(card_max, sta_cap);
            }
        }
        client.max_bandwidth = client_max;
        const char *bw_names[] = {"20 MHz", "40 MHz", "80 MHz", "160 MHz", "320 MHz"};
        client.width_str = (client_max >= 0 && client_max <= 4) ? bw_names[client_max] : "20 MHz";
        client.supported_bandwidths.clear();
        for (int b = 0; b <= client_max; ++b) {
            client.supported_bandwidths.push_back(b);
        }
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
        int interval = 25;
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
