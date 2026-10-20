#include "web_server.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
constexpr size_t kMaximumClientWorkers = 16;

bool sendAll(int socket_fd, const std::string &data) {
    size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t result = send(socket_fd, data.data() + sent,
                                    data.size() - sent, MSG_NOSIGNAL);
        if (result < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (result == 0)
            return false;
        sent += static_cast<size_t>(result);
    }
    return true;
}

int createListener(int port, int backlog) {
    const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
        return -1;

    int reuse = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(socket_fd, reinterpret_cast<sockaddr *>(&address),
             sizeof(address)) < 0 ||
        listen(socket_fd, backlog) < 0) {
        const int saved_errno = errno;
        close(socket_fd);
        errno = saved_errno;
        return -1;
    }
    return socket_fd;
}

std::string httpResponse(const std::string &status,
                         const std::string &content_type,
                         const std::string &body) {
    return "HTTP/1.1 " + status +
           "\r\n"
           "Content-Type: " +
           content_type +
           "\r\n"
           "Content-Length: " +
           std::to_string(body.size()) +
           "\r\n"
           "Cache-Control: no-store\r\n"
           "Connection: close\r\n\r\n" +
           body;
}
} // namespace

WebServer::WebServer(int web_port, int ingest_port, std::string web_root,
                     ActiveProber *prober)
    : web_port_(web_port), ingest_port_(ingest_port),
      web_root_(std::move(web_root)), prober_(prober), web_fd_(-1),
      ingest_fd_(-1), is_running_(false) {}

WebServer::~WebServer() { stop(); }

bool WebServer::start() {
    if (is_running_)
        return true;

    const int web_socket = createListener(web_port_, 16);
    if (web_socket < 0) {
        std::cerr << "[Web] Cannot listen on port " << web_port_ << ": "
                  << std::strerror(errno) << '\n';
        return false;
    }

    const int ingest_socket = createListener(ingest_port_, 8);
    if (ingest_socket < 0) {
        std::cerr << "[Ingest] Cannot listen on port " << ingest_port_ << ": "
                  << std::strerror(errno) << '\n';
        close(web_socket);
        return false;
    }

    web_fd_ = web_socket;
    ingest_fd_ = ingest_socket;
    is_running_ = true;

    try {
        web_thread_ = std::thread(&WebServer::runWebServer, this);
        ingest_thread_ = std::thread(&WebServer::runIngestServer, this);
    } catch (const std::exception &error) {
        std::cerr << "[Server] Cannot start listener threads: " << error.what()
                  << '\n';
        stop();
        return false;
    }

    std::cout << "[Edge CSI] Web dashboard: http://" << getLocalIP() << ':'
              << web_port_ << "/\n"
              << "[Edge CSI] SSE stream:    http://" << getLocalIP() << ':'
              << web_port_ << "/events\n"
              << "[Edge CSI] Relay ingest:  tcp://" << getLocalIP() << ':'
              << ingest_port_ << '\n';
    return true;
}

void WebServer::stop() {
    {
        std::lock_guard<std::mutex> lock(workers_mutex_);
        if (!is_running_.exchange(false))
            return;
    }
    data_changed_.notify_all();

    const int web_socket = web_fd_.exchange(-1);
    if (web_socket >= 0) {
        shutdown(web_socket, SHUT_RDWR);
        close(web_socket);
    }
    const int ingest_socket = ingest_fd_.exchange(-1);
    if (ingest_socket >= 0) {
        shutdown(ingest_socket, SHUT_RDWR);
        close(ingest_socket);
    }

    {
        std::lock_guard<std::mutex> lock(workers_mutex_);
        for (const int client_fd : clients_)
            shutdown(client_fd, SHUT_RDWR);
    }

    if (web_thread_.joinable())
        web_thread_.join();
    if (ingest_thread_.joinable())
        ingest_thread_.join();

    {
        std::unique_lock<std::mutex> lock(workers_mutex_);
        workers_changed_.wait(lock, [this] { return active_workers_ == 0; });
    }
}

void WebServer::publish(const CSIData &frame) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    latest_frame_ = frame;
    motion_detector_.update(latest_frame_);
    breath_detector_.update(latest_frame_);

    // Quyết định hiện diện hợp nhất 3 trạng thái cốt lõi:
    // 1. Chuyển động vĩ mô (người đi lại / cử động)
    const bool is_macro_moving =
        latest_frame_.motion_calibrated &&
        (latest_frame_.motion_score > latest_frame_.motion_threshold_high);
    // 2. Vi chuyển động nhịp thở (người ngồi yên thức hoặc nằm ngủ)
    const bool is_micro_breathing = latest_frame_.is_human_presence;

    // Có người = Đang di chuyển HOẶC Đang ngồi/ngủ có nhịp thở
    latest_frame_.presence_detected = is_macro_moving || is_micro_breathing;

    if (is_macro_moving) {
        latest_frame_.presence_status = "human_moving";
    } else if (is_micro_breathing) {
        latest_frame_.presence_status = "human_present";
    } else {
        latest_frame_.presence_status = "empty_room";
    }

    has_frame_ = true;
    ++sequence_;
    data_changed_.notify_all();
}

void WebServer::resetMotionCalibration() {
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        motion_detector_.reset();
        breath_detector_.reset();
        if (has_frame_) {
            latest_frame_.motion_score = 0.0f;
            latest_frame_.motion_threshold_high = 0.0f;
            latest_frame_.motion_threshold_low = 0.0f;
            latest_frame_.motion_window_samples = 0;
            latest_frame_.motion_calibration_frames = 0;
            latest_frame_.motion_calibration_stable = false;
            latest_frame_.motion_ready = false;
            latest_frame_.motion_calibrated = false;
            latest_frame_.presence_detected = false;
            latest_frame_.breath_valid = false;
            latest_frame_.is_human_presence = false;
            latest_frame_.is_sleeping = false;
            latest_frame_.breath_regularity = 0.0f;
            latest_frame_.presence_status = "calibrating";
            ++sequence_;
        }
    }
    data_changed_.notify_all();
}

void WebServer::runWebServer() {
    while (is_running_) {
        const int listener = web_fd_.load();
        if (listener < 0)
            break;

        sockaddr_in client_address{};
        socklen_t address_size = sizeof(client_address);
        const int client_fd =
            accept(listener, reinterpret_cast<sockaddr *>(&client_address),
                   &address_size);
        if (client_fd < 0) {
            if (!is_running_)
                break;
            if (errno == EINTR)
                continue;
            std::cerr << "[Web] accept failed: " << std::strerror(errno)
                      << '\n';
            continue;
        }

        startClientWorker(client_fd, true);
    }
}

void WebServer::runIngestServer() {
    while (is_running_) {
        const int listener = ingest_fd_.load();
        if (listener < 0)
            break;

        sockaddr_in client_address{};
        socklen_t address_size = sizeof(client_address);
        const int client_fd =
            accept(listener, reinterpret_cast<sockaddr *>(&client_address),
                   &address_size);
        if (client_fd < 0) {
            if (!is_running_)
                break;
            if (errno == EINTR)
                continue;
            std::cerr << "[Ingest] accept failed: " << std::strerror(errno)
                      << '\n';
            continue;
        }

        startClientWorker(client_fd, false);
    }
}

void WebServer::startClientWorker(int client_fd, bool web_client) {
    {
        std::lock_guard<std::mutex> lock(workers_mutex_);
        if (!is_running_) {
            close(client_fd);
            return;
        }
        if (active_workers_ >= kMaximumClientWorkers) {
            if (web_client)
                sendAll(client_fd, httpResponse("503 Service Unavailable",
                                                "text/plain; charset=utf-8",
                                                "Too many active clients"));
            close(client_fd);
            return;
        }
        try {
            clients_.insert(client_fd);
        } catch (const std::exception &error) {
            std::cerr << "[Server] Cannot register client: " << error.what()
                      << '\n';
            close(client_fd);
            return;
        }
        ++active_workers_;
    }

    try {
        std::thread worker([this, client_fd, web_client] {
            try {
                if (web_client)
                    handleWebClient(client_fd);
                else
                    handleIngestClient(client_fd);
            } catch (const std::exception &error) {
                std::cerr << "[Server] Client handler failed: " << error.what()
                          << '\n';
            } catch (...) {
                std::cerr << "[Server] Client handler failed with an unknown "
                             "exception\n";
            }

            shutdown(client_fd, SHUT_RDWR);
            close(client_fd);
            {
                std::lock_guard<std::mutex> lock(workers_mutex_);
                clients_.erase(client_fd);
                --active_workers_;
            }
            workers_changed_.notify_all();
        });
        worker.detach();
    } catch (const std::exception &error) {
        std::cerr << "[Server] Cannot start client handler: " << error.what()
                  << '\n';
        close(client_fd);
        {
            std::lock_guard<std::mutex> lock(workers_mutex_);
            clients_.erase(client_fd);
            --active_workers_;
        }
        workers_changed_.notify_all();
    }
}

void WebServer::handleWebClient(int client_fd) {
    char request[4096]{};
    const ssize_t bytes_read = recv(client_fd, request, sizeof(request) - 1, 0);
    if (bytes_read <= 0)
        return;

    std::istringstream request_stream(
        std::string(request, static_cast<size_t>(bytes_read)));
    std::string method;
    std::string path;
    request_stream >> method >> path;
    const size_t query_position = path.find('?');
    if (query_position != std::string::npos)
        path.resize(query_position);

    if (method == "POST" && path == "/api/calibrate") {
        resetMotionCalibration();
        sendAll(
            client_fd,
            httpResponse(
                "200 OK", "application/json; charset=utf-8",
                "{\"status\":\"calibrating\",\"stable_frames_required\":50}"));
        return;
    }

    if (path == "/api/prober") {
        const std::string full_request(request, static_cast<size_t>(bytes_read));
        std::string body;
        const size_t header_end = full_request.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            body = full_request.substr(header_end + 4);
        }
        handleProberApi(client_fd, method, body);
        return;
    }

    if (method != "GET") {
        sendAll(client_fd, httpResponse("405 Method Not Allowed",
                                        "text/plain; charset=utf-8",
                                        "Method not allowed"));
        return;
    }

    if (path == "/events") {
        handleEventStream(client_fd);
        return;
    }
    if (path == "/latest_csi.json") {
        std::string body;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            body =
                has_frame_
                    ? latest_frame_.toJson()
                    : "{\"has_data\":false,\"subcarriers_count\":0,\"motion_"
                      "score\":0,"
                      "\"motion_ready\":false,\"motion_window_samples\":0,"
                      "\"motion_calibrated\":false,\"motion_calibration_"
                      "frames\":0,"
                      "\"motion_calibration_stable\":false,"
                      "\"motion_threshold_high\":0,\"motion_threshold_low\":0,"
                      "\"presence_detected\":false,\"breath_bpm\":0,\"breath_"
                      "wave\":0,"
                      "\"breath_quality\":0,\"breath_phase_span\":0,\"breath_"
                      "asymmetry\":0,"
                      "\"breath_rrv\":0,\"breath_regularity\":0,\"breath_"
                      "valid\":false,\"is_human_presence\":false,"
                      "\"is_sleeping\":false,\"presence_status\":\"empty_"
                      "room\",\"chains\":[]}";
        }
        sendAll(
            client_fd,
            httpResponse("200 OK", "application/json; charset=utf-8", body));
        return;
    }
    if (path == "/health.json") {
        bool has_data;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            has_data = has_frame_;
        }
        const std::string body =
            std::string("{\"status\":\"ok\",\"has_data\":") +
            (has_data ? "true}" : "false}");
        sendAll(
            client_fd,
            httpResponse("200 OK", "application/json; charset=utf-8", body));
        return;
    }

    std::string filepath;
    std::string content_type;
    if (path == "/" || path == "/index.html") {
        filepath = web_root_ + "/index.html";
        content_type = "text/html; charset=utf-8";
    } else if (path == "/style.css") {
        filepath = web_root_ + "/style.css";
        content_type = "text/css; charset=utf-8";
    } else if (path == "/script.js") {
        filepath = web_root_ + "/script.js";
        content_type = "application/javascript; charset=utf-8";
    } else {
        sendAll(client_fd,
                httpResponse("404 Not Found", "text/plain; charset=utf-8",
                             "Not found"));
        return;
    }

    const std::string body = readFile(filepath);
    if (body.empty()) {
        sendAll(client_fd,
                httpResponse("404 Not Found", "text/plain; charset=utf-8",
                             "Web asset not found"));
        return;
    }
    sendAll(client_fd, httpResponse("200 OK", content_type, body));
}

void WebServer::handleProberApi(int client_fd, const std::string &method,
                                const std::string &body) {
    if (!prober_) {
        sendAll(client_fd,
                httpResponse("503 Service Unavailable",
                             "application/json; charset=utf-8",
                             "{\"error\":\"Prober not initialized\"}"));
        return;
    }

    if (method == "POST") {
        ProberStatus current = prober_->getStatus();
        bool new_enabled = current.enabled;
        std::string new_mode = current.mode;
        std::vector<std::string> new_targets = current.target_ips;
        int new_interval = current.interval_ms;

        // Trích xuất "enabled": true/false
        size_t pos = body.find("\"enabled\"");
        if (pos != std::string::npos) {
            size_t colon = body.find(':', pos);
            if (colon != std::string::npos) {
                std::string sub = body.substr(colon + 1, 15);
                if (sub.find("true") != std::string::npos)
                    new_enabled = true;
                else if (sub.find("false") != std::string::npos)
                    new_enabled = false;
            }
        }
        // Trích xuất "mode": "single" / "multi"
        pos = body.find("\"mode\"");
        if (pos != std::string::npos) {
            size_t first_q = body.find('\"', pos + 6);
            if (first_q != std::string::npos) {
                size_t second_q = body.find('\"', first_q + 1);
                if (second_q != std::string::npos) {
                    new_mode = body.substr(first_q + 1, second_q - first_q - 1);
                }
            }
        }
        // Trích xuất "target_ips": ["...", "..."]
        pos = body.find("\"target_ips\"");
        if (pos != std::string::npos) {
            size_t start_arr = body.find('[', pos);
            size_t end_arr = body.find(']', start_arr);
            if (start_arr != std::string::npos && end_arr != std::string::npos) {
                new_targets.clear();
                std::string arr_str = body.substr(start_arr + 1, end_arr - start_arr - 1);
                size_t p = 0;
                while ((p = arr_str.find('\"', p)) != std::string::npos) {
                    size_t p2 = arr_str.find('\"', p + 1);
                    if (p2 == std::string::npos)
                        break;
                    std::string ip = arr_str.substr(p + 1, p2 - p - 1);
                    if (!ip.empty())
                        new_targets.push_back(ip);
                    p = p2 + 1;
                }
            }
        } else {
            // Hoặc trích xuất "target_ip": "..." nếu gửi kiểu đơn
            pos = body.find("\"target_ip\"");
            if (pos != std::string::npos) {
                size_t first_q = body.find('\"', pos + 11);
                if (first_q != std::string::npos) {
                    size_t second_q = body.find('\"', first_q + 1);
                    if (second_q != std::string::npos) {
                        std::string ip = body.substr(first_q + 1, second_q - first_q - 1);
                        if (!ip.empty()) {
                            new_targets.clear();
                            new_targets.push_back(ip);
                        }
                    }
                }
            }
        }
        // Trích xuất "interval_ms": 50
        pos = body.find("\"interval_ms\"");
        if (pos != std::string::npos) {
            size_t colon = body.find(':', pos);
            if (colon != std::string::npos) {
                try {
                    new_interval = std::stoi(body.substr(colon + 1));
                } catch (...) {
                }
            }
        }

        prober_->setConfig(new_enabled, new_mode, new_targets, new_interval);
    }

    // Format ProberStatus thành JSON trả về
    ProberStatus status = prober_->getStatus();
    std::ostringstream json;
    json << "{\"status\":\"ok\""
         << ",\"enabled\":" << (status.enabled ? "true" : "false")
         << ",\"mode\":\"" << status.mode << "\""
         << ",\"target_ip\":\"" << status.target_ip << "\""
         << ",\"target_ips\":[";
    for (size_t i = 0; i < status.target_ips.size(); ++i) {
        if (i > 0)
            json << ",";
        json << "\"" << status.target_ips[i] << "\"";
    }
    json << "]"
         << ",\"interval_ms\":" << status.interval_ms
         << ",\"packets_sent\":" << status.packets_sent
         << ",\"interface\":\"" << status.interface_name << "\""
         << ",\"cards\":[";

    for (size_t i = 0; i < status.cards.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"iface\":\"" << status.cards[i].iface << "\""
             << ",\"ssid\":\"" << status.cards[i].ssid << "\""
             << ",\"band\":\"" << status.cards[i].band << "\""
             << ",\"station_count\":" << status.cards[i].station_count << "}";
    }
    json << "],\"clients\":[";

    for (size_t i = 0; i < status.clients.size(); ++i) {
        if (i > 0)
            json << ",";
        json << "{\"ip\":\"" << status.clients[i].ip << "\""
             << ",\"mac\":\"" << status.clients[i].mac << "\""
             << ",\"hostname\":\"" << status.clients[i].hostname << "\""
             << ",\"iface\":\"" << status.clients[i].iface << "\""
             << ",\"ssid\":\"" << status.clients[i].ssid << "\""
             << ",\"band\":\"" << status.clients[i].band << "\""
             << ",\"signal_dbm\":\"" << status.clients[i].signal_dbm << "\""
             << ",\"snr\":\"" << status.clients[i].snr << "\""
             << ",\"bitrate\":\"" << status.clients[i].bitrate << "\""
             << ",\"inactive_ms\":\"" << status.clients[i].inactive_ms << "\""
             << ",\"is_wireless\":" << (status.clients[i].is_wireless ? "true" : "false") << "}";
    }
    json << "]}";

    sendAll(client_fd, httpResponse("200 OK",
                                    "application/json; charset=utf-8",
                                    json.str()));
}

void WebServer::handleIngestClient(int client_fd) {
    CSIParser parser;
    uint8_t buffer[16384];
    while (is_running_) {
        const ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes_read < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (bytes_read == 0)
            break;

        std::vector<CSIData> frames;
        if (!parser.feed(buffer, static_cast<size_t>(bytes_read), frames)) {
            std::cerr << "[Ingest] Invalid or oversized relay stream; "
                         "disconnecting client\n";
            break;
        }
        for (const CSIData &frame : frames)
            publish(frame);
    }
}

void WebServer::handleEventStream(int client_fd) {
    const std::string headers = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/event-stream\r\n"
                                "Cache-Control: no-cache\r\n"
                                "Connection: keep-alive\r\n"
                                "X-Accel-Buffering: no\r\n\r\n";
    if (!sendAll(client_fd, headers))
        return;

    uint64_t last_sequence = 0;
    while (is_running_) {
        CSIData frame;
        uint64_t current_sequence = 0;
        bool has_frame = false;
        {
            std::unique_lock<std::mutex> lock(data_mutex_);
            data_changed_.wait_for(
                lock, std::chrono::seconds(15), [this, last_sequence] {
                    return !is_running_ || sequence_ != last_sequence;
                });
            if (!is_running_)
                break;
            has_frame = has_frame_;
            current_sequence = sequence_;
            if (has_frame)
                frame = latest_frame_;
        }

        if (has_frame && current_sequence != last_sequence) {
            const std::string event =
                "id: " + std::to_string(current_sequence) +
                "\n"
                "event: csi\n"
                "data: " +
                frame.toJson() + "\n\n";
            if (!sendAll(client_fd, event))
                break;
            last_sequence = current_sequence;
        } else if (!sendAll(client_fd, ": keep-alive\n\n")) {
            break;
        }
    }
}

std::string WebServer::readFile(const std::string &filepath) const {
    std::ifstream file(filepath, std::ios::binary);
    if (!file)
        return {};
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

std::string WebServer::getLocalIP() const {
    std::string local_ip = "127.0.0.1";
    ifaddrs *interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0)
        return local_ip;

    for (ifaddrs *interface = interfaces; interface != nullptr;
         interface = interface->ifa_next) {
        if (!interface->ifa_addr || interface->ifa_addr->sa_family != AF_INET ||
            (interface->ifa_flags & IFF_LOOPBACK) != 0)
            continue;

        char address[INET_ADDRSTRLEN]{};
        const auto *ipv4 =
            reinterpret_cast<const sockaddr_in *>(interface->ifa_addr);
        if (inet_ntop(AF_INET, &ipv4->sin_addr, address, sizeof(address))) {
            local_ip = address;
            break;
        }
    }
    freeifaddrs(interfaces);
    return local_ip;
}
