#ifndef DISTANCE_ESTIMATOR_HPP
#define DISTANCE_ESTIMATOR_HPP

#include "csi_parser.hpp"
#include <complex>
#include <vector>

enum class WifiBand {
    Band24GHz, // 2.4 GHz (mặc định f = 2442 MHz)
    Band52GHz  // 5.2 GHz (mặc định f = 5200 MHz, hoặc 5745 MHz kênh 149)
};

class DistanceEstimator {
  public:
    DistanceEstimator();

    void process(CSIData &frame, int fallback_rssi_dbm = 0);
    void reset();

    // Cấu hình băng tần và công thức FSPL
    void setBand(WifiBand band);
    void setFrequencyMHz(float freq_mhz);
    void setTxPowerDbm(float tx_power_dbm);
    void setPathLossExponent(float n);

    WifiBand getBand() const { return band_; }
    float getFrequencyMHz() const { return freq_mhz_; }
    float getReferencePathLoss1m() const;
    float getTxPowerDbm() const { return tx_power_dbm_; }
    float getPathLossExponent() const { return path_loss_n_; }

  private:
    float computeRicianK(const std::vector<float> &amplitudes);
    float computeLoSRatio(const CSIChain &chain);
    void computeIFFT256(const std::vector<int16_t> &i_data,
                        const std::vector<int16_t> &q_data,
                        std::vector<float> &pdp_power);

    // Trạng thái bộ lọc Kalman 1D kép (Khoảng cách & Vận tốc)
    float x_dist_ = 1.0f;     // Khoảng cách ước tính (m)
    float x_vel_ = 0.0f;      // Vận tốc hướng tâm (m/s)
    float p_cov_[2][2]{};     // Hiệp phương sai sai số
    uint64_t last_ts_us_ = 0;
    bool initialized_ = false;

    // Tham số mô hình suy hao FSPL (Free-Space Path Loss)
    WifiBand band_ = WifiBand::Band52GHz;
    float freq_mhz_ = 5200.0f;     // Tần số sóng mang (MHz)
    float tx_power_dbm_ = 6.5f;    // Công suất phát hiệu dụng điện thoại (EIRP dBm: ~6.5 dBm đã bù trừ ăng ten & hấp thụ bàn tay)
    float path_loss_n_ = 2.1f;     // Hệ số suy hao môi trường trong nhà (FSPL LoS n = 2.0 - 2.2)

    // Bộ lọc làm mượt EMA cho RSSI
    float smoothed_rssi_dbm_ = -44.0f;
    bool has_smoothed_rssi_ = false;
};

#endif // DISTANCE_ESTIMATOR_HPP
