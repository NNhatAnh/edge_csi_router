#include "driver_audit_logger.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

DriverAuditLogger &DriverAuditLogger::instance() {
    static DriverAuditLogger logger;
    return logger;
}

DriverAuditLogger::DriverAuditLogger() {
    init("", 5140, "/root/edge_csi_audit.log");
}

DriverAuditLogger::~DriverAuditLogger() {
    close();
}

std::string DriverAuditLogger::getTimestampMicro() {
    struct timeval tv {};
    gettimeofday(&tv, nullptr);
    struct tm tm_info {};
    localtime_r(&tv.tv_sec, &tm_info);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
                  tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
                  tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
                  static_cast<long>(tv.tv_usec));
    return std::string(buf);
}

void DriverAuditLogger::init(const std::string &remote_ip, int remote_port,
                             const std::string &log_file) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Mở /dev/kmsg nếu chưa mở
    if (kmsg_fd_ < 0) {
        kmsg_fd_ = open("/dev/kmsg", O_WRONLY | O_NONBLOCK);
    }

    // 2. Mở hoặc cập nhật socket UDP
    if (remote_ip_ != remote_ip || remote_port_ != remote_port) {
        if (udp_fd_ >= 0) {
            ::close(udp_fd_);
            udp_fd_ = -1;
        }
        remote_ip_ = remote_ip;
        remote_port_ = remote_port;
        if (!remote_ip_.empty()) {
            udp_fd_ = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
        }
    }

    // 3. Mở hoặc chuyển đổi file log cục bộ
    if (!initialized_ || log_file_ != log_file || !file_stream_.is_open()) {
        if (file_stream_.is_open()) {
            file_stream_.flush();
            file_stream_.close();
        }
        log_file_ = log_file.empty() ? "/root/edge_csi_audit.log" : log_file;
        try {
            namespace fs = std::filesystem;
            std::error_code ec;
            if (fs::exists(log_file_, ec)) {
                current_file_size_ = fs::file_size(log_file_, ec);
            } else {
                current_file_size_ = 0;
            }
            file_stream_.open(log_file_, std::ios::out | std::ios::app);
        } catch (...) {
            current_file_size_ = 0;
        }
    }

    initialized_ = true;

    // Ghi sự kiện khởi động
    std::string start_msg = "[AUDIT] [SYSTEM] [INIT] Logger initialized. Target: " +
                            log_file_ + (remote_ip_.empty() ? "" : (" Remote UDP: " + remote_ip_ + ":" + std::to_string(remote_port_)));
    writeFileLocked(getTimestampMicro() + " " + start_msg);
    writeKmsgLocked("<5>" + start_msg); // Level NOTICE in printk
}

void DriverAuditLogger::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_stream_.is_open()) {
        file_stream_.flush();
        file_stream_.close();
    }
    if (kmsg_fd_ >= 0) {
        ::close(kmsg_fd_);
        kmsg_fd_ = -1;
    }
    if (udp_fd_ >= 0) {
        ::close(udp_fd_);
        udp_fd_ = -1;
    }
    initialized_ = false;
}

void DriverAuditLogger::rotateLocked() {
    if (current_file_size_ < kMaxLogSize) {
        return;
    }

    if (file_stream_.is_open()) {
        file_stream_.flush();
        file_stream_.close();
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    std::string old_file = log_file_ + ".old";
    fs::remove(old_file, ec);
    fs::rename(log_file_, old_file, ec);

    file_stream_.open(log_file_, std::ios::out | std::ios::trunc);
    current_file_size_ = 0;
}

void DriverAuditLogger::writeKmsgLocked(const std::string &line) {
    if (kmsg_fd_ >= 0) {
        std::string payload = line + "\n";
        ssize_t ret = write(kmsg_fd_, payload.c_str(), payload.size());
        (void)ret;
    }
}

void DriverAuditLogger::writeUdpLocked(const std::string &line) {
    if (udp_fd_ >= 0 && !remote_ip_.empty()) {
        sockaddr_in dest{};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(static_cast<uint16_t>(remote_port_));
        if (inet_pton(AF_INET, remote_ip_.c_str(), &dest.sin_addr) > 0) {
            std::string payload = line + "\n";
            sendto(udp_fd_, payload.c_str(), payload.size(), 0,
                   reinterpret_cast<sockaddr *>(&dest), sizeof(dest));
        }
    }
}

void DriverAuditLogger::writeFileLocked(const std::string &line) {
    if (file_stream_.is_open()) {
        file_stream_ << line << "\n";
        file_stream_.flush();
        current_file_size_ += line.size() + 1;
        rotateLocked();
    }
}

void DriverAuditLogger::log(const std::string &category, const std::string &action,
                            const std::string &target, const std::string &detail,
                            int error_code) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string ts = getTimestampMicro();

    std::ostringstream oss;
    oss << "[" << category << "] [" << action << "] target=" << target;
    if (!detail.empty()) {
        oss << " detail=\"" << detail << "\"";
    }
    if (error_code != 0) {
        oss << " errno=" << error_code << " (" << std::strerror(error_code) << ")";
    }

    std::string body = oss.str();
    std::string file_line = ts + " " + body;
    std::string kmsg_line = "<5>[EdgeCSI] " + body; // Printk level 5 (NOTICE)

    writeFileLocked(file_line);
    writeKmsgLocked(kmsg_line);
    writeUdpLocked(file_line);
}

void DriverAuditLogger::logSysfsWrite(const std::string &path,
                                      const std::string &value, bool success,
                                      int error_code) {
    std::string action = success ? "WRITE_OK" : "WRITE_FAIL";
    std::string detail = "val=\"" + value + "\"";
    log("SYSFS", action, path, detail, success ? 0 : error_code);
}

void DriverAuditLogger::logProcess(const std::string &action, int pid,
                                   const std::string &detail) {
    log("PROC", action, "PID=" + std::to_string(pid), detail, 0);
}
