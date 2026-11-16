#include "link_stability_estimator.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
template <typename T>
T clampVal(T val, T low, T high) {
    return std::max(low, std::min(val, high));
}

// Bảng ánh xạ chất lượng sóng chuẩn theo đồ thị đồng hồ (Wi-Fi Signal Speedometer Gauge):
// >= -30 dBm: 100% (Maximum Signal)
//    -50 dBm: 85% - 90% (Excellent Signal Strength)
//    -60 dBm: 70% - 75% (Good Signal Strength)
//    -67 dBm: 50% (Reliable Signal Strength Minimum)
//    -70 dBm: 40% (Not a Strong Signal)
//    -80 dBm: 20% (Unreliable Signal Strength)
// <= -90 dBm:  0% (Probably no Connection)
float computeRssiQualityScore(float rssi_dbm) {
    if (rssi_dbm >= -30.0f) {
        return 100.0f;
    }
    if (rssi_dbm >= -50.0f) {
        // Tuyến tính từ -50 dBm (85đ) lên -30 dBm (100đ)
        return 85.0f + ((rssi_dbm - (-50.0f)) / 20.0f) * 15.0f;
    }
    if (rssi_dbm >= -60.0f) {
        // Tuyến tính từ -60 dBm (70đ) lên -50 dBm (85đ)
        return 70.0f + ((rssi_dbm - (-60.0f)) / 10.0f) * 15.0f;
    }
    if (rssi_dbm >= -67.0f) {
        // Tuyến tính từ -67 dBm (50đ) lên -60 dBm (70đ)
        return 50.0f + ((rssi_dbm - (-67.0f)) / 7.0f) * 20.0f;
    }
    if (rssi_dbm >= -70.0f) {
        // Tuyến tính từ -70 dBm (40đ) lên -67 dBm (50đ)
        return 40.0f + ((rssi_dbm - (-70.0f)) / 3.0f) * 10.0f;
    }
    if (rssi_dbm >= -80.0f) {
        // Tuyến tính từ -80 dBm (20đ) lên -70 dBm (40đ)
        return 20.0f + ((rssi_dbm - (-80.0f)) / 10.0f) * 20.0f;
    }
    if (rssi_dbm >= -90.0f) {
        // Tuyến tính từ -90 dBm (0đ) lên -80 dBm (20đ)
        return 0.0f + ((rssi_dbm - (-90.0f)) / 10.0f) * 20.0f;
    }
    return 0.0f;
}
} // namespace

LinkStabilityEstimator::LinkStabilityEstimator() {
    reset();
}

void LinkStabilityEstimator::reset() {
    prev_amplitudes_.clear();
    prev_rssi_dbm_ = 0.0f;
    prev_agc_[0] = 0;
    prev_agc_[1] = 0;
    prev_agc_[2] = 0;
    has_prev_frame_ = false;

    amplitude_history_.clear();
    smoothed_score_ = 90.0f;
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

    // 0. Xác định công suất thu RSSI thực tế của thiết bị:
    // Ưu tiên 1: frame.smoothed_rssi_db do DistanceEstimator tính toán (từ kernel prober / iw station get)
    // Ưu tiên 2: frame.rssi_comb_db từ CFR header (nếu hợp lệ)
    // Ưu tiên 3: Ước tính từ biên độ CSI và mạch AGC khuếch đại
    float effective_rssi = 0.0f;
    if (frame.smoothed_rssi_db < -5.0f && frame.smoothed_rssi_db > -110.0f) {
        effective_rssi = frame.smoothed_rssi_db;
    } else if (frame.rssi_comb_db < 0 && frame.rssi_comb_db > -110) {
        effective_rssi = static_cast<float>(frame.rssi_comb_db);
    } else if (frame.rssi_comb_db > 0 && frame.rssi_comb_db < 100) {
        effective_rssi = static_cast<float>(frame.rssi_comb_db) - 95.0f;
    } else {
        if (!amps.empty()) {
            float sum_sq = 0.0f;
            for (float a : amps) sum_sq += a * a;
            const float mean_sq = sum_sq / std::max(1.0f, static_cast<float>(amps.size()));
            const float csi_pwr = 10.0f * std::log10(mean_sq + 1.0f);
            const float gain_ratio = static_cast<float>(frame.amplitude_gain_ratio[0]);
            const float gain_comp = 20.0f * std::log10(128.0f / std::max(1.0f, gain_ratio));
            const float agc_comp = 0.85f * static_cast<float>(frame.agc_indices[0]);
            effective_rssi = clampVal(-95.0f + csi_pwr + gain_comp - agc_comp, -85.0f, -30.0f);
        } else {
            effective_rssi = -55.0f;
        }
    }

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

        const float delta_rssi = std::abs(effective_rssi - prev_rssi_dbm_);
        const bool agc_changed =
            (frame.agc_indices[0] != prev_agc_[0] ||
             frame.agc_indices[1] != prev_agc_[1] ||
             frame.agc_indices[2] != prev_agc_[2]);

        // Nếu nấc AGC đổi hoặc RSSI nhảy bước >= 2 dB nhưng hình dạng vân phổ CSI giữ nguyên (r >= 0.90)
        // -> Khẳng định kết nối thực tế không hề suy giảm, chỉ là bước nhảy khuếch đại phần cứng!
        if ((agc_changed || delta_rssi >= 2.0f) && spectral_corr >= 0.90f) {
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
    if (amplitude_history_.size() < 5) {
        current_status_ = LinkStabilityStatus::Calibrating;
        frame.link_status_text = "calibrating";
    } else if (effective_rssi < -78.0f) {
        current_status_ = LinkStabilityStatus::WeakSignal;
        frame.link_status_text = "weak";
    } else if (effective_rssi >= -52.0f && spectral_corr >= 0.85f) {
        // Tín hiệu rất mạnh (ở gần router) và vân phổ ổn định -> Sóng truyền thẳng trực tiếp Clean LoS
        current_status_ = LinkStabilityStatus::CleanLoS;
        frame.link_status_text = "clean_los";
    } else if (temporal_variance > 0.080f) {
        // Môi trường xung quanh có người/vật thể di chuyển lớn làm biến thiên đa đường
        current_status_ = LinkStabilityStatus::DynamicInterference;
        frame.link_status_text = "dynamic_motion";
    } else if (frame.deep_fade_warning) {
        // Thiết bị đứng yên nhưng nằm trúng vết lõm suy hao chọn lọc tần số thực sự
        current_status_ = LinkStabilityStatus::DeepFadeWarning;
        frame.link_status_text = "deep_fade";
    } else if (k_rician >= 1.8f || los_ratio >= 0.18f) {
        current_status_ = LinkStabilityStatus::CleanLoS;
        frame.link_status_text = "clean_los";
    } else {
        // Kênh truyền phản xạ tĩnh nhưng ổn định
        current_status_ = LinkStabilityStatus::MultipathStable;
        frame.link_status_text = "multipath_stable";
    }

    // 6. Tính toán điểm ổn định & chất lượng tổng hợp (Quality & Stability Score 0 - 100)
    // 6.1. Thành phần công suất thu RSSI (75% trọng số cơ sở theo đồ thị đồng hồ Wi-Fi chuẩn)
    const float score_rssi = computeRssiQualityScore(effective_rssi);

    // 6.2. Thành phần độ ổn định kênh truyền CSI (25% trọng số):
    // - Tương quan phổ r (chuẩn hóa từ 0.60 đến 0.95): kênh không bị méo/biến dạng
    // - Độ tĩnh thời gian (phương sai biên độ chuẩn hóa từ 0.00 đến 0.15)
    const float score_corr =
        clampVal((spectral_corr - 0.60f) / 0.35f, 0.0f, 1.0f) * 100.0f;
    const float score_stillness =
        (1.0f - clampVal(temporal_variance / 0.15f, 0.0f, 1.0f)) * 100.0f;
    const float csi_stability = 0.60f * score_corr + 0.40f * score_stillness;

    // Điểm tổng hợp cơ sở: 75% RSSI + 25% CSI Stability
    float raw_score = 0.75f * score_rssi + 0.25f * csi_stability;

    // Trừ điểm nhẹ nếu phát hiện vết lõm suy hao chọn lọc tần số (Deep Fade)
    if (frame.deep_fade_warning) {
        const float penalty =
            clampVal((fade_ratio - 4.5f) / 5.0f, 0.0f, 1.0f) * 6.0f;
        raw_score -= penalty;
    }

    // Cam kết ngưỡng sàn tuyệt đối (Floor Guarantees) theo chuẩn đồng hồ đo:
    // - Nếu RSSI >= -50 dBm (ở gần router 0.2m - 1.5m), điểm chất lượng BẮT BUỘC >= 85% (Vùng Xanh Excellent/Max)
    // - Nếu RSSI >= -60 dBm, điểm chất lượng BẮT BUỘC >= 70% (Vùng Vàng/Xanh Good)
    // - Nếu RSSI >= -67 dBm, điểm chất lượng BẮT BUỘC >= 50% (Vùng Đạt chuẩn tối thiểu Reliable)
    if (effective_rssi >= -50.0f) {
        raw_score = std::max(raw_score, 85.0f);
    } else if (effective_rssi >= -60.0f) {
        raw_score = std::max(raw_score, 70.0f);
    } else if (effective_rssi >= -67.0f) {
        raw_score = std::max(raw_score, 50.0f);
    }

    raw_score = clampVal(raw_score, 5.0f, 100.0f);

    // Làm mượt bằng Exponential Moving Average (EMA) kết hợp Deadband chống rung giật
    if (!score_initialized_) {
        smoothed_score_ = raw_score;
        score_initialized_ = true;
    } else {
        const float diff = std::abs(raw_score - smoothed_score_);
        if (diff >= 0.15f) {
            const float alpha = is_agc_event ? 0.08f : 0.22f;
            smoothed_score_ = (1.0f - alpha) * smoothed_score_ + alpha * raw_score;
        }
    }

    frame.link_stability_score = std::round(smoothed_score_ * 10.0f) / 10.0f;

    // 7. Cập nhật khung lịch sử
    prev_amplitudes_ = amps;
    prev_rssi_dbm_ = effective_rssi;
    prev_agc_[0] = frame.agc_indices[0];
    prev_agc_[1] = frame.agc_indices[1];
    prev_agc_[2] = frame.agc_indices[2];
    has_prev_frame_ = true;
}
