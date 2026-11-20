#include "web_server.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
constexpr size_t kMaximumClientWorkers = 16;

bool equalsIgnoreCase(const std::string &a, const std::string &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool endsWith(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

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

bool sendAllBytes(int socket_fd, const char *data, size_t size) {
    size_t sent = 0;
    while (sent < size) {
        const ssize_t result = send(socket_fd, data + sent,
                                    size - sent, MSG_NOSIGNAL);
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
           "Access-Control-Allow-Origin: *\r\n"
           "Access-Control-Allow-Methods: GET, POST, OPTIONS, PUT, DELETE\r\n"
           "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With, X-Wake-Token\r\n"
           "Cache-Control: no-store\r\n"
           "Connection: close\r\n\r\n" +
           body;
}
} // namespace

WebServer::WebServer(int web_port, int ingest_port, std::string web_root,
                     ActiveProber *prober, CFRController *cfr_controller)
    : web_port_(web_port), ingest_port_(ingest_port),
      web_root_(std::move(web_root)), prober_(prober),
      cfr_controller_(cfr_controller), web_fd_(-1), ingest_fd_(-1),
      is_running_(false) {}

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
    wave_detector_.update(latest_frame_);

    // Cập nhật cấu hình băng thông từ chính metadata phần cứng của frame (0 syscall, 0 popen)
    distance_estimator_.setChannelBandwidth(latest_frame_.channel_bw);

    // Tự động nhận diện băng tần (2.4 GHz vs 5.2 GHz vs 6 GHz) từ CFRController chỉ khi đổi interface
    if (cfr_controller_) {
        const std::string iface = cfr_controller_->getCaptureIface();
        if (iface != last_capture_iface_ && !iface.empty()) {
            last_capture_iface_ = iface;
            distance_estimator_.setFrequencyMHz(cfr_controller_->getCenterFreqMHz());
        }

        // Tự động nhận diện profile thiết bị (Apple / Samsung / Phổ thông) chỉ khi đổi target
        if (prober_) {
            const std::string target_mac = cfr_controller_->getTargetMac();
            if (target_mac != last_target_mac_) {
                last_target_mac_ = target_mac;
                cached_target_hostname_ = "";
                if (!target_mac.empty()) {
                    ProberStatus pstat = prober_->getStatus(false);
                    for (const auto &c : pstat.clients) {
                        if (equalsIgnoreCase(c.mac, target_mac)) {
                            cached_target_hostname_ = c.hostname;
                            break;
                        }
                    }
                }
                distance_estimator_.setTargetDevice(target_mac, cached_target_hostname_);
            }
        }
    }

    // Lấy RSSI trung bình từ driver kernel station dump nếu có để làm neo kiểm chuẩn
    // Cache kết quả mỗi 500ms để tránh deep copy vector clients trên từng frame (100 FPS)
    int fallback_rssi = cached_fallback_rssi_;
    const auto now_rssi = std::chrono::steady_clock::now();
    if (prober_ && cfr_controller_ &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now_rssi - last_rssi_check_time_).count() >= 500) {
        last_rssi_check_time_ = now_rssi;
        const std::string t_mac = cfr_controller_->getTargetMac();
        if (!t_mac.empty()) {
            ProberStatus pstat = prober_->getStatus(false);
            for (const auto &c : pstat.clients) {
                if (equalsIgnoreCase(c.mac, t_mac)) {
                    int sig = std::atoi(c.signal_dbm.c_str());
                    if (sig < 0 && sig > -100) {
                        cached_fallback_rssi_ = sig;
                        fallback_rssi = sig;
                    }
                    break;
                }
            }
        }
    }

    // Khoảng cách và mức RSSI được tính toán trực tiếp từ biên độ CSI và AGC phần cứng của frame
    distance_estimator_.process(latest_frame_, fallback_rssi);

    // Đánh giá độ ổn định và chất lượng kết nối của client (kết hợp RSSI + CSI)
    link_stability_estimator_.process(latest_frame_);

    // Quyết định hiện diện hợp nhất 3 trạng thái cốt lõi:
    // 1. Chuyển động vĩ mô (người đi lại / cử động) - đồng bộ cờ presence_detected đã qua bộ lọc Hysteresis & Deadband từ motion_detector_
    const bool is_macro_moving =
        latest_frame_.motion_calibrated && latest_frame_.presence_detected;
    // 2. Vi chuyển động nhịp thở (người ngồi yên thức hoặc nằm ngủ)
    const bool is_micro_breathing = latest_frame_.is_human_presence;

    // Có người = Đang di chuyển HOẶC Đang ngồi/ngủ có nhịp thở
    latest_frame_.presence_detected = is_macro_moving || is_micro_breathing;

    if (is_macro_moving) {
        latest_frame_.presence_status = "human_moving";
    } else if (is_micro_breathing) {
        latest_frame_.presence_status = "human_present";
    } else if (latest_frame_.presence_status == "calibrating") {
        // Giữ trạng thái đang khởi tạo dữ liệu của breath_detector
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
        wave_detector_.reset();
        distance_estimator_.reset();
        link_stability_estimator_.reset();
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
    if (web_client) {
        struct timeval tv{3, 0};
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
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

    // Hỗ trợ CORS Preflight (OPTIONS) cho mọi Origin và Method
    if (method == "OPTIONS") {
        const std::string cors_response =
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS, PUT, DELETE\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With, X-Wake-Token\r\n"
            "Access-Control-Max-Age: 86400\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        sendAll(client_fd, cors_response);
        return;
    }

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
        handleProberApi(client_fd, method, path, body);
        return;
    }

    if (path == "/api/devices") {
        std::ostringstream ss;
        ss << "[";
        if (prober_) {
            ProberStatus pstat = prober_->getStatus(false);
            bool first = true;
            for (const auto &c : pstat.clients) {
                if (c.mac.empty()) continue;
                if (!first) ss << ",";
                first = false;
                ss << "{\"mac\":\"" << c.mac << "\""
                   << ",\"ip\":\"" << c.ip << "\""
                   << ",\"hostname\":\"" << escapeJsonString(c.hostname) << "\""
                   << ",\"band\":\"" << escapeJsonString(c.band) << "\""
                   << ",\"iface\":\"" << escapeJsonString(c.iface) << "\""
                   << ",\"signal_dbm\":\"" << escapeJsonString(c.signal_dbm) << "\"}";
            }
        }
        ss << "]";
        sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8", ss.str()));
        return;
    }

    if (path.rfind("/api/cfr/download", 0) == 0) {
        handleCfrDownload(client_fd);
        return;
    }

    if (path == "/api/cfr") {
        const std::string full_request(request, static_cast<size_t>(bytes_read));
        std::string body;
        const size_t header_end = full_request.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            body = full_request.substr(header_end + 4);
        }
        handleCfrApi(client_fd, method, body);
        return;
    }

    if (path.rfind("/api/distance", 0) == 0) {
        const std::string full_request(request, static_cast<size_t>(bytes_read));
        std::string body;
        const size_t header_end = full_request.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            body = full_request.substr(header_end + 4);
        }
        handleDistanceApi(client_fd, method, path, body);
        return;
    }

    if (path.rfind("/api/v6", 0) == 0 || path.rfind("/api/gesture", 0) == 0) {
        const std::string full_request(request, static_cast<size_t>(bytes_read));
        std::string body;
        const size_t header_end = full_request.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            body = full_request.substr(header_end + 4);
        }
        handleV6Api(client_fd, method, path, body);
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
                      "room\",\"wave_detected\":false,\"wave_confidence\":0,"
                      "\"wave_status\":\"idle\",\"wave_samples_in_window\":0,"
                      "\"wave_raw_prob\":0,\"wave_reason\":\"idle\",\"chains\":[]}";
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

    if (path.find("..") != std::string::npos) {
        sendAll(client_fd,
                httpResponse("403 Forbidden", "text/plain; charset=utf-8",
                             "Forbidden"));
        return;
    }

    if (path == "/" || path == "/index.html") {
        filepath = web_root_ + "/index.html";
        content_type = "text/html; charset=utf-8";
    } else if (path == "/gioithieu" || path == "/gioithieu.html") {
        filepath = web_root_ + "/gioithieu.html";
        content_type = "text/html; charset=utf-8";
    } else if (path == "/style.css") {
        filepath = web_root_ + "/style.css";
        content_type = "text/css; charset=utf-8";
    } else if (path == "/script.js" || endsWith(path, ".js")) {
        filepath = web_root_ + path;
        content_type = "application/javascript; charset=utf-8";
    } else if (path == "/Logo-VNPT-Technology.png" || endsWith(path, ".png")) {
        filepath = web_root_ + path;
        content_type = "image/png";
    } else if (endsWith(path, ".svg")) {
        filepath = web_root_ + path;
        content_type = "image/svg+xml";
    } else if (endsWith(path, ".ico")) {
        filepath = web_root_ + path;
        content_type = "image/x-icon";
    } else if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) {
        filepath = web_root_ + path;
        content_type = "image/jpeg";
    } else if (endsWith(path, ".webp")) {
        filepath = web_root_ + path;
        content_type = "image/webp";
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
                                const std::string &path,
                                const std::string &body) {
    if (!prober_) {
        sendAll(client_fd,
                httpResponse("503 Service Unavailable",
                             "application/json; charset=utf-8",
                             "{\"error\":\"Prober not initialized\"}"));
        return;
    }

    if (method == "POST") {
        ProberStatus current = prober_->getStatus(false);
        bool new_enabled = current.enabled;
        std::string new_target = current.target_ip;
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

        // Trích xuất "target_ip" hoặc "target_ips" (chỉ lấy 1 mục tiêu duy nhất)
        pos = body.find("\"target_ip\"");
        if (pos != std::string::npos) {
            size_t first_q = body.find('\"', pos + 11);
            if (first_q != std::string::npos) {
                size_t second_q = body.find('\"', first_q + 1);
                if (second_q != std::string::npos) {
                    new_target = body.substr(first_q + 1, second_q - first_q - 1);
                }
            }
        } else {
            pos = body.find("\"target_ips\"");
            if (pos != std::string::npos) {
                size_t start_arr = body.find('[', pos);
                size_t first_q = (start_arr != std::string::npos) ? body.find('\"', start_arr) : std::string::npos;
                if (first_q != std::string::npos) {
                    size_t second_q = body.find('\"', first_q + 1);
                    if (second_q != std::string::npos) {
                        new_target = body.substr(first_q + 1, second_q - first_q - 1);
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

        prober_->setConfig(new_enabled, new_target, new_interval);
    }

    // Format ProberStatus thành JSON trả về
    bool force_scan = (path.find("scan=1") != std::string::npos ||
                       path.find("refresh=1") != std::string::npos);
    ProberStatus status = prober_->getStatus(force_scan);
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
             << ",\"station_count\":" << status.cards[i].station_count
             << ",\"max_bandwidth\":" << status.cards[i].max_bandwidth
             << ",\"width_str\":\"" << status.cards[i].width_str << "\""
             << ",\"supported_bandwidths\":[";
        for (size_t b = 0; b < status.cards[i].supported_bandwidths.size(); ++b) {
            if (b > 0) json << ",";
            json << status.cards[i].supported_bandwidths[b];
        }
        json << "]}";
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
             << ",\"is_wireless\":" << (status.clients[i].is_wireless ? "true" : "false")
             << ",\"max_bandwidth\":" << status.clients[i].max_bandwidth
             << ",\"width_str\":\"" << status.clients[i].width_str << "\""
             << ",\"supported_bandwidths\":[";
        for (size_t b = 0; b < status.clients[i].supported_bandwidths.size(); ++b) {
            if (b > 0) json << ",";
            json << status.clients[i].supported_bandwidths[b];
        }
        json << "]}";
    }
    json << "]}";

    sendAll(client_fd, httpResponse("200 OK",
                                    "application/json; charset=utf-8",
                                    json.str()));
}

void WebServer::handleCfrApi(int client_fd, const std::string &method,
                             const std::string &body) {
    if (!cfr_controller_) {
        sendAll(client_fd,
                httpResponse("503 Service Unavailable",
                             "application/json; charset=utf-8",
                             "{\"error\":\"CFR controller not initialized\"}"));
        return;
    }

    if (method == "POST") {
        std::string action = "start";
        size_t pos = body.find("\"action\"");
        if (pos != std::string::npos) {
            size_t first_q = body.find('\"', pos + 8);
            if (first_q != std::string::npos) {
                size_t second_q = body.find('\"', first_q + 1);
                if (second_q != std::string::npos) {
                    action = body.substr(first_q + 1, second_q - first_q - 1);
                }
            }
        }

        if (action == "stop") {
            cfr_controller_->stopCapture();
            if (prober_) {
                prober_->setEnabled(false);
            }
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                has_frame_ = false;
                motion_detector_.reset();
                breath_detector_.reset();
            }
            sendAll(client_fd,
                    httpResponse("200 OK", "application/json; charset=utf-8",
                                 "{\"status\":\"ok\",\"running\":false}"));
            return;
        }

        if (action == "update_period") {
            int new_period = 50;
            size_t pos_p = body.find("\"period\"");
            if (pos_p != std::string::npos) {
                size_t col = body.find(':', pos_p);
                if (col != std::string::npos) {
                    try {
                        new_period = std::stoi(body.substr(col + 1, 10));
                    } catch (...) {}
                }
            }
            if (new_period >= 10 && new_period <= 2000) {
                cfr_controller_->updatePeriod(new_period);
                if (prober_) {
                    CFRCaptureStatus cstat = cfr_controller_->getStatus();
                    prober_->setConfig(true, cstat.target_ip, new_period, cstat.target_mac);
                }
            }
            sendAll(client_fd,
                    httpResponse("200 OK", "application/json; charset=utf-8",
                                 "{\"status\":\"ok\",\"period\":" + std::to_string(new_period) + "}"));
            return;
        }

        // Action: "start"
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            has_frame_ = false;
            motion_detector_.reset();
            breath_detector_.reset();
        }
        std::string mac;
        std::string iface;
        std::string ip;
        // Chu kỳ cố định 10ms (100 Hz) theo chuẩn Model V6 / M5R để đồng bộ toàn hệ thống
        int period = 10;
        int bandwidth = 0;
        int method_id = 0;
        bool auto_probe = true;

        // Trích xuất "mac"
        pos = body.find("\"mac\"");
        if (pos != std::string::npos) {
            size_t q1 = body.find('\"', pos + 5);
            if (q1 != std::string::npos) {
                size_t q2 = body.find('\"', q1 + 1);
                if (q2 != std::string::npos) {
                    mac = body.substr(q1 + 1, q2 - q1 - 1);
                }
            }
        }
        // Trích xuất "iface"
        pos = body.find("\"iface\"");
        if (pos != std::string::npos) {
            size_t q1 = body.find('\"', pos + 7);
            if (q1 != std::string::npos) {
                size_t q2 = body.find('\"', q1 + 1);
                if (q2 != std::string::npos) {
                    iface = body.substr(q1 + 1, q2 - q1 - 1);
                }
            }
        }
        // Trích xuất "ip"
        pos = body.find("\"ip\"");
        if (pos != std::string::npos) {
            size_t q1 = body.find('\"', pos + 4);
            if (q1 != std::string::npos) {
                size_t q2 = body.find('\"', q1 + 1);
                if (q2 != std::string::npos) {
                    ip = body.substr(q1 + 1, q2 - q1 - 1);
                }
            }
        }
        // Cố định chu kỳ 10ms (100 Hz) theo chuẩn Model V6 / M5R để đồng bộ toàn hệ thống
        period = 10;
        // Trích xuất "auto_probe": true/false
        pos = body.find("\"auto_probe\"");
        if (pos != std::string::npos) {
            size_t col = body.find(':', pos);
            if (col != std::string::npos) {
                std::string sub = body.substr(col + 1, 15);
                if (sub.find("false") != std::string::npos)
                    auto_probe = false;
                else if (sub.find("true") != std::string::npos)
                    auto_probe = true;
            }
        }

        // Trích xuất "bandwidth": 0, 1, 2, 3, 4
        bool has_bandwidth_param = false;
        pos = body.find("\"bandwidth\"");
        if (pos != std::string::npos) {
            size_t col = body.find(':', pos);
            if (col != std::string::npos) {
                try {
                    bandwidth = std::stoi(body.substr(col + 1, 10));
                    has_bandwidth_param = true;
                } catch (...) {}
            }
        }

        // Trích xuất "method" hoặc "method_id": 0, 1
        pos = body.find("\"method\"");
        if (pos == std::string::npos) {
            pos = body.find("\"method_id\"");
        }
        if (pos != std::string::npos) {
            size_t col = body.find(':', pos);
            if (col != std::string::npos) {
                try {
                    method_id = std::stoi(body.substr(col + 1, 10));
                } catch (...) {}
            }
        }

        // Nếu MAC trống hoặc "auto", tự động chọn thiết bị Wi-Fi tốt nhất
        // Ưu tiên:
        // 1. Thiết bị đang được chọn trong prober (nếu có target_ip cụ thể != "auto")
        // 2. Thiết bị trên băng tần 5GHz có sóng mạnh nhất (vì 5GHz hỗ trợ 80MHz và Gesture Model 1)
        // 3. Fallback sang 2.4GHz nếu không có thiết bị nào trên 5GHz
        if ((mac.empty() || mac == "auto") && prober_) {
            ProberStatus pstat = prober_->getStatus(false);
            std::vector<ClientInfo> clients = pstat.clients;
            if (clients.empty()) {
                clients = prober_->scanClients();
            }

            const ClientInfo *chosen_client = nullptr;

            // Nếu người dùng đã chọn 1 target_ip cụ thể trong prober (khác auto và rỗng)
            if (!ip.empty() && ip != "auto" && ip != "N/A") {
                for (const auto &c : clients) {
                    if (c.ip == ip || c.mac == ip) {
                        chosen_client = &c;
                        break;
                    }
                }
            } else if (!pstat.target_ips.empty() && !pstat.target_ips[0].empty() &&
                       pstat.target_ips[0] != "auto" && pstat.target_ips[0] != "N/A") {
                for (const auto &c : clients) {
                    if (c.ip == pstat.target_ips[0] || c.mac == pstat.target_ips[0]) {
                        chosen_client = &c;
                        break;
                    }
                }
            }

            if (!chosen_client) {
                sendAll(client_fd,
                        httpResponse("400 Bad Request", "application/json; charset=utf-8",
                                     "{\"error\":\"Chưa chọn thiết bị Wi-Fi để thu thập CSI. Vui lòng chọn thiết bị trên giao diện.\"}"));
                return;
            }

            mac = chosen_client->mac;
            iface = chosen_client->iface;
            ip = (chosen_client->ip != "N/A" && !chosen_client->ip.empty()) ? chosen_client->ip : "";
            if (!has_bandwidth_param && chosen_client->max_bandwidth > 0) {
                bandwidth = chosen_client->max_bandwidth;
            }
        }

        if (mac.empty()) {
            sendAll(client_fd,
                    httpResponse("400 Bad Request", "application/json; charset=utf-8",
                                 "{\"error\":\"Chưa chọn thiết bị Wi-Fi để thu thập CSI. Vui lòng chọn thiết bị trên giao diện.\"}"));
            return;
        }

        // Nếu capture ĐANG CHẠY cho đúng trạm này với cùng băng thông:
        // Không kill tiến trình và không restart CFR, chỉ cần kích hoạt wake-up burst bổ sung!
        CFRCaptureStatus cur_cfr = cfr_controller_->getStatus();
        if (cur_cfr.running && equalsIgnoreCase(cur_cfr.target_mac, mac) &&
            (bandwidth == 0 || bandwidth == cur_cfr.bandwidth)) {
            if (period > 0 && period != cur_cfr.period) {
                cfr_controller_->updatePeriod(period);
                cur_cfr.period = period;
            }
            if (auto_probe && prober_) {
                std::string burst_ip = (!ip.empty() && ip != "N/A") ? ip : cur_cfr.target_ip;
                if (burst_ip.empty()) burst_ip = prober_->findIpForMac(mac);
                if (!burst_ip.empty()) {
                    int p_interval = (period > 0) ? period : cur_cfr.period;
                    prober_->setConfig(true, burst_ip, p_interval, mac);
                    prober_->triggerBurst(burst_ip, mac, 15);
                }
            }
            std::ostringstream ss;
            ss << "{\"status\":\"ok\",\"running\":true"
               << ",\"target_mac\":\"" << cur_cfr.target_mac << "\""
               << ",\"target_ip\":\"" << cur_cfr.target_ip << "\""
               << ",\"iface\":\"" << cur_cfr.iface << "\""
               << ",\"phy\":\"" << cur_cfr.phy << "\""
               << ",\"hw\":\"" << cur_cfr.hw << "\""
               << ",\"mac_dev\":\"" << cur_cfr.mac_dev << "\""
               << ",\"bandwidth\":" << cur_cfr.bandwidth
               << ",\"period\":" << cur_cfr.period
               << ",\"latest_file\":\"" << cur_cfr.latest_file << "\""
               << ",\"latest_file_size\":" << cur_cfr.latest_file_size
               << ",\"pid\":" << cur_cfr.pid
               << ",\"uptime_sec\":" << cur_cfr.uptime_sec
               << ",\"available_bandwidths\":[";
            for (size_t b = 0; b < cur_cfr.available_bandwidths.size(); ++b) {
                if (b > 0) ss << ",";
                ss << cur_cfr.available_bandwidths[b];
            }
            ss << "]}";
            sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8", ss.str()));
            return;
        }

        // 1. Pre-warm: Đánh thức thiết bị trước bằng chuỗi gói thăm dò nếu auto_probe bật
        if (auto_probe && prober_) {
            ProberStatus pstat = prober_->getStatus(false);
            int probe_interval = (period > 0) ? period : pstat.interval_ms;
            std::string burst_ip = (!ip.empty() && ip != "N/A") ? ip : "";
            if (burst_ip.empty() && !mac.empty()) {
                burst_ip = prober_->findIpForMac(mac);
                if (!burst_ip.empty()) {
                    ip = burst_ip;
                }
            }
            if (!burst_ip.empty()) {
                prober_->setConfig(true, burst_ip, probe_interval, mac);
                // Bắn 10 gói ARP + UDP có khoảng nghỉ 10ms giữa các gói để đánh thức trạm êm dịu mà không làm nghẽn hàng đợi
                prober_->triggerBurst(burst_ip, mac, 10);
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
        }

        // 2. Kích hoạt thu CFR khi trạm đã thức giấc
        bool started = cfr_controller_->startCapture(mac, iface, ip, period, bandwidth, method_id);
        CFRCaptureStatus status = cfr_controller_->getStatus();

        if (started) {
            std::ostringstream ss;
            ss << "{\"status\":\"ok\",\"running\":true"
               << ",\"target_mac\":\"" << status.target_mac << "\""
               << ",\"target_ip\":\"" << status.target_ip << "\""
               << ",\"iface\":\"" << status.iface << "\""
               << ",\"phy\":\"" << status.phy << "\""
               << ",\"hw\":\"" << status.hw << "\""
               << ",\"mac_dev\":\"" << status.mac_dev << "\""
               << ",\"bandwidth\":" << status.bandwidth
               << ",\"period\":" << status.period
               << ",\"latest_file\":\"" << status.latest_file << "\""
               << ",\"latest_file_size\":" << status.latest_file_size
               << ",\"pid\":" << status.pid
               << ",\"uptime_sec\":" << status.uptime_sec
               << ",\"available_bandwidths\":[";
            for (size_t b = 0; b < status.available_bandwidths.size(); ++b) {
                if (b > 0) ss << ",";
                ss << status.available_bandwidths[b];
            }
            ss << "]}";
            sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8", ss.str()));
        } else {
            std::ostringstream ss;
            ss << "{\"status\":\"error\",\"running\":false"
               << ",\"error\":\"" << status.last_error << "\""
               << "}";
            sendAll(client_fd, httpResponse("500 Internal Server Error", "application/json; charset=utf-8", ss.str()));
        }
        return;
    }

    // GET: Trả về trạng thái hiện tại
    CFRCaptureStatus status = cfr_controller_->getStatus();
    if (!status.running) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (has_frame_) {
            has_frame_ = false;
            motion_detector_.reset();
            breath_detector_.reset();
            wave_detector_.reset();
        }
    }
    std::ostringstream ss;
    ss << "{\"running\":" << (status.running ? "true" : "false")
       << ",\"target_mac\":\"" << status.target_mac << "\""
       << ",\"target_ip\":\"" << status.target_ip << "\""
       << ",\"iface\":\"" << status.iface << "\""
       << ",\"phy\":\"" << status.phy << "\""
       << ",\"hw\":\"" << status.hw << "\""
       << ",\"mac_dev\":\"" << status.mac_dev << "\""
       << ",\"bandwidth\":" << status.bandwidth
       << ",\"period\":" << status.period
       << ",\"latest_file\":\"" << status.latest_file << "\""
       << ",\"latest_file_size\":" << status.latest_file_size
       << ",\"pid\":" << status.pid
       << ",\"uptime_sec\":" << status.uptime_sec
       << ",\"last_error\":\"" << status.last_error << "\""
       << ",\"available_bandwidths\":[";
    for (size_t b = 0; b < status.available_bandwidths.size(); ++b) {
        if (b > 0) ss << ",";
        ss << status.available_bandwidths[b];
    }
    ss << "]}";
    sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8", ss.str()));
}

void WebServer::handleCfrDownload(int client_fd) {
    if (!cfr_controller_) {
        sendAll(client_fd,
                httpResponse("503 Service Unavailable",
                             "application/json; charset=utf-8",
                             "{\"error\":\"CFR controller not initialized\"}"));
        return;
    }

    size_t file_size = 0;
    std::string filepath = cfr_controller_->getLatestCaptureFilePath(&file_size);
    if (filepath.empty()) {
        sendAll(client_fd,
                httpResponse("404 Not Found", "application/json; charset=utf-8",
                             "{\"error\":\"No capture file available for download\"}"));
        return;
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(filepath, ec) || !fs::is_regular_file(filepath, ec)) {
        sendAll(client_fd,
                httpResponse("404 Not Found", "application/json; charset=utf-8",
                             "{\"error\":\"Capture file does not exist on disk\"}"));
        return;
    }

    file_size = static_cast<size_t>(fs::file_size(filepath, ec));
    std::string filename = fs::path(filepath).filename().string();

    std::ostringstream header;
    header << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: application/octet-stream\r\n"
           << "Content-Disposition: attachment; filename=\"" << filename << "\"\r\n"
           << "Content-Length: " << file_size << "\r\n"
           << "Access-Control-Allow-Origin: *\r\n"
           << "Cache-Control: no-cache\r\n"
           << "Connection: close\r\n\r\n";

    if (!sendAll(client_fd, header.str())) {
        return;
    }

    std::ifstream file(filepath, std::ios::binary);
    if (!file) return;

    std::vector<char> buffer(65536);
    size_t remaining = file_size;
    while (remaining > 0 && file) {
        const size_t to_read = std::min(remaining, buffer.size());
        file.read(buffer.data(), static_cast<std::streamsize>(to_read));
        const std::streamsize bytes_read = file.gcount();
        if (bytes_read <= 0) break;
        if (!sendAllBytes(client_fd, buffer.data(), static_cast<size_t>(bytes_read))) {
            break;
        }
        remaining -= static_cast<size_t>(bytes_read);
    }
}

void WebServer::handleDistanceApi(int client_fd, const std::string &method,
                                  const std::string &path, const std::string &body) {
    if (method == "POST") {
        if (path == "/api/distance/calibrate") {
            float distance_m = 1.0f;
            size_t pos = body.find("\"distance_m\"");
            if (pos != std::string::npos) {
                size_t colon = body.find(':', pos);
                if (colon != std::string::npos) {
                    try {
                        distance_m = std::stof(body.substr(colon + 1));
                    } catch (...) {}
                }
            }

            float custom_n = 0.0f;
            size_t npos = body.find("\"n\"");
            if (npos != std::string::npos) {
                size_t colon = body.find(':', npos);
                if (colon != std::string::npos) {
                    try {
                        custom_n = std::stof(body.substr(colon + 1));
                    } catch (...) {}
                }
            }

            std::string mac;
            size_t mpos = body.find("\"mac\"");
            if (mpos != std::string::npos) {
                size_t q1 = body.find('\"', mpos + 5);
                if (q1 != std::string::npos) {
                    size_t q2 = body.find('\"', q1 + 1);
                    if (q2 != std::string::npos) {
                        mac = body.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
            }
            if (mac.empty() || mac == "auto" || mac == "DEFAULT") {
                if (cfr_controller_ && !cfr_controller_->getTargetMac().empty()) {
                    mac = cfr_controller_->getTargetMac();
                } else if (!last_target_mac_.empty()) {
                    mac = last_target_mac_;
                } else if (prober_) {
                    ProberStatus pstat = prober_->getStatus(false);
                    if (!pstat.clients.empty()) {
                        mac = pstat.clients[0].mac;
                    }
                }
            }
            if (mac.empty() || mac == "auto") {
                mac = "DEFAULT";
            }

            bool ok = distance_estimator_.calibrate(mac, distance_m, custom_n);
            if (ok) {
                std::ostringstream resp;
                resp << "{\"status\":\"ok\",\"mac\":\"" << mac
                     << "\",\"distance_m\":" << distance_m
                     << ",\"band\":\"" << distance_estimator_.getBandName() << "\""
                     << ",\"applied_a_1m\":" << distance_estimator_.getEffectiveA1m()
                     << ",\"applied_n\":" << distance_estimator_.getPathLossExponent()
                     << ",\"applied_env\":\"" << distance_estimator_.getEnvModeString() << "\""
                     << "}";
                sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8", resp.str()));
            } else {
                sendAll(client_fd, httpResponse("400 Bad Request", "application/json; charset=utf-8",
                                                "{\"error\":\"Failed to calibrate. Ensure target MAC and valid distance (>0.2m)\"}"));
            }
            return;
        } else if (path == "/api/distance/set_env") {
            std::string env = "los";
            size_t epos = body.find("\"env\"");
            if (epos != std::string::npos) {
                size_t q1 = body.find('\"', epos + 5);
                if (q1 != std::string::npos) {
                    size_t q2 = body.find('\"', q1 + 1);
                    if (q2 != std::string::npos) env = body.substr(q1 + 1, q2 - q1 - 1);
                }
            }
            float custom_n = 0.0f;
            size_t npos = body.find("\"n\"");
            if (npos != std::string::npos) {
                size_t colon = body.find(':', npos);
                if (colon != std::string::npos) {
                    try { custom_n = std::stof(body.substr(colon + 1)); } catch (...) {}
                }
            }
            distance_estimator_.setEnvModeString(env, custom_n);
            std::ostringstream resp;
            resp << "{\"status\":\"ok\",\"env\":\"" << distance_estimator_.getEnvModeString()
                 << "\",\"applied_n\":" << distance_estimator_.getPathLossExponent() << "}";
            sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8", resp.str()));
            return;
        } else if (path == "/api/distance/set_n") {
            float custom_n = 1.75f;
            size_t npos = body.find("\"n\"");
            if (npos != std::string::npos) {
                size_t colon = body.find(':', npos);
                if (colon != std::string::npos) {
                    try { custom_n = std::stof(body.substr(colon + 1)); } catch (...) {}
                }
            }
            distance_estimator_.setPathLossExponent(custom_n);
            std::ostringstream resp;
            resp << "{\"status\":\"ok\",\"applied_n\":" << distance_estimator_.getPathLossExponent() << "}";
            sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8", resp.str()));
            return;
        } else if (path == "/api/distance/reset_cal") {
            std::string mac;
            size_t mpos = body.find("\"mac\"");
            if (mpos != std::string::npos) {
                size_t q1 = body.find('\"', mpos + 5);
                if (q1 != std::string::npos) {
                    size_t q2 = body.find('\"', q1 + 1);
                    if (q2 != std::string::npos) {
                        mac = body.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
            }
            if (mac.empty() || mac == "auto") {
                if (cfr_controller_ && !cfr_controller_->getTargetMac().empty()) {
                    mac = cfr_controller_->getTargetMac();
                } else if (!last_target_mac_.empty()) {
                    mac = last_target_mac_;
                }
            }
            distance_estimator_.resetCalibration(mac);
            sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8", "{\"status\":\"ok\",\"message\":\"Calibration reset\"}"));
            return;
        }
    } else if (method == "GET") {
        if (path == "/api/distance/calibrations") {
            auto list = distance_estimator_.getAllCalibrations();
            std::ostringstream resp;
            resp << "{\"calibrations\":[";
            for (size_t i = 0; i < list.size(); ++i) {
                if (i > 0) resp << ",";
                resp << "{\"mac\":\"" << list[i].mac
                     << "\",\"band\":\"" << list[i].band
                     << "\",\"a_1m\":" << list[i].a_1m
                     << ",\"n\":" << list[i].n
                     << ",\"dist\":" << list[i].calibrated_at_dist_m
                     << ",\"ts\":" << list[i].timestamp_sec << "}";
            }
            resp << "]}";
            sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8", resp.str()));
            return;
        }
    }

    sendAll(client_fd, httpResponse("404 Not Found", "application/json; charset=utf-8", "{\"error\":\"Unknown distance API\"}"));
}

void WebServer::handleV6Api(int client_fd, const std::string &method,
                            const std::string &path, const std::string &body) {
    (void)method;
    (void)body;

    if (path == "/api/v6/devices") {
        std::ostringstream ss;
        ss << "[";
        if (prober_) {
            ProberStatus ps = prober_->getStatus(false);
            bool first = true;
            for (const auto &c : ps.clients) {
                if (c.mac.empty()) continue;
                if (!first) ss << ",";
                first = false;
                ss << "{\"mac\":\"" << c.mac << "\",\"ip\":\"" << c.ip << "\",\"hostname\":\"" << escapeJsonString(c.hostname) << "\"}";
            }
        }
        ss << "]";
        sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8", ss.str()));
        return;
    }

    if (path == "/api/v6/score" || path == "/api/v6/status") {
        std::string target_mac = cfr_controller_ ? cfr_controller_->getTargetMac() : "";
        std::ostringstream ss;
        ss << "{"
           << "\"status\":\"ok\""
           << ",\"mode\":\"standalone_8082\""
           << ",\"port\":8082"
           << ",\"target_mac\":\"" << escapeJsonString(target_mac) << "\""
           << "}";
        sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8", ss.str()));
        return;
    }

    if (path == "/api/v6/start" || path == "/api/v6/stop" || path == "/api/v6/reset") {
        sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8",
                                        "{\"status\":\"ok\",\"mode\":\"standalone_8082\",\"port\":8082}"));
        return;
    }

    if (path == "/api/v6/sensitivity" || path == "/api/v6/config") {
        sendAll(client_fd, httpResponse("200 OK", "application/json; charset=utf-8",
                                        "{\"status\":\"ok\",\"mode\":\"standalone_8082\",\"threshold_high\":0.80,\"threshold_low\":0.65}"));
        return;
    }

    sendAll(client_fd, httpResponse("404 Not Found", "application/json; charset=utf-8",
                                    "{\"error\":\"Unknown V6 API endpoint\"}"));
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
    // Thiết lập timeout gửi và tắt Nagle's algorithm để phản hồi tức thì, tránh ứ đọng Send-Q khi Wi-Fi lag
    struct timeval snd_tv;
    snd_tv.tv_sec = 2;
    snd_tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&snd_tv, sizeof(snd_tv));
    int one = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    const std::string headers = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/event-stream\r\n"
                                "Cache-Control: no-cache\r\n"
                                "Connection: keep-alive\r\n"
                                "X-Accel-Buffering: no\r\n\r\n";
    if (!sendAll(client_fd, headers))
        return;

    std::cout << "[Web] SSE client stream connected (fd=" << client_fd << ")\n";

    constexpr int64_t kPaceMs = 20; // ~50 FPS (20ms) - nhịp mượt mà, triệt tiêu nghẽn socket TCP và quá tải JSON parse
    uint64_t last_sequence = 0;
    auto last_send_time = std::chrono::steady_clock::now();

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
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - last_send_time).count();

            if (elapsed_ms < kPaceMs) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(std::max<int64_t>(1, kPaceMs - elapsed_ms)));
                // Lấy frame mới nhất sau khi ngủ nhịp bù
                {
                    std::lock_guard<std::mutex> lock(data_mutex_);
                    if (!is_running_)
                        break;
                    has_frame = has_frame_;
                    current_sequence = sequence_;
                    if (has_frame)
                        frame = latest_frame_;
                }
            }

            last_send_time = std::chrono::steady_clock::now();
            last_sequence = current_sequence;
            const std::string event =
                "id: " + std::to_string(current_sequence) +
                "\n"
                "event: csi\n"
                "data: " +
                frame.toJson(false, false) + "\n\n";
            if (!sendAll(client_fd, event))
                break;
        } else if (!sendAll(client_fd, ": keep-alive\n\n")) {
            break;
        }
    }

    std::cout << "[Web] SSE client stream disconnected (fd=" << client_fd << ")\n";
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
