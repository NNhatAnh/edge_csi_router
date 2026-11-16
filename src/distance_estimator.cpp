#include "distance_estimator.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <complex>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

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
        const float angle = 2.0f * kPi / static_cast<float>(len);
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

    const float inv_n = 1.0f / static_cast<float>(n);
    for (size_t i = 0; i < n; ++i) {
        data[i] *= inv_n;
    }
}

// Bảng tham số mặc định chuẩn hóa theo vật lý sóng (FSPL) và đo kiểm thực nghiệm
static const BandDefaultParams kBandDefaults[] = {
    // Band24GHz: 2.4 GHz (2412 MHz, FSPL 1m = 40.2 dB) - Ít suy hao qua vật cản, bước sóng 12.5cm
    {-34.0f, 1.95f, 2412.0f, "2.4 GHz"},
    // Band5GHz: 5.2 GHz (5210 MHz, FSPL 1m = 46.8 dB) - Khớp thực tế RSSI -44 dBm ở 1.5m
    {-40.0f, 2.15f, 5210.0f, "5.2 GHz"},
    // Band6GHz: 6 GHz (6130 MHz, FSPL 1m = 48.7 dB) - Suy hao FSPL cao hơn 5GHz +1.9 dB, bước sóng 4.6cm
    {-42.0f, 2.25f, 6130.0f, "6 GHz"}
};

std::string normalizeMacUpper(std::string mac) {
    for (char &c : mac) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return mac;
}
} // namespace

DistanceEstimator::DistanceEstimator() {
    loadCalibrationsFromFile();
    applyBandDefaults();
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

WifiBand DistanceEstimator::classifyBandFromFreq(float freq_mhz) {
    if (freq_mhz < 3000.0f) {
        return WifiBand::Band24GHz;
    } else if (freq_mhz >= 5925.0f) {
        return WifiBand::Band6GHz;
    }
    return WifiBand::Band5GHz;
}

std::string DistanceEstimator::getBandName() const {
    size_t idx = static_cast<size_t>(band_);
    if (idx < 3) {
        return kBandDefaults[idx].band_name;
    }
    return "5.2 GHz";
}

float DistanceEstimator::getReferenceRssi1m() const {
    size_t idx = static_cast<size_t>(band_);
    if (idx < 3) {
        return kBandDefaults[idx].a_1m;
    }
    return -40.0f;
}

float DistanceEstimator::getEffectiveA1m() const {
    return dynamic_a_1m_;
}

float DistanceEstimator::getReferencePathLoss1m() const {
    // FSPL tại 1 mét: PL_1m = 20*log10(d_m) + 20*log10(f_MHz) - 27.55 dB
    return 20.0f * std::log10(freq_mhz_) - 27.55f;
}

std::string DistanceEstimator::makeCalKey(const std::string &mac, WifiBand band) const {
    std::string bstr = "5g";
    if (band == WifiBand::Band24GHz) bstr = "24g";
    else if (band == WifiBand::Band6GHz) bstr = "6g";
    return normalizeMacUpper(mac) + "_" + bstr;
}

void DistanceEstimator::applyBandDefaults() {
    size_t idx = static_cast<size_t>(band_);
    if (idx >= 3) idx = 1;

    const auto &def = kBandDefaults[idx];
    float base_a_1m = def.a_1m;
    float base_n = def.path_loss_n;

    // Kiểm tra xem thiết bị này đã được người dùng hiệu chuẩn cho băng tần này chưa
    {
        std::lock_guard<std::mutex> lock(cal_mutex_);
        if (!target_mac_.empty()) {
            std::string key = makeCalKey(target_mac_, band_);
            auto it = calibrations_.find(key);
            if (it != calibrations_.end()) {
                dynamic_a_1m_ = it->second.a_1m;
                path_loss_n_ = it->second.n;
                is_custom_calibrated_ = true;
                active_profile_name_ = "Calibrated";
                return;
            }
        }
    }

    // Chưa có hiệu chuẩn thủ công: áp dụng profile thông minh dựa theo thiết bị
    is_custom_calibrated_ = false;
    std::string lower_host = target_hostname_;
    for (char &c : lower_host) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lower_host.find("iphone") != std::string::npos ||
        lower_host.find("ipad") != std::string::npos ||
        lower_host.find("apple") != std::string::npos) {
        base_a_1m += 2.0f; // Apple thường có EIRP cao hơn ~2dB
        active_profile_name_ = "Apple";
    } else if (lower_host.find("galaxy") != std::string::npos ||
               lower_host.find("samsung") != std::string::npos ||
               lower_host.find("s23") != std::string::npos ||
               lower_host.find("s24") != std::string::npos) {
        base_a_1m += 1.0f; // Samsung flagship
        active_profile_name_ = "Samsung";
    } else {
        active_profile_name_ = "Standard";
    }

    dynamic_a_1m_ = base_a_1m;
    path_loss_n_ = base_n;
}

void DistanceEstimator::setBand(WifiBand band) {
    if (band_ != band) {
        band_ = band;
        size_t idx = static_cast<size_t>(band_);
        if (idx < 3) {
            freq_mhz_ = kBandDefaults[idx].default_freq;
        }
        applyBandDefaults();
        reset();
    }
}

void DistanceEstimator::setFrequencyMHz(float freq_mhz) {
    if (freq_mhz > 1000.0f && freq_mhz < 7500.0f) {
        freq_mhz_ = freq_mhz;
        WifiBand new_band = classifyBandFromFreq(freq_mhz);
        if (new_band != band_) {
            band_ = new_band;
            applyBandDefaults();
            reset();
        }
    }
}

void DistanceEstimator::setChannelBandwidth(int bw_code) {
    if (bw_code >= 0 && bw_code <= 4) {
        capture_bw_code_ = bw_code;
    }
}

void DistanceEstimator::setTxPowerDbm(float tx_power_dbm) {
    if (tx_power_dbm >= -10.0f && tx_power_dbm <= 30.0f) {
        tx_power_dbm_ = tx_power_dbm;
    }
}

void DistanceEstimator::setPathLossExponent(float n) {
    if (n >= 1.2f && n <= 4.5f) {
        path_loss_n_ = n;
    }
}

void DistanceEstimator::setTargetDevice(const std::string &mac, const std::string &hostname) {
    bool changed = (target_mac_ != mac || target_hostname_ != hostname);
    target_mac_ = mac;
    target_hostname_ = hostname;
    if (changed) {
        applyBandDefaults();
        reset();
    }
}

void DistanceEstimator::setClientProfile(const std::string &hostname, int /*packet_bw*/) {
    target_hostname_ = hostname;
    applyBandDefaults();
}

bool DistanceEstimator::calibrate(const std::string &mac, float actual_distance_m) {
    if (actual_distance_m < 0.2f || actual_distance_m > 25.0f) {
        return false;
    }

    std::string use_mac = mac.empty() ? target_mac_ : mac;
    if (use_mac.empty() || use_mac == "auto") {
        use_mac = "DEFAULT";
    }

    // Tính A_cal từ RSSI đã làm mượt: A_cal = RSSI + 10 * n * log10(d_ref)
    float rssi_ref = has_smoothed_rssi_ ? smoothed_rssi_dbm_ : -44.0f;
    float cal_a_1m = rssi_ref + 10.0f * path_loss_n_ * std::log10(actual_distance_m);

    DeviceCalibration cal;
    cal.mac = normalizeMacUpper(use_mac);
    cal.band = getBandName();
    cal.a_1m = std::round(cal_a_1m * 10.0f) / 10.0f;
    cal.n = path_loss_n_;
    cal.calibrated_at_dist_m = actual_distance_m;
    cal.timestamp_sec = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    {
        std::lock_guard<std::mutex> lock(cal_mutex_);
        calibrations_[makeCalKey(use_mac, band_)] = cal;
    }

    saveCalibrationsToFile();

    dynamic_a_1m_ = cal.a_1m;
    is_custom_calibrated_ = true;
    active_profile_name_ = "Calibrated";

    // Khởi tạo lại Kalman để bắt ngay vào khoảng cách thực tế vừa hiệu chuẩn
    x_dist_ = actual_distance_m;
    x_vel_ = 0.0f;
    p_cov_[0][0] = 0.05f; // Thu hẹp phương sai ban đầu
    p_cov_[1][1] = 0.05f;

    std::cout << "[Distance] Calibrated " << use_mac << " on " << cal.band
              << " at " << actual_distance_m << "m: A_1m = " << cal.a_1m << " dBm\n";
    return true;
}

bool DistanceEstimator::resetCalibration(const std::string &mac) {
    std::string use_mac = mac.empty() ? target_mac_ : mac;
    {
        std::lock_guard<std::mutex> lock(cal_mutex_);
        if (use_mac.empty()) {
            calibrations_.clear();
        } else {
            calibrations_.erase(makeCalKey(use_mac, WifiBand::Band24GHz));
            calibrations_.erase(makeCalKey(use_mac, WifiBand::Band5GHz));
            calibrations_.erase(makeCalKey(use_mac, WifiBand::Band6GHz));
        }
    }
    saveCalibrationsToFile();
    applyBandDefaults();
    reset();
    return true;
}

std::vector<DeviceCalibration> DistanceEstimator::getAllCalibrations() const {
    std::lock_guard<std::mutex> lock(cal_mutex_);
    std::vector<DeviceCalibration> result;
    for (const auto &pair : calibrations_) {
        result.push_back(pair.second);
    }
    return result;
}

void DistanceEstimator::loadCalibrationsFromFile() {
    std::lock_guard<std::mutex> lock(cal_mutex_);
    calibrations_.clear();

    std::ifstream f(cal_file_path_);
    if (!f.is_open()) {
        f.open("/tmp/distance_cal.json");
    }
    if (!f.is_open()) return;

    std::string line;
    std::string cur_mac, cur_band;
    float cur_a = -40.0f, cur_n = 2.15f, cur_d = 1.0f;
    int64_t cur_ts = 0;

    // Parser JSON tối giản không phụ thuộc thư viện ngoài
    while (std::getline(f, line)) {
        size_t mac_pos = line.find("\"mac\": \"");
        if (mac_pos != std::string::npos) {
            size_t end = line.find('\"', mac_pos + 8);
            if (end != std::string::npos) cur_mac = line.substr(mac_pos + 8, end - (mac_pos + 8));
        }
        size_t band_pos = line.find("\"band\": \"");
        if (band_pos != std::string::npos) {
            size_t end = line.find('\"', band_pos + 9);
            if (end != std::string::npos) cur_band = line.substr(band_pos + 9, end - (band_pos + 9));
        }
        size_t a_pos = line.find("\"a_1m\": ");
        if (a_pos != std::string::npos) {
            try { cur_a = std::stof(line.substr(a_pos + 8)); } catch (...) {}
        }
        size_t n_pos = line.find("\"n\": ");
        if (n_pos != std::string::npos) {
            try { cur_n = std::stof(line.substr(n_pos + 5)); } catch (...) {}
        }
        size_t d_pos = line.find("\"dist\": ");
        if (d_pos != std::string::npos) {
            try { cur_d = std::stof(line.substr(d_pos + 8)); } catch (...) {}
        }
        size_t ts_pos = line.find("\"ts\": ");
        if (ts_pos != std::string::npos) {
            try { cur_ts = std::stoll(line.substr(ts_pos + 6)); } catch (...) {}
        }

        if (line.find('}') != std::string::npos && !cur_mac.empty()) {
            DeviceCalibration c;
            c.mac = normalizeMacUpper(cur_mac);
            c.band = cur_band.empty() ? "5.2 GHz" : cur_band;
            c.a_1m = cur_a;
            c.n = cur_n;
            c.calibrated_at_dist_m = cur_d;
            c.timestamp_sec = cur_ts;

            WifiBand b = WifiBand::Band5GHz;
            if (c.band.find("2.4") != std::string::npos) b = WifiBand::Band24GHz;
            else if (c.band.find("6") != std::string::npos) b = WifiBand::Band6GHz;

            calibrations_[makeCalKey(c.mac, b)] = c;
            cur_mac.clear();
        }
    }
}

void DistanceEstimator::saveCalibrationsToFile() const {
    std::string path = cal_file_path_;
    std::ofstream f(path);
    if (!f.is_open()) {
        path = "/tmp/distance_cal.json";
        f.open(path);
    }
    if (!f.is_open()) return;

    f << "{\n  \"calibrations\": [\n";
    bool first = true;
    for (const auto &pair : calibrations_) {
        if (!first) f << ",\n";
        first = false;
        const auto &c = pair.second;
        f << "    {\n"
          << "      \"mac\": \"" << c.mac << "\",\n"
          << "      \"band\": \"" << c.band << "\",\n"
          << "      \"a_1m\": " << c.a_1m << ",\n"
          << "      \"n\": " << c.n << ",\n"
          << "      \"dist\": " << c.calibrated_at_dist_m << ",\n"
          << "      \"ts\": " << c.timestamp_sec << "\n"
          << "    }";
    }
    f << "\n  ]\n}\n";
}

float DistanceEstimator::computeRicianK(const std::vector<float> &amplitudes) {
    if (amplitudes.size() < 8) return 1.0f;
    float sum_pwr = 0.0f;
    float sum_pwr_sq = 0.0f;
    for (float a : amplitudes) {
        float p = a * a;
        sum_pwr += p;
        sum_pwr_sq += p * p;
    }
    float n = static_cast<float>(amplitudes.size());
    float mean_pwr = sum_pwr / n;
    float var_pwr = (sum_pwr_sq / n) - (mean_pwr * mean_pwr);
    if (var_pwr <= 1e-4f || mean_pwr <= 1e-4f) return 4.0f;

    float ratio = 2.0f * (mean_pwr * mean_pwr) / var_pwr - 1.0f;
    if (ratio <= 0.0f) return 0.2f;
    float k = (std::sqrt(ratio) - 1.0f) / 2.0f;
    return std::clamp(k, 0.2f, 5.0f);
}

float DistanceEstimator::computeLoSRatio(const CSIChain &chain) {
    if (chain.amplitudes.size() < 16) return 0.5f;
    std::vector<float> pdp;
    computeIFFT256(chain.i, chain.q, pdp);
    if (pdp.size() < 32) return 0.5f;

    float first_peak = 0.0f;
    for (size_t i = 0; i < std::min<size_t>(8, pdp.size()); ++i) {
        if (pdp[i] > first_peak) first_peak = pdp[i];
    }
    float total_energy = 0.0f;
    for (float p : pdp) total_energy += p;

    if (total_energy <= 1e-6f) return 0.5f;
    return std::clamp(first_peak / total_energy, 0.1f, 1.0f);
}

void DistanceEstimator::computeIFFT256(const std::vector<int16_t> &i_data,
                                      const std::vector<int16_t> &q_data,
                                      std::vector<float> &pdp_power) {
    pdp_power.assign(kIFFTSize, 0.0f);
    if (i_data.empty() || i_data.size() != q_data.size()) return;

    std::vector<std::complex<float>> freq_domain(kIFFTSize, std::complex<float>(0.0f, 0.0f));
    const size_t tones = std::min(i_data.size(), kIFFTSize);
    for (size_t k = 0; k < tones; ++k) {
        freq_domain[k] = std::complex<float>(static_cast<float>(i_data[k]), static_cast<float>(q_data[k]));
    }

    radix2IFFT(freq_domain);
    for (size_t t = 0; t < kIFFTSize; ++t) {
        pdp_power[t] = std::norm(freq_domain[t]);
    }
}

void DistanceEstimator::process(CSIData &frame, int fallback_rssi_dbm) {
    if (frame.chains.empty()) return;

    const CSIChain &primary_chain = frame.chains[0];
    const float k_rician = computeRicianK(primary_chain.amplitudes);
    const float eta_los = computeLoSRatio(primary_chain);
    frame.csi_rician_k = k_rician;
    frame.csi_los_ratio = eta_los;

    // 1. Trích xuất mức RSSI phần cứng chuẩn (từ ath12k CR7 metadata hoặc CSI amplitude)
    float raw_rssi_dbm = -50.0f;
    if (frame.rssi_comb_db < 0) {
        // frame.rssi_comb_db đã là giá trị âm chuẩn tính theo dBm từ ath12k (ví dụ: -44 dBm ở 1.5m)
        raw_rssi_dbm = static_cast<float>(frame.rssi_comb_db);
    } else if (frame.rssi_comb_db > 0) {
        // Nếu driver trả về raw SNR dương (dB so với sàn nhiễu -95 dBm)
        raw_rssi_dbm = static_cast<float>(frame.rssi_comb_db) - 95.0f;
    } else if (fallback_rssi_dbm != 0) {
        raw_rssi_dbm = static_cast<float>(fallback_rssi_dbm);
    } else {
        float sum_sq = 0.0f;
        for (float a : primary_chain.amplitudes) sum_sq += a * a;
        const float mean_sq = sum_sq / std::max(1.0f, static_cast<float>(primary_chain.amplitudes.size()));
        const float csi_pwr = 10.0f * std::log10(mean_sq + 1.0f);
        const float gain_ratio = static_cast<float>(frame.amplitude_gain_ratio[0]);
        const float gain_comp = 20.0f * std::log10(128.0f / std::max(1.0f, gain_ratio));
        const float agc_comp = 0.85f * static_cast<float>(frame.agc_indices[0]);
        raw_rssi_dbm = std::clamp(-95.0f + csi_pwr + gain_comp - agc_comp, -85.0f, -15.0f);
    }

    // 2. Lọc làm mượt EMA với vùng chết (Deadband thích ứng) trên miền dB
    if (!has_smoothed_rssi_) {
        smoothed_rssi_dbm_ = raw_rssi_dbm;
        has_smoothed_rssi_ = true;
    } else {
        const float delta_rssi = raw_rssi_dbm - smoothed_rssi_dbm_;
        // Xác định đối tượng đang tĩnh hay đang chuyển động từ motion detector
        const bool is_stationary = frame.motion_ready && (!frame.presence_detected || frame.motion_score < frame.motion_threshold_low);
        const float deadband = is_stationary ? 1.5f : 0.6f;

        if (std::abs(delta_rssi) < deadband) {
            // Dao động nhỏ dưới ngưỡng nhiễu đa đường: chỉ làm mượt cực chậm để số đứng im
            smoothed_rssi_dbm_ += 0.02f * delta_rssi;
        } else {
            // Thay đổi lớn thực sự: bám đuổi nhanh hơn
            smoothed_rssi_dbm_ += 0.20f * delta_rssi;
        }
    }

    const float rssi_dbm = smoothed_rssi_dbm_;
    frame.smoothed_rssi_db = std::round(rssi_dbm * 10.0f) / 10.0f;

    // 3. Bù suy hao do vật cản NLoS dựa trên LoS ratio từ CSI
    const float nlos_comp_db = (1.0f - std::clamp(eta_los, 0.20f, 1.0f)) * 0.8f;
    const float rssi_effective = rssi_dbm + nlos_comp_db;

    // 4. Mô hình suy hao Log-distance Path Loss chuẩn xác theo băng tần
    // d = 10 ^ ((A_1m - RSSI_effective) / (10 * n))
    const float a_1m = dynamic_a_1m_;
    const float exponent = (a_1m - rssi_effective) / (10.0f * path_loss_n_);
    const float raw_distance = std::pow(10.0f, exponent);

    const float safe_raw = std::isfinite(raw_distance) ? raw_distance : 1.0f;
    const float clamped_raw_dist = std::clamp(safe_raw, 0.15f, 15.0f);

    // 5. Bộ lọc Kalman 1D kép thích ứng chuyển động (Motion-Aware Kalman)
    const bool is_stationary = frame.motion_ready && (!frame.presence_detected || frame.motion_score < frame.motion_threshold_low);

    if (!initialized_) {
        x_dist_ = clamped_raw_dist;
        x_vel_ = 0.0f;
        last_ts_us_ = frame.chip_tsf_us;
        initialized_ = true;
    } else {
        float dt = 0.05f;
        if (frame.chip_tsf_us > last_ts_us_ && last_ts_us_ > 0) {
            dt = static_cast<float>(frame.chip_tsf_us - last_ts_us_) * 1e-6f;
            dt = std::clamp(dt, 0.01f, 0.5f);
        }
        last_ts_us_ = frame.chip_tsf_us;

        // Dự đoán (Predict step)
        if (is_stationary) {
            // Khi phòng tĩnh: triệt tiêu vận tốc ảo để không bị trôi/vọt lố quán tính
            x_vel_ = 0.0f;
            p_cov_[0][0] += 0.001f;
        } else {
            x_dist_ += x_vel_ * dt;
            x_vel_ *= 0.85f;
            p_cov_[0][0] += dt * (p_cov_[0][1] + p_cov_[1][0]) + dt * dt * p_cov_[1][1] + 0.01f;
            p_cov_[0][1] += dt * p_cov_[1][1];
            p_cov_[1][0] += dt * p_cov_[1][1];
            p_cov_[1][1] += 0.03f;
        }

        // Cập nhật (Measurement update step)
        const float csi_quality = std::clamp(k_rician, 0.2f, 4.0f) * std::clamp(eta_los, 0.2f, 1.0f);
        const float r_meas = is_stationary ? (1.0f / csi_quality) : (0.35f / csi_quality);
        const float s = p_cov_[0][0] + r_meas;
        const float k0 = p_cov_[0][0] / s;
        const float k1 = p_cov_[1][0] / s;

        float y = clamped_raw_dist - x_dist_;
        if (is_stationary && std::abs(y) < 0.12f) {
            // Sai số dưới 12cm khi đứng yên coi như nhiễu: không kéo số đo
            y = 0.0f;
        }

        x_dist_ += k0 * y;
        if (!is_stationary) {
            x_vel_ += k1 * y;
        } else {
            x_vel_ = 0.0f;
        }

        const float p00_prior = p_cov_[0][0];
        const float p01_prior = p_cov_[0][1];
        p_cov_[0][0] = (1.0f - k0) * p00_prior;
        p_cov_[0][1] = (1.0f - k0) * p01_prior;
        p_cov_[1][0] -= k1 * p00_prior;
        p_cov_[1][1] -= k1 * p01_prior;

        x_dist_ = std::clamp(x_dist_, 0.15f, 15.0f);
    }

    // 6. Đưa kết quả vào telemetry CSIData
    frame.estimated_distance_m = std::round(x_dist_ * 100.0f) / 100.0f;
    frame.distance_confidence = std::clamp(1.0f - (p_cov_[0][0] / 0.5f), 0.3f, 0.99f);

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
        frame.distance_trend = "approaching";
    } else if (x_vel_ > 0.15f) {
        frame.distance_trend = "departing";
    } else {
        frame.distance_trend = "stationary";
    }

    // Xuất thông tin chẩn đoán đa băng tần
    frame.distance_band = getBandName();
    frame.distance_freq_mhz = freq_mhz_;
    frame.distance_a_1m = dynamic_a_1m_;
    frame.distance_n = path_loss_n_;
    frame.distance_cal_source = getCalibrationSource();
}
