#include "link_stability_estimator.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
template <typename T>
T clampVal(T val, T low, T high) {
    return std::max(low, std::min(val, high));
}
} // namespace

LinkStabilityEstimator::LinkStabilityEstimator() {
    reset();
}

void LinkStabilityEstimator::reset() {
    prev_amplitudes_.clear();
    prev_rssi_dbm_ = 0;
    prev_agc_[0] = 0;
    prev_agc_[1] = 0;
    prev_agc_[2] = 0;
    has_prev_frame_ = false;

    amplitude_history_.clear();
    smoothed_score_ = 92.0f;
    score_initialized_ = false;
    current_status_ = LinkStabilityStatus::Calibrating;
}

float LinkStabilityEstimator::computePearsonCorrelation(
    const std::vector<float> &a, const std::vector<float> &b) {
    const size_t n = std::min(a.size(), b.size());
    if (n < 8)
        return 1.0f;

    double sum_a = 0.0;
    double sum_b = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum_a += a[i];
        sum_b += b[i];
    }
    const double mean_a = sum_a / static_cast<double>(n);
    const double mean_b = sum_b / static_cast<double>(n);

    double num = 0.0;
    double denom_a = 0.0;
    double denom_b = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double da = static_cast<double>(a[i]) - mean_a;
        const double db = static_cast<double>(b[i]) - mean_b;
        num += da * db;
        denom_a += da * da;
        denom_b += db * db;
    }

    const double denom = std::sqrt(denom_a * denom_b);
    if (denom < 1e-9)
        return 1.0f;

    return static_cast<float>(clampVal(num / denom, -1.0, 1.0));
}

float LinkStabilityEstimator::computeDeepFadeRatio(
    const std::vector<float> &amplitudes, float &out_mean, float &out_min) {
    if (amplitudes.empty()) {
        out_mean = 0.0f;
        out_min = 0.0f;
        return 1.0f;
    }

    // Lọc bỏ các sóng mang con DC (0 Hz) và guard tones có biên độ = 0 hoặc tiệm cận 0
    std::vector<float> active_amps;
    active_amps.reserve(amplitudes.size());
    double sum = 0.0;
    for (float val : amplitudes) {
        if (val > 1.0f) { // Bỏ qua sóng mang con DC/null của driver Wi-Fi
            active_amps.push_back(val);
            sum += val;
        }
    }

    if (active_amps.empty()) {
        out_mean = 0.0f;
        out_min = 0.0f;
        return 1.0f;
    }

    out_mean = static_cast<float>(sum / static_cast<double>(active_amps.size()));

    // Dùng phân vị thứ 5 (5th percentile) để tìm vết lõm suy hao sâu thực sự
    // tránh trường hợp 1-2 tone đơn lẻ ở mép kênh bị nhiễu tạo false alarm
    std::sort(active_amps.begin(), active_amps.end());
    const size_t p5_idx = active_amps.size() * 5 / 100;
    out_min = active_amps[p5_idx];

    const float effective_min = std::max(out_min, 1.0f);
    return out_mean / effective_min;
}

void LinkStabilityEstimator::process(CSIData &frame) {
    if (frame.chains.empty() || frame.chains[0].amplitudes.empty())
        return;

    const auto &amps = frame.chains[0].amplitudes;

    // 1. Phân tích hiện tượng suy hao chọn lọc tần số (Deep Fade)
    float mean_amp = 0.0f;
    float min_amp = 0.0f;
    const float fade_ratio = computeDeepFadeRatio(amps, mean_amp, min_amp);
    frame.deep_fade_ratio = fade_ratio;
    // Ngưỡng Deep Fade: Mean / Min >= 4.5x (tương đương biên độ điểm cực tiểu sụt > 13 dB so với trung bình)
    frame.deep_fade_warning = (fade_ratio >= 4.5f);

    // 2. Tính hệ số tương quan vân phổ Pearson và bóc tách bước nhảy mạch AGC
    float spectral_corr = 1.0f;
    bool is_agc_event = false;

    if (has_prev_frame_ && !prev_amplitudes_.empty()) {
        spectral_corr = computePearsonCorrelation(amps, prev_amplitudes_);

        const int delta_rssi = std::abs(static_cast<int>(frame.rssi_comb_db) -
                                        static_cast<int>(prev_rssi_dbm_));
        const bool agc_changed =
            (frame.agc_indices[0] != prev_agc_[0] ||
             frame.agc_indices[1] != prev_agc_[1] ||
             frame.agc_indices[2] != prev_agc_[2]);

        // Nếu nấc AGC đổi hoặc RSSI nhảy bước >= 2 dB nhưng hình dạng vân phổ CSI giữ nguyên (r >= 0.90)
        // -> Khẳng định kết nối thực tế không hề suy giảm, chỉ là bước nhảy khuếch đại phần cứng!
        if ((agc_changed || delta_rssi >= 2) && spectral_corr >= 0.90f) {
            is_agc_event = true;
        }
    }

    frame.spectral_correlation = spectral_corr;
    frame.is_agc_event = is_agc_event;

    // 3. Phân tích phương sai biến thiên biên độ theo thời gian (Temporal Variance)
    amplitude_history_.push_back(mean_amp);
    if (amplitude_history_.size() > kHistoryCapacity) {
        amplitude_history_.pop_front();
    }

    float temporal_variance = 0.0f;
    if (amplitude_history_.size() >= 8) {
        double hist_sum = 0.0;
        for (float v : amplitude_history_) {
            hist_sum += v;
        }
        const double hist_mean =
            hist_sum / static_cast<double>(amplitude_history_.size());

        if (hist_mean > 1e-4) {
            double var_sum = 0.0;
            for (float v : amplitude_history_) {
                const double norm_diff = (static_cast<double>(v) - hist_mean) / hist_mean;
                var_sum += norm_diff * norm_diff;
            }
            temporal_variance = static_cast<float>(
                var_sum / static_cast<double>(amplitude_history_.size()));
        }
    }
    frame.temporal_csi_variance = temporal_variance;

    // 4. Lấy chỉ số Rician K và tỷ lệ tia truyền thẳng LoS
    const float los_ratio =
        frame.csi_los_ratio > 0.0f ? frame.csi_los_ratio : 0.5f;
    const float k_rician =
        frame.csi_rician_k > 0.0f ? frame.csi_rician_k : 1.5f;

    // 5. Phân loại trạng thái liên kết
    if (amplitude_history_.size() < 8) {
        current_status_ = LinkStabilityStatus::Calibrating;
        frame.link_status_text = "calibrating";
    } else if (frame.rssi_comb_db < -84) {
        current_status_ = LinkStabilityStatus::WeakSignal;
        frame.link_status_text = "weak";
    } else if (temporal_variance > 0.060f) {
        // Môi trường xung quanh có người/vật thể di chuyển lớn làm nhiễu sóng đa đường
        current_status_ = LinkStabilityStatus::DynamicInterference;
        frame.link_status_text = "dynamic_motion";
    } else if (frame.deep_fade_warning) {
        // Thiết bị đứng yên nhưng nằm trúng vết lõm suy hao chọn lọc tần số thực sự
        current_status_ = LinkStabilityStatus::DeepFadeWarning;
        frame.link_status_text = "deep_fade";
    } else if (k_rician >= 2.0f && los_ratio >= 0.20f) {
        // Tia truyền thẳng trực tiếp chiếm ưu thế
        current_status_ = LinkStabilityStatus::CleanLoS;
        frame.link_status_text = "clean_los";
    } else {
        // Kênh truyền phản xạ tĩnh nhưng ổn định
        current_status_ = LinkStabilityStatus::MultipathStable;
        frame.link_status_text = "multipath_stable";
    }

    // 6. Tính toán điểm ổn định & chất lượng tổng hợp (Quality & Stability Score 0 - 100)
    // 6.1. Thành phần công suất thu RSSI (30% trọng số): chuẩn hóa từ -85 dBm (0đ) đến -40 dBm (100đ)
    float raw_rssi = static_cast<float>(frame.rssi_comb_db);
    if (raw_rssi > 0.0f) {
        raw_rssi -= 95.0f; // nếu driver trả về SNR dương
    } else if (raw_rssi == 0.0f) {
        // Nếu driver chưa có WMI RSSI, ước tính RSSI động từ chính biên độ CSI và gain ratio phần cứng
        if (!amps.empty()) {
            float sum_sq = 0.0f;
            for (float a : amps) sum_sq += a * a;
            const float mean_sq = sum_sq / std::max(1.0f, static_cast<float>(amps.size()));
            const float csi_pwr = 10.0f * std::log10(mean_sq + 1.0f);
            const float gain_ratio = static_cast<float>(frame.amplitude_gain_ratio[0]);
            const float gain_comp = 20.0f * std::log10(128.0f / std::max(1.0f, gain_ratio));
            raw_rssi = clampVal(-88.0f + csi_pwr - gain_comp, -85.0f, -40.0f);
        } else {
            raw_rssi = -50.0f; // fallback trung bình
        }
    }
    const float score_rssi =
        clampVal((raw_rssi - (-85.0f)) / 45.0f, 0.0f, 1.0f) * 100.0f;

    // 6.2. Thành phần tỷ lệ tia truyền thẳng LoS (25% trọng số): chuẩn hóa từ 0.10 đến 0.35
    const float score_los =
        clampVal((los_ratio - 0.10f) / 0.25f, 0.0f, 1.0f) * 100.0f;

    // 6.3. Thành phần hệ số Rician K (20% trọng số): chuẩn hóa từ K=0 đến K=3.0
    const float score_rician =
        clampVal(k_rician / 3.0f, 0.0f, 1.0f) * 100.0f;

    // 6.4. Thành phần độ tĩnh thời gian (25% trọng số): nâng trần dung sai phương sai lên 0.12 (phù hợp cả khi cầm tay)
    const float score_stillness =
        (1.0f - clampVal(temporal_variance / 0.12f, 0.0f, 1.0f)) * 100.0f;

    // Điểm tổng hợp cơ sở: 30% RSSI + 25% LoS + 20% Rician K + 25% Độ tĩnh
    float raw_score =
        0.30f * score_rssi + 0.25f * score_los + 0.20f * score_rician + 0.25f * score_stillness;

    // Trừ điểm nếu bị Deep Fade thực sự (suy hao chọn lọc tần số > 13 dB)
    if (frame.deep_fade_warning) {
        const float penalty =
            clampVal((fade_ratio - 4.5f) / 5.0f, 0.0f, 1.0f) * 20.0f;
        raw_score -= penalty;
    }

    raw_score = clampVal(raw_score, 10.0f, 100.0f);

    // Làm mượt bằng Exponential Moving Average (EMA) kết hợp Deadband chống rung giật
    if (!score_initialized_) {
        smoothed_score_ = raw_score;
        score_initialized_ = true;
    } else {
        // Vùng chết Deadband: 0.10f giúp nhận diện các vi biến động thực tế khi người chuyển động / vẫy tay
        const float diff = std::abs(raw_score - smoothed_score_);
        if (diff >= 0.10f) {
            // Hệ số alpha (0.18 bình thường, 0.04 khi AGC Hop) giúp kim chuyển động sống động, phản hồi nhanh
            const float alpha = is_agc_event ? 0.04f : 0.18f;
            smoothed_score_ = (1.0f - alpha) * smoothed_score_ + alpha * raw_score;
        }
    }

    frame.link_stability_score = smoothed_score_;

    // 7. Cập nhật khung lịch sử
    prev_amplitudes_ = amps;
    prev_rssi_dbm_ = frame.rssi_comb_db;
    prev_agc_[0] = frame.agc_indices[0];
    prev_agc_[1] = frame.agc_indices[1];
    prev_agc_[2] = frame.agc_indices[2];
    has_prev_frame_ = true;
}
