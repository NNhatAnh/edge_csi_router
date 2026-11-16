#ifndef LINK_STABILITY_ESTIMATOR_HPP
#define LINK_STABILITY_ESTIMATOR_HPP

#include "csi_parser.hpp"
#include <deque>
#include <vector>

enum class LinkStabilityStatus {
    Calibrating,         // Đang tích lũy các khung ban đầu
    CleanLoS,            // Sóng truyền thẳng sạch, cực kỳ ổn định
    MultipathStable,     // Ổn định ở chế độ phản xạ đa đường tĩnh
    DeepFadeWarning,     // Bị lõm chọn lọc tần số (nguy cơ lỗi bit ở MCS cao)
    DynamicInterference, // Môi trường xung quanh có người/vật thể di chuyển
    WeakSignal           // Tín hiệu quá yếu (suy hao cự ly lớn hoặc chắn hoàn toàn)
};

class LinkStabilityEstimator {
  public:
    LinkStabilityEstimator();

    void process(CSIData &frame);
    void reset();

    LinkStabilityStatus getStatus() const { return current_status_; }
    float getSmoothedScore() const { return smoothed_score_; }

  private:
    float computePearsonCorrelation(const std::vector<float> &a,
                                    const std::vector<float> &b);
    float computeDeepFadeRatio(const std::vector<float> &amplitudes,
                               float &out_mean, float &out_min);

    // Lưu trữ khung trước để tính tương quan phổ và phát hiện AGC Hop
    std::vector<float> prev_amplitudes_;
    float prev_rssi_dbm_ = 0.0f;
    uint8_t prev_agc_[3]{0, 0, 0};
    bool has_prev_frame_ = false;

    // Bộ đệm trượt đo phương sai biến thiên biên độ theo thời gian (Temporal Variance)
    std::deque<float> amplitude_history_;
    static constexpr size_t kHistoryCapacity = 30; // 30 frames (~1.5s tại 20Hz)

    // Điểm số làm mượt EMA
    float smoothed_score_ = 90.0f;
    bool score_initialized_ = false;

    LinkStabilityStatus current_status_ = LinkStabilityStatus::Calibrating;
};

#endif // LINK_STABILITY_ESTIMATOR_HPP
