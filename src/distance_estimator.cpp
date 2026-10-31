#include "distance_estimator.hpp"

#include <algorithm>
#include <cmath>
#include <complex>

namespace {
constexpr size_t kIFFTSize = 256;
constexpr float kPi = 3.14159265358979323846f;

// Đảo bit cho IFFT 256 điểm (8 bits)
uint8_t bitReverse8(uint8_t x) {
    x = ((x & 0x55) << 1) | ((x & 0xAA) >> 1);
    x = ((x & 0x33) << 2) | ((x & 0xCC) >> 2);
    x = ((x & 0x0F) << 4) | ((x & 0xF0) >> 4);
    return x;
}

// Cooley-Tukey Radix-2 IFFT thuần C++ tối ưu hiệu năng
void radix2IFFT(std::vector<std::complex<float>> &data) {
    const size_t n = data.size();
    for (size_t i = 0; i < n; ++i) {
        const size_t j = bitReverse8(static_cast<uint8_t>(i));
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        const float angle = 2.0f * kPi / static_cast<float>(len); // Dấu + cho IFFT
        const std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; ++j) {
                const std::complex<float> u = data[i + j];
                const std::complex<float> v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    // Chuẩn hóa 1/N cho IFFT
    const float inv_n = 1.0f / static_cast<float>(n);
    for (size_t i = 0; i < n; ++i) {
        data[i] *= inv_n;
    }
}
} // namespace

DistanceEstimator::DistanceEstimator() {
    reset();
}

void DistanceEstimator::reset() {
    x_dist_ = 1.0f;
    x_vel_ = 0.0f;
    p_cov_[0][0] = 0.5f;
    p_cov_[0][1] = 0.0f;
    p_cov_[1][0] = 0.0f;
    p_cov_[1][1] = 0.2f;
    last_ts_us_ = 0;
    initialized_ = false;
    smoothed_rssi_dbm_ = -44.0f;
    has_smoothed_rssi_ = false;
}

void DistanceEstimator::setBand(WifiBand band) {
    band_ = band;
    if (band == WifiBand::Band24GHz) {
        freq_mhz_ = 2442.0f;
    } else {
        freq_mhz_ = 5200.0f;
    }
}

void DistanceEstimator::setFrequencyMHz(float freq_mhz) {
    if (freq_mhz > 1000.0f && freq_mhz < 7500.0f) {
        freq_mhz_ = freq_mhz;
        if (freq_mhz < 3000.0f) {
            band_ = WifiBand::Band24GHz;
        } else {
            band_ = WifiBand::Band52GHz;
        }
    }
}

void DistanceEstimator::setTxPowerDbm(float tx_power_dbm) {
    if (tx_power_dbm >= -10.0f && tx_power_dbm <= 30.0f) {
        tx_power_dbm_ = tx_power_dbm;
    }
}

void DistanceEstimator::setPathLossExponent(float n) {
    if (n >= 1.5f && n <= 4.5f) {
        path_loss_n_ = n;
    }
}

float DistanceEstimator::getReferencePathLoss1m() const {
    // FSPL tại 1 mét: PL_1m = 20*log10(d_m) + 20*log10(f_MHz) - 27.55 dB
    // Tại d_m = 1m: 20*log10(1) = 0
    return 20.0f * std::log10(freq_mhz_) - 27.55f;
}

float DistanceEstimator::computeRicianK(const std::vector<float> &amplitudes) {
    if (amplitudes.empty())
        return 1.0f;

    float sum = 0.0f;
    for (float a : amplitudes) {
        sum += a;
    }
    const float mean = sum / static_cast<float>(amplitudes.size());

    float sq_diff_sum = 0.0f;
    for (float a : amplitudes) {
        const float diff = a - mean;
        sq_diff_sum += diff * diff;
    }
    const float variance = sq_diff_sum / static_cast<float>(amplitudes.size());

    // K-factor = mean^2 / (2 * variance)
    const float k_factor = (mean * mean) / (2.0f * variance + 1e-5f);
    return std::clamp(k_factor, 0.1f, 20.0f);
}

void DistanceEstimator::computeIFFT256(const std::vector<int16_t> &i_data,
                                      const std::vector<int16_t> &q_data,
                                      std::vector<float> &pdp_power) {
    pdp_power.assign(kIFFTSize, 0.0f);
    if (i_data.empty() || q_data.empty())
        return;

    std::vector<std::complex<float>> freq_bins(kIFFTSize, {0.0f, 0.0f});
    const size_t tones = std::min(i_data.size(), q_data.size());
    const size_t copy_tones = std::min(tones, kIFFTSize);

    // Điền các sóng mang con vào giữa mảng tần số (zero-padding biên)
    const size_t start_idx = (kIFFTSize - copy_tones) / 2;
    for (size_t k = 0; k < copy_tones; ++k) {
        freq_bins[start_idx + k] = {static_cast<float>(i_data[k]),
                                   static_cast<float>(q_data[k])};
    }

    radix2IFFT(freq_bins);

    for (size_t n = 0; n < kIFFTSize; ++n) {
        const float real = freq_bins[n].real();
        const float imag = freq_bins[n].imag();
        pdp_power[n] = real * real + imag * imag;
    }
}

float DistanceEstimator::computeLoSRatio(const CSIChain &chain) {
    std::vector<float> pdp;
    computeIFFT256(chain.i, chain.q, pdp);

    if (pdp.empty())
        return 0.5f;

    const size_t tones = std::min(chain.i.size(), chain.q.size());
    const size_t copy_tones = std::clamp(tones, size_t(1), kIFFTSize);

    float total_power = 0.0f;
    float max_p = 0.0f;
    size_t max_idx = 0;

    for (size_t i = 0; i < pdp.size(); ++i) {
        total_power += pdp[i];
        if (pdp[i] > max_p) {
            max_p = pdp[i];
            max_idx = i;
        }
    }

    if (total_power <= 1e-6f)
        return 0.5f;

    // Tìm đỉnh sớm nhất có năng lượng tối thiểu 40% so với đỉnh cực đại (tia truyền thẳng LoS)
    float los_power = max_p;
    const float threshold = max_p * 0.40f;
    for (size_t i = 0; i < max_idx; ++i) {
        if (pdp[i] >= threshold) {
            los_power = pdp[i];
            break;
        }
    }

    // Chuẩn hóa theo hệ số zero-padding (kIFFTSize / copy_tones) để độc lập với băng thông (20/40/80 MHz)
    const float bandwidth_norm = static_cast<float>(kIFFTSize) / static_cast<float>(copy_tones);
    const float los_ratio = (los_power / total_power) * bandwidth_norm;
    return std::clamp(los_ratio, 0.10f, 0.95f);
}

void DistanceEstimator::process(CSIData &frame, int fallback_rssi_dbm) {
    if (frame.chains.empty())
        return;

    const auto &primary_chain = frame.chains[0];
    const float k_rician = computeRicianK(primary_chain.amplitudes);
    const float eta_los = computeLoSRatio(primary_chain);

    frame.csi_rician_k = k_rician;
    frame.csi_los_ratio = eta_los;

    // 1. Xác định mức công suất thu RSSI chuẩn (dBm)
    float raw_rssi_dbm = -50.0f;
    if (frame.rssi_comb_db < 0) {
        // Giá trị dBm âm chuẩn từ chip vô tuyến (ví dụ: -22 dBm khi để sát, -45 dBm ở 2m)
        raw_rssi_dbm = static_cast<float>(frame.rssi_comb_db);
    } else if (frame.rssi_comb_db > 0) {
        // Nếu frame.rssi_comb_db mang giá trị SNR dương (dB so với sàn nhiễu -95 dBm)
        raw_rssi_dbm = static_cast<float>(frame.rssi_comb_db) - 95.0f;
    } else if (fallback_rssi_dbm != 0) {
        // Dự phòng lấy từ bảng quét station của router prober
        raw_rssi_dbm = static_cast<float>(fallback_rssi_dbm);
    } else {
        // Hoàn toàn không có header RSSI: tính tương đương từ biên độ CSI và AGC
        float sum_sq = 0.0f;
        for (float a : primary_chain.amplitudes) {
            sum_sq += a * a;
        }
        const float mean_sq = sum_sq / std::max(1.0f, static_cast<float>(primary_chain.amplitudes.size()));
        const float csi_pwr = 10.0f * std::log10(mean_sq + 1.0f);
        const float gain_ratio = static_cast<float>(frame.amplitude_gain_ratio[0]);
        const float gain_comp = 20.0f * std::log10(128.0f / std::max(1.0f, gain_ratio));
        const float agc_comp = 0.85f * static_cast<float>(frame.agc_indices[0]);
        raw_rssi_dbm = std::clamp(-95.0f + csi_pwr + gain_comp - agc_comp, -85.0f, -15.0f);
    }

    // Lọc làm mượt EMA (Exponential Moving Average) để triệt tiêu nhiễu dao động tức thời
    if (!has_smoothed_rssi_) {
        smoothed_rssi_dbm_ = raw_rssi_dbm;
        has_smoothed_rssi_ = true;
    } else {
        smoothed_rssi_dbm_ = 0.80f * smoothed_rssi_dbm_ + 0.20f * raw_rssi_dbm;
    }

    const float rssi_dbm = smoothed_rssi_dbm_;
    frame.rssi_comb_db = static_cast<int8_t>(std::round(rssi_dbm));

    // 2. Hiệu chuẩn mức RSSI hiệu dụng từ CSI (bù suy hao vật cản NLoS nếu có)
    const float nlos_comp_db = (1.0f - std::clamp(eta_los, 0.20f, 1.0f)) * 0.8f;
    const float rssi_effective = rssi_dbm + nlos_comp_db;

    // 3. Mô hình suy hao FSPL (Free-Space Path Loss) tính cự ly tức thời theo mét (m)
    // FSPL (dB) = 20*log10(d_m) + 20*log10(f_MHz) - 27.55
    // PL_1m = 20*log10(f_MHz) - 27.55
    // PathLoss = P_tx - RSSI_effective = PL_1m + 10*n*log10(d_m)
    // => d_m = 10 ^ ((PathLoss - PL_1m) / (10 * n))
    const float pl_1m = getReferencePathLoss1m();
    const float path_loss = tx_power_dbm_ - rssi_effective;
    const float exponent = (path_loss - pl_1m) / (10.0f * path_loss_n_);
    const float raw_distance = std::pow(10.0f, exponent);

    // Giới hạn khoảng cách thô thực tế trong phòng: từ 0.15m (để sát router) đến 12.0m
    const float clamped_raw_dist = std::clamp(raw_distance, 0.15f, 12.0f);

    // 4. Bộ lọc Kalman 1D kép (Khoảng cách d & Vận tốc v)
    if (!initialized_) {
        x_dist_ = clamped_raw_dist;
        x_vel_ = 0.0f;
        last_ts_us_ = frame.chip_tsf_us;
        initialized_ = true;
    } else {
        float dt = 0.05f; // Mặc định 50 ms
        if (frame.chip_tsf_us > last_ts_us_ && last_ts_us_ > 0) {
            dt = static_cast<float>(frame.chip_tsf_us - last_ts_us_) * 1e-6f;
            dt = std::clamp(dt, 0.01f, 0.5f);
        }
        last_ts_us_ = frame.chip_tsf_us;

        // Dự đoán (Predict step)
        x_dist_ += x_vel_ * dt;
        x_vel_ *= 0.88f; // Giảm dần vận tốc nếu không có chuyển động

        p_cov_[0][0] += dt * (p_cov_[0][1] + p_cov_[1][0]) + dt * dt * p_cov_[1][1] + 0.02f;
        p_cov_[0][1] += dt * p_cov_[1][1];
        p_cov_[1][0] += dt * p_cov_[1][1];
        p_cov_[1][1] += 0.05f;

        // Cập nhật (Measurement update step)
        // Kết hợp CSI (Rician K & LoS ratio) để xác định phương sai đo R:
        // - Khi LoS mạnh và K-factor cao: tin cậy RSSI cao -> R_meas nhỏ (~0.15 - 0.3)
        // - Khi NLoS, đa đường mạnh: giảm tin cậy đo tức thời -> R_meas lớn (~0.8 - 1.5) để tránh giật
        const float csi_quality = std::clamp(k_rician, 0.2f, 4.0f) * std::clamp(eta_los, 0.2f, 1.0f);
        const float r_meas = 0.35f / csi_quality;
        const float s = p_cov_[0][0] + r_meas;
        const float k0 = p_cov_[0][0] / s;
        const float k1 = p_cov_[1][0] / s;

        const float y = clamped_raw_dist - x_dist_; // Đổi mới (Innovation)
        x_dist_ += k0 * y;
        x_vel_ += k1 * y;

        p_cov_[0][0] *= (1.0f - k0);
        p_cov_[0][1] *= (1.0f - k0);
        p_cov_[1][0] -= k1 * p_cov_[0][0];
        p_cov_[1][1] -= k1 * p_cov_[0][1];

        x_dist_ = std::clamp(x_dist_, 0.15f, 12.0f);
    }

    // Đưa kết quả vào telemetry CSIData
    frame.estimated_distance_m = std::round(x_dist_ * 100.0f) / 100.0f;

    // Phân vùng cự ly (Proximity Zoning)
    if (x_dist_ < 0.6f) {
        frame.distance_zone = "immediate"; // Sát thiết bị (<0.6m)
    } else if (x_dist_ < 1.8f) {
        frame.distance_zone = "near";      // Gần (0.6 - 1.8m)
    } else if (x_dist_ < 4.0f) {
        frame.distance_zone = "mid";       // Trung bình (1.8 - 4.0m)
    } else {
        frame.distance_zone = "far";       // Xa (>4.0m)
    }

    // Nhận biết xu hướng di chuyển (Distance Trend)
    if (x_vel_ < -0.15f) {
        frame.distance_trend = "approaching"; // Đang lại gần
    } else if (x_vel_ > 0.15f) {
        frame.distance_trend = "departing";   // Đang đi xa
    } else {
        frame.distance_trend = "stationary";  // Đứng yên
    }

    // Độ tin cậy ước tính (0.1 - 0.99)
    const float conf = 0.40f + 0.40f * eta_los + 0.20f * std::min(1.0f, k_rician / 4.0f);
    frame.distance_confidence = std::clamp(conf, 0.1f, 0.98f);
}
