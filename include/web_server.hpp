#ifndef WEB_SERVER_HPP
#define WEB_SERVER_HPP

#include "csi_parser.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

class WebServer
{
public:
    WebServer(int web_port, int ingest_port, std::string web_root);
    ~WebServer();

    bool start();
    void stop();
    void publish(const CSIData &frame);

private:
    int web_port_;
    int ingest_port_;
    std::string web_root_;
    std::atomic<int> web_fd_;
    std::atomic<int> ingest_fd_;
    std::atomic<bool> is_running_;

    std::thread web_thread_;
    std::thread ingest_thread_;
    std::mutex workers_mutex_;
    std::condition_variable workers_changed_;
    size_t active_workers_ = 0;
    std::unordered_set<int> clients_;

    std::mutex data_mutex_;
    std::condition_variable data_changed_;
    MotionDetector motion_detector_;
    CSIData latest_frame_;
    uint64_t sequence_ = 0;
    bool has_frame_ = false;

    void runWebServer();
    void runIngestServer();
    void resetMotionCalibration();
    void handleWebClient(int client_fd);
    void handleIngestClient(int client_fd);
    void handleEventStream(int client_fd);
    void startClientWorker(int client_fd, bool web_client);

    std::string readFile(const std::string &filepath) const;
    std::string getLocalIP() const;
};

#endif
