#ifndef DRIVER_AUDIT_LOGGER_HPP
#define DRIVER_AUDIT_LOGGER_HPP

#include <chrono>
#include <fstream>
#include <mutex>
#include <string>

class DriverAuditLogger {
public:
    static DriverAuditLogger &instance();

    // Khởi tạo logger: remote_ip (tùy chọn gửi UDP về laptop), log_file
    void init(const std::string &remote_ip = "", int remote_port = 5140,
              const std::string &log_file = "/root/edge_csi_audit.log");

    // Ghi một sự kiện tương tác driver / phần cứng
    void log(const std::string &category, const std::string &action,
             const std::string &target, const std::string &detail, int error_code = 0);

    // Helper chuyên dụng ghi sysfs/debugfs
    void logSysfsWrite(const std::string &path, const std::string &value,
                       bool success, int error_code = 0);

    // Helper chuyên dụng ghi tiến trình (fork, exec, kill, waitpid)
    void logProcess(const std::string &action, int pid, const std::string &detail);

    // Đóng và flush toàn bộ tài nguyên
    void close();

private:
    DriverAuditLogger();
    ~DriverAuditLogger();

    DriverAuditLogger(const DriverAuditLogger &) = delete;
    DriverAuditLogger &operator=(const DriverAuditLogger &) = delete;

    std::mutex mutex_;
    int kmsg_fd_ = -1;
    int udp_fd_ = -1;
    std::string remote_ip_;
    int remote_port_ = 5140;
    std::string log_file_ = "/root/edge_csi_audit.log";
    std::ofstream file_stream_;
    size_t current_file_size_ = 0;
    bool initialized_ = false;

    static constexpr size_t kMaxLogSize = 512 * 1024; // 512 KB rotating limit on persistent flash (/overlay)

    void rotateLocked();
    void writeKmsgLocked(const std::string &line);
    void writeUdpLocked(const std::string &line);
    void writeFileLocked(const std::string &line);

    static std::string getTimestampMicro();
};

#endif // DRIVER_AUDIT_LOGGER_HPP
