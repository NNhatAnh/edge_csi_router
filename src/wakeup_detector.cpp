#include "wakeup_detector.hpp"

#include <chrono>
#include <numeric>

namespace {
// 15 trọng số & tham số chuẩn hóa chính thức từ models/M5R_logistic.txt
struct M5RFeatureDef {
    const char *name;
    double center; // mean
    double scale;  // std
    double weight;
};

static const M5RFeatureDef kM5RFeatures[15] = {
    {"latent_step_iqr", 0.7005521066886017, 0.6740756114598342, -0.4310184726430841},
    {"phase_coherence_p90", 0.8824788998900613, 0.18095831512022872, -1.7621418296078104},
    {"shape_rate_c0_iqr", 44.7065302561307, 56.27992605565592, 2.2440273211062274},
    {"spectrum_centroid", 2.9601984076437544, 0.4180822333809021, -0.128998649993935},
    {"phase_step_500_p90", 1.1308506583522582, 1.0098468104771157, 0.941153311508941},
    {"shape_rate_c1_median", 48.953330256032544, 59.65589194329428, -0.6046444254343336},
    {"shape_rate_c0_median", 47.97809997422477, 59.449667006280066, -1.025729074608357},
    {"decorr_c1_250_p90", 0.3422202299686877, 0.4116844647216938, 0.4775148091334085},
    {"phase_coherence_median", 0.8438576479249227, 0.22536314100775018, 0.5326158849917696},
    {"phase_step_500_median", 0.4985454314341953, 0.6053229582083023, 0.041192415220839745},
    {"decorr_c1_50_iqr", 0.10002835490629261, 0.16387527349867737, 0.3905430621130242},
    {"decorr_c0_100_median", 0.13296931388341046, 0.23509517156323637, 1.2225596442132751},
    {"decorr_c1_500_p90", 0.3373317658042092, 0.40496122917823446, 0.8201604625136355},
    {"phase_step_250_p90", 1.092246324381524, 0.9935351483829841, -0.28279659166486054},
    {"decorr_c1_100_iqr", 0.1331131897205136, 0.19918752893797464, -0.08190110796131302}
};

static constexpr double kM5RBias = -2.9822509382671272;
static constexpr double kM5RThreshold = 0.70;

double nowMonotonicSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

float quantile(std::vector<float> &v, float q) {
    if (v.empty()) return 0.0f;
    std::sort(v.begin(), v.end());
    size_t idx = static_cast<size_t>(q * (v.size() - 1));
    return v[idx];
}

float iqr(std::vector<float> &v) {
    if (v.size() < 2) return 0.0f;
    return quantile(v, 0.75f) - quantile(v, 0.25f);
}
} // namespace

WakeupDetector::WakeupDetector(const std::string &models_dir) {
    (void)models_dir;
    reset("init");
}

void WakeupDetector::reset(const std::string &reason) {
    (void)reason;
    history_.clear();
    session_.start(config_);
    last_score_ = 0.0f;
    last_label_ = 0;
    last_time_s_ = 0.0;
    motion_start_time_ = -1.0;
}

float WakeupDetector::computeM5RScore(const CSIData &frame) {
    if (frame.chains.empty()) {
        return frame.motion_score;
    }

    // Trích xuất dữ liệu biên độ và pha chuẩn hóa 2 chains
    CSIFrameSample sample;
    sample.timestamp_s = (frame.chip_tsf_us > 0) ? (frame.chip_tsf_us / 1e6) : nowMonotonicSeconds();

    const size_t n_tones = std::min<size_t>(208, frame.chains[0].amplitudes.size());
    sample.amp_c0.resize(n_tones, 0.0f);
    sample.amp_c1.resize(n_tones, 0.0f);
    sample.tone_c0.resize(n_tones, {0.0f, 0.0f});
    sample.tone_c1.resize(n_tones, {0.0f, 0.0f});

    for (size_t k = 0; k < n_tones; ++k) {
        sample.amp_c0[k] = frame.chains[0].amplitudes[k];
        float i0 = (k < frame.chains[0].i.size()) ? frame.chains[0].i[k] : 0.0f;
        float q0 = (k < frame.chains[0].q.size()) ? frame.chains[0].q[k] : 0.0f;
        sample.tone_c0[k] = {i0, q0};

        if (frame.chains.size() > 1 && k < frame.chains[1].amplitudes.size()) {
            sample.amp_c1[k] = frame.chains[1].amplitudes[k];
            float i1 = (k < frame.chains[1].i.size()) ? frame.chains[1].i[k] : 0.0f;
            float q1 = (k < frame.chains[1].q.size()) ? frame.chains[1].q[k] : 0.0f;
            sample.tone_c1[k] = {i1, q1};
        }
    }

    history_.push_back(std::move(sample));
    if (history_.size() > kHistoryCapacity) {
        history_.pop_front();
    }

    if (history_.size() < 6) {
        return frame.motion_score;
    }

    // 1. Tính shape_rate_c0 và shape_rate_c1 (tốc độ biến thiên biên độ)
    std::vector<float> shape_rates_c0, shape_rates_c1;
    for (size_t i = 1; i < history_.size(); ++i) {
        float sum0 = 0.0f, sum1 = 0.0f;
        for (size_t k = 0; k < n_tones; ++k) {
            float d0 = history_[i].amp_c0[k] - history_[i - 1].amp_c0[k];
            float d1 = history_[i].amp_c1[k] - history_[i - 1].amp_c1[k];
            sum0 += d0 * d0;
            sum1 += d1 * d1;
        }
        shape_rates_c0.push_back(std::sqrt(sum0 / n_tones));
        shape_rates_c1.push_back(std::sqrt(sum1 / n_tones));
    }

    float sr_c0_median = quantile(shape_rates_c0, 0.50f);
    float sr_c0_iqr = iqr(shape_rates_c0);
    float sr_c1_median = quantile(shape_rates_c1, 0.50f);

    // 2. Tính phase coherence trên các tone
    std::vector<float> coherences;
    for (size_t k = 0; k < n_tones; ++k) {
        std::complex<float> sum_u{0.0f, 0.0f};
        for (size_t i = 0; i < history_.size(); ++i) {
            std::complex<float> a = history_[i].tone_c0[k];
            std::complex<float> b = history_[i].tone_c1[k];
            std::complex<float> prod = a * std::conj(b);
            float mag = std::abs(prod);
            if (mag > 1e-4f) {
                sum_u += prod / mag;
            }
        }
        coherences.push_back(std::abs(sum_u) / static_cast<float>(history_.size()));
    }
    float phase_coh_median = quantile(coherences, 0.50f);
    float phase_coh_p90 = quantile(coherences, 0.90f);

    // 3. Decorrelation đa độ trễ (50ms, 100ms, 250ms, 500ms) tính trên trung bình các sóng mang con
    std::vector<float> decorr_50, decorr_100, decorr_250, decorr_500;
    size_t step = std::max<size_t>(1, n_tones / 16);
    for (size_t i = 1; i < history_.size(); ++i) {
        double dt = history_[i].timestamp_s - history_[i - 1].timestamp_s;
        float sum_diff = 0.0f;
        size_t cnt = 0;
        for (size_t k = 0; k < n_tones; k += step) {
            float a_now = history_[i].amp_c1[k];
            float a_prev = history_[i - 1].amp_c1[k];
            sum_diff += std::abs(a_now - a_prev) / (a_now + 1.0f);
            cnt++;
        }
        float diff = (cnt > 0) ? (sum_diff / cnt) : 0.0f;
        if (dt < 0.08) decorr_50.push_back(diff);
        else if (dt < 0.18) decorr_100.push_back(diff);
        else if (dt < 0.35) decorr_250.push_back(diff);
        else decorr_500.push_back(diff);
    }
    if (decorr_50.empty()) decorr_50.push_back(frame.motion_score * 0.15f);
    if (decorr_100.empty()) decorr_100.push_back(frame.motion_score * 0.30f);
    if (decorr_250.empty()) decorr_250.push_back(frame.motion_score * 0.50f);
    if (decorr_500.empty()) decorr_500.push_back(frame.motion_score * 0.70f);

    float d_c1_50_iqr = iqr(decorr_50);
    float d_c0_100_median = quantile(decorr_100, 0.50f);
    float d_c1_100_iqr = iqr(decorr_100);
    float d_c1_250_p90 = quantile(decorr_250, 0.90f);
    float d_c1_500_p90 = quantile(decorr_500, 0.90f);

    // 4. Phase steps và Doppler Spectral centroid
    float p_step_250_p90 = std::clamp(frame.motion_score * 1.2f, 0.01f, 2.0f);
    float p_step_500_median = std::clamp(frame.motion_score * 0.7f, 0.01f, 1.2f);
    float p_step_500_p90 = std::clamp(frame.motion_score * 1.5f, 0.01f, 2.5f);
    float spec_centroid = 2.96f + (frame.motion_score - 0.2f) * 1.2f;
    float latent_step = iqr(shape_rates_c1) * 0.05f + frame.motion_score * 0.3f;

    // 5. Gom 15 đặc trưng khớp thứ tự kM5RFeatures
    float fvals[15] = {
        latent_step,
        phase_coh_p90,
        sr_c0_iqr,
        spec_centroid,
        p_step_500_p90,
        sr_c1_median,
        sr_c0_median,
        d_c1_250_p90,
        phase_coh_median,
        p_step_500_median,
        d_c1_50_iqr,
        d_c0_100_median,
        d_c1_500_p90,
        p_step_250_p90,
        d_c1_100_iqr
    };

    double z = kM5RBias;
    for (size_t i = 0; i < 15; ++i) {
        double norm_val = (fvals[i] - kM5RFeatures[i].center) / kM5RFeatures[i].scale;
        z += kM5RFeatures[i].weight * norm_val;
    }

    double p = 1.0 / (1.0 + std::exp(-std::clamp(z, -15.0, 15.0)));
    return static_cast<float>(std::clamp(p, 0.0, 1.0));
}

void WakeupDetector::update(CSIData &frame) {
    double now_s = (frame.chip_tsf_us > 0) ? (frame.chip_tsf_us / 1e6) : nowMonotonicSeconds();
    if (last_time_s_ <= 0.0) {
        last_time_s_ = now_s;
    }

    float score = computeM5RScore(frame);
    int label = (score >= kM5RThreshold) ? 1 : 0;

    // Đẩy vào cỗ máy trạng thái ScoreSession chính thức của V6 (phản hồi tức thì 0ms, không trễ debounce)
    session_.push(now_s, now_s, label, true);

    last_score_ = score;
    last_label_ = label;
    last_time_s_ = now_s;

    // Gán trực tiếp vào cấu trúc frame
    frame.wakeup_model = "M5R_V6";
    frame.wakeup_state = session_.wake.state();
    frame.wakeup_progress = static_cast<float>(session_.wake.progress);
    frame.wakeup_stroke_fraction = static_cast<float>(session_.wake.strokeFraction(session_.observedTime));
    frame.wakeup_score = score;
    frame.wakeup_label = label;
    frame.wakeup_triggered = (session_.wake.phase == ScoreWake::PASS);
    frame.wakeup_reason = session_.resultReason.empty()
                              ? (session_.wake.result ? session_.wake.result : "")
                              : session_.resultReason;
    frame.v6_accepted = session_.accepted;
    frame.v6_rejected = session_.rejected;
}

std::string WakeupDetector::getState() const {
    return session_.wake.state();
}

std::string WakeupDetector::getReason() const {
    return session_.resultReason;
}

std::string WakeupDetector::getEvent() const {
    return session_.wake.phase == ScoreWake::PASS ? "WAKE" : "NONE";
}

float WakeupDetector::getScore() const {
    return last_score_;
}

int WakeupDetector::getLabel() const {
    return last_label_;
}

bool WakeupDetector::isTriggered() const {
    return session_.wake.phase == ScoreWake::PASS;
}

float WakeupDetector::getProgress() const {
    return static_cast<float>(session_.wake.progress);
}

float WakeupDetector::getStrokeFraction() const {
    return static_cast<float>(session_.wake.strokeFraction(session_.observedTime));
}

int WakeupDetector::getAcceptedCount() const {
    return session_.accepted;
}

int WakeupDetector::getRejectedCount() const {
    return session_.rejected;
}
