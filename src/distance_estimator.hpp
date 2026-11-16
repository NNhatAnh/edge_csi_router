#ifndef DISTANCE_ESTIMATOR_HPP
#define DISTANCE_ESTIMATOR_HPP

#include "csi_parser.hpp"
#include <complex>
#include <map>
#include <mutex>
#include <string>
#include <vector>

enum class WifiBand {
    Band24GHz, // 2.4 GHz (2400 - 2484 MHz)
    Band5GHz,  // 5.2 / 5.8 GHz (5150 - 5885 MHz)
    Band6GHz   // 6 GHz (5925 - 7125 MHz - Wi-Fi 6E / 7 / 8)
};

enum class DistanceEnvMode {
    LoS,      // Truyền thẳng, phòng thoáng, ít vật cản (n ~ 1.75)
    Standard, // Phòng tiêu chuẩn, có bàn ghế đồ đạc (n ~ 2.15)
    NLoS      // Nhiều vật cản, góc khuất, vách ngăn (n ~ 3.00)
};

struct BandDefaultParams {
    float a_1m;           // Mức RSSI thực nghiệm tại 1m (dBm)
    float path_loss_n;    // Hệ số suy hao môi trường trong nhà
    float default_freq;   // Tần số mặc định (MHz)
    std::string band_name;// "2.4 GHz", "5.2 GHz", "6 GHz"
};

struct DeviceCalibration {
    std::string mac;
    std::string band;
    float a_1m = -40.0f;
    float n = 1.75f;
    float calibrated_at_dist_m = 1.0f;
    std::string env_mode = "los";
    int64_t timestamp_sec = 0;
};

class DistanceEstimator {
  public:
    DistanceEstimator();

    void process(CSIData &frame, int fallback_rssi_dbm = 0);
    void reset();

    // Cấu hình băng tần và tần số thực
    void setBand(WifiBand band);
    void setFrequencyMHz(float freq_mhz);
    void setChannelBandwidth(int bw_code); // 0=20MHz, 1=40MHz, 2=80MHz, 3=160MHz, 4=320MHz
    void setTxPowerDbm(float tx_power_dbm);
    void setPathLossExponent(float n);

    // Cấu hình môi trường (3 Tab Môi Trường: LoS / Standard / NLoS)
    void setEnvMode(DistanceEnvMode mode, float custom_n = 0.0f);
    void setEnvModeString(const std::string &env, float custom_n = 0.0f);
    DistanceEnvMode getEnvMode() const { return env_mode_; }
    std::string getEnvModeString() const;
    static float getDefaultNForEnv(DistanceEnvMode mode, WifiBand band);

    WifiBand getBand() const { return band_; }
    std::string getBandName() const;
    float getFrequencyMHz() const { return freq_mhz_; }
    float getReferencePathLoss1m() const;
    float getReferenceRssi1m() const;
    float getEffectiveA1m() const;
    float getTxPowerDbm() const { return tx_power_dbm_; }
    float getPathLossExponent() const { return path_loss_n_; }
    float getErrorMarginM() const { return last_error_margin_m_; }

    // Quản lý thiết bị mục tiêu & profile
    void setTargetDevice(const std::string &mac, const std::string &hostname);
    void setClientProfile(const std::string &hostname, int packet_bw = -1);
    std::string getActiveProfileName() const { return active_profile_name_; }
    std::string getCalibrationSource() const { return is_custom_calibrated_ ? "custom" : "default"; }

    // Hiệu chuẩn 1 chạm (One-click Calibration kèm n tùy chọn)
    bool calibrate(const std::string &mac, float actual_distance_m, float custom_n = 0.0f);
    bool resetCalibration(const std::string &mac = "");
    std::vector<DeviceCalibration> getAllCalibrations() const;

    static WifiBand classifyBandFromFreq(float freq_mhz);

  private:
    float computeRicianK(const std::vector<float> &amplitudes);
    float computeLoSRatio(const CSIChain &chain);
    void computeIFFT256(const std::vector<int16_t> &i_data,
                        const std::vector<int16_t> &q_data,
                        std::vector<float> &pdp_power);

    void applyBandDefaults();
    void loadCalibrationsFromFile();
    void saveCalibrationsToFile() const;
    std::string makeCalKey(const std::string &mac, WifiBand band) const;

    mutable std::mutex cal_mutex_;
    std::map<std::string, DeviceCalibration> calibrations_;
    std::string cal_file_path_ = "/root/distance_cal.json";

    // Trạng thái bộ lọc Kalman 1D kép (Khoảng cách & Vận tốc)
    float x_dist_ = 1.0f;
    float x_vel_ = 0.0f;
    float p_cov_[2][2]{};
    uint64_t last_ts_us_ = 0;
    bool initialized_ = false;

    // Tham số băng tần và tần số
    WifiBand band_ = WifiBand::Band5GHz;
    DistanceEnvMode env_mode_ = DistanceEnvMode::LoS;
    float freq_mhz_ = 5210.0f;
    int capture_bw_code_ = 2; // Mặc định 80MHz cho 5GHz
    float path_loss_n_ = 1.75f;
    float tx_power_dbm_ = 18.0f;
    float last_error_margin_m_ = 0.15f;

    // Bộ lọc làm mượt EMA cho RSSI
    float smoothed_rssi_dbm_ = -44.0f;
    bool has_smoothed_rssi_ = false;

    // Thông tin thiết bị mục tiêu & Profile
    std::string target_mac_;
    std::string target_hostname_;
    std::string active_profile_name_ = "Standard";
    float dynamic_a_1m_ = -40.0f;
    bool is_custom_calibrated_ = false;
};

#endif // DISTANCE_ESTIMATOR_HPP
