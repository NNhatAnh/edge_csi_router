#include "breath.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinBreathHz = 0.10f; // 6 nhịp/phút (giới hạn dưới sinh lý)
constexpr float kMaxBreathHz = 0.55f; // 33 nhịp/phút (giới hạn trên sinh lý)
constexpr float kBreathFrequencyStepHz = 0.01f;
constexpr uint64_t kMinimumWindowNs = 8000000000ULL;  // 8 giây để bắt đầu ước lượng
constexpr uint64_t kMaximumWindowNs = 25000000000ULL; // 25 giây tối đa
constexpr size_t kMinimumSamples = 30;
constexpr float kMaximumGrossMotionScore = 0.40f;
constexpr size_t kEstimateInterval = 5;

// Ngưỡng độ lệch pha vi sai
constexpr float kMinHumanPhaseSpan = 0.018f; // Nhận diện cả thở nông/nhẹ từ xa

float wrapPhase(float phase) {
    while (phase > kPi)
        phase -= 2.0f * kPi;
    while (phase < -kPi)
        phase += 2.0f * kPi;
    return phase;
}
} // namespace

// ============================================================================
// BreathBandpassFilter: Bộ lọc Bandpass thời gian thực chạy từng frame
// ============================================================================
BreathBandpassFilter::BreathBandpassFilter() { reset(); }

void BreathBandpassFilter::reset() {
    prev_x_ = 0.0f;
    y_hp_ = 0.0f;
    y_lp1_ = 0.0f;
    y_lp2_ = 0.0f;
    initialized_ = false;
}

float BreathBandpassFilter::process(float input, float dt_s) {
    if (!initialized_) {
        prev_x_ = input;
        y_hp_ = 0.0f;
        y_lp1_ = 0.0f;
        y_lp2_ = 0.0f;
        initialized_ = true;
        return 0.0f;
    }

    if (dt_s <= 0.0001f || dt_s > 1.0f)
        dt_s = 0.033f;

    // 1. High-pass filter (cắt DC và trôi dạt nhiệt độ < 0.09 Hz)
    const float tau_hp = 1.0f / (2.0f * kPi * 0.09f);
    const float alpha_hp = tau_hp / (tau_hp + dt_s);
    y_hp_ = alpha_hp * (y_hp_ + input - prev_x_);
    prev_x_ = input;

    // 2. Low-pass filter bậc 2 (khử nhiễu jitter tần số cao > 0.55 Hz)
    const float tau_lp = 1.0f / (2.0f * kPi * 0.55f);
    const float alpha_lp = dt_s / (tau_lp + dt_s);
    y_lp1_ += alpha_lp * (y_hp_ - y_lp1_);
    y_lp2_ += alpha_lp * (y_lp1_ - y_lp2_);

    return y_lp2_;
}

// ============================================================================
// BreathDetector
// ============================================================================
BreathDetector::BreathDetector(size_t window_size)
    : window_size_(window_size) {}

void BreathDetector::reset() {
    phase_window_.clear();
    timestamp_window_ns_.clear();
    previous_wrapped_phase_.clear();
    previous_unwrapped_phase_.clear();
    estimate_counter_ = 0;
    real_time_filter_.reset();
    last_frame_time_ns_ = 0;
    sensitive_tones_.clear();

    current_bpm_ = 0.0f;
    current_wave_ = 0.0f;
    current_quality_ = 0.0f;
    current_phase_span_ = 0.0f;
    current_asymmetry_ = 0.0f;
    current_rrv_ = 0.0f;
    current_regularity_ = 0.0f;
    current_coherence_ = 0.0f;
    current_prominence_ = 0.0f;
    current_breath_valid_ = false;
    is_human_presence_ = false;
    is_sleeping_ = false;
    settling_frames_ = 0;
    presence_status_ = "empty_room";
    breath_wave_history_.clear();
    presence_confidence_ = 0.0f;
    sleep_qualify_counter_ = 0;
}

std::vector<float>
BreathDetector::getWrappedPhaseDiff(const CSIData &frame) const {
    if (frame.chains.empty())
        return {};

    // Tìm 2 antenna chain có SNR cao nhất để loại trừ antenna bị suy hao sâu
    size_t rx1 = 0;
    size_t rx2 = (frame.chains.size() >= 2) ? 1 : 0;
    if (frame.chains.size() > 2) {
        std::vector<std::pair<float, size_t>> chain_snr;
        for (size_t c = 0; c < frame.chains.size(); ++c) {
            float sum_a = 0.0f;
            for (float a : frame.chains[c].amplitudes)
                sum_a += a;
            float avg = frame.chains[c].amplitudes.empty()
                            ? 0.0f
                            : (sum_a / static_cast<float>(frame.chains[c].amplitudes.size()));
            chain_snr.push_back({avg, c});
        }
        std::sort(chain_snr.rbegin(), chain_snr.rend());
        rx1 = chain_snr[0].second;
        rx2 = chain_snr[1].second;
    }

    size_t tone_count = frame.tonesPerChain();
    tone_count = std::min(tone_count, frame.chains[rx1].phases.size());
    if (rx1 != rx2)
        tone_count = std::min(tone_count, frame.chains[rx2].phases.size());
    if (tone_count == 0)
        return {};

    std::vector<float> phase(tone_count);
    for (size_t tone = 0; tone < tone_count; ++tone) {
        if (rx1 != rx2) {
            // Hiệu pha giữa 2 antenna triệt tiêu hoàn toàn CFO và SFO
            phase[tone] = wrapPhase(frame.chains[rx1].phases[tone] -
                                    frame.chains[rx2].phases[tone]);
        } else {
            phase[tone] = wrapPhase(frame.chains[rx1].phases[tone]);
        }
    }
    return phase;
}

void BreathDetector::updateSensitiveTones(const CSIData &frame) {
    if (frame.chains.empty() || phase_window_.size() < 20)
        return;

    const size_t tone_count = phase_window_.front().size();
    if (tone_count < 8)
        return;

    // Loại bỏ 10% biên ngoài (guard bands)
    const size_t start_tone = tone_count / 10;
    const size_t end_tone = tone_count - start_tone;
    if (end_tone <= start_tone)
        return;

    // Tính biên độ trung bình để loại subcarrier null
    float mean_amp = 0.0f;
    size_t amp_samples = 0;
    if (!frame.chains[0].amplitudes.empty()) {
        for (size_t t = start_tone; t < end_tone && t < frame.chains[0].amplitudes.size(); ++t) {
            mean_amp += frame.chains[0].amplitudes[t];
            ++amp_samples;
        }
        if (amp_samples > 0)
            mean_amp /= static_cast<float>(amp_samples);
    }

    // Đánh giá phương sai pha trên cửa sổ gần nhất
    const size_t sample_count = phase_window_.size();
    struct ToneScore {
        size_t tone;
        float score;
    };
    std::vector<ToneScore> scores;

    for (size_t t = start_tone; t < end_tone; ++t) {
        if (mean_amp > 0.0f && t < frame.chains[0].amplitudes.size()) {
            if (frame.chains[0].amplitudes[t] < 0.15f * mean_amp)
                continue; // Bỏ subcarrier bị suy hao null
        }

        float sum = 0.0f;
        for (const auto &sample : phase_window_)
            sum += sample[t];
        const float mean = sum / static_cast<float>(sample_count);

        float var = 0.0f;
        for (const auto &sample : phase_window_) {
            const float diff = sample[t] - mean;
            var += diff * diff;
        }
        const float std_dev = std::sqrt(var / static_cast<float>(sample_count));

        // Subcarrier nhạy cảm với nhịp thở có độ lệch pha trong dải [0.015, 2.0] rad
        if (std_dev >= 0.015f && std_dev <= 2.5f) {
            // Điểm số ưu tiên subcarrier có dao động rõ ràng nhưng không bị nhiễu nhảy pha
            const float score = 1.0f / (1.0f + std::abs(std_dev - 0.25f));
            scores.push_back({t, score});
        }
    }

    std::sort(scores.begin(), scores.end(),
              [](const ToneScore &a, const ToneScore &b) { return a.score > b.score; });

    sensitive_tones_.clear();
    const size_t target_count = std::min<size_t>(12, scores.size());
    for (size_t i = 0; i < target_count; ++i) {
        sensitive_tones_.push_back(scores[i].tone);
    }
}

float BreathDetector::synthesizeCompositePhase(
    const std::vector<float> &unwrapped) const {
    if (unwrapped.empty())
        return 0.0f;

    if (!sensitive_tones_.empty()) {
        float sum = 0.0f;
        for (size_t t : sensitive_tones_) {
            if (t < unwrapped.size())
                sum += unwrapped[t];
        }
        return sum / static_cast<float>(sensitive_tones_.size());
    }

    // Dự phòng: trung bình 50% subcarrier ở giữa
    const size_t first = unwrapped.size() / 4;
    const size_t last = (unwrapped.size() * 3) / 4;
    if (last <= first)
        return unwrapped[0];

    float sum = 0.0f;
    for (size_t t = first; t < last; ++t)
        sum += unwrapped[t];
    return sum / static_cast<float>(last - first);
}

float BreathDetector::computeSubcarrierCoherence() const {
    if (sensitive_tones_.size() < 2 || phase_window_.size() < 20)
        return 0.0f;

    const size_t M = sensitive_tones_.size();
    // Lấy tối đa 300 mẫu gần nhất (~10 giây) để đo độ đồng biến thời gian thực
    const size_t N = std::min<size_t>(phase_window_.size(), 300);
    const size_t offset = phase_window_.size() - N;

    std::vector<std::vector<float>> zero_mean(M, std::vector<float>(N));
    std::vector<float> std_dev(M, 0.0f);

    for (size_t m = 0; m < M; ++m) {
        const size_t tone = sensitive_tones_[m];
        float sum = 0.0f;
        for (size_t i = 0; i < N; ++i)
            sum += phase_window_[offset + i][tone];
        const float mean = sum / static_cast<float>(N);

        float var = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            const float diff = phase_window_[offset + i][tone] - mean;
            zero_mean[m][i] = diff;
            var += diff * diff;
        }
        std_dev[m] = std::sqrt(var / static_cast<float>(N));
    }

    float pair_corr_sum = 0.0f;
    size_t pair_count = 0;

    for (size_t i = 0; i < M; ++i) {
        if (std_dev[i] < 1.0e-5f)
            continue;
        for (size_t j = i + 1; j < M; ++j) {
            if (std_dev[j] < 1.0e-5f)
                continue;
            float cov = 0.0f;
            for (size_t k = 0; k < N; ++k)
                cov += zero_mean[i][k] * zero_mean[j][k];

            const float r = std::abs(cov / (static_cast<float>(N) * std_dev[i] * std_dev[j]));
            pair_corr_sum += r;
            ++pair_count;
        }
    }

    return pair_count > 0 ? (pair_corr_sum / static_cast<float>(pair_count)) : 0.0f;
}

float BreathDetector::computeAutocorrelationRegularity(
    const std::vector<double> &wave, float frequency_hz,
    double sample_rate) const {
    if (wave.size() < 20 || frequency_hz <= 0.0f || sample_rate <= 0.0)
        return 0.0f;

    const size_t period_samples = static_cast<size_t>(
        std::round(sample_rate / static_cast<double>(frequency_hz)));
    if (period_samples == 0 || period_samples >= wave.size() - 2)
        return 0.0f;

    // Tìm kiếm trong dải quanh chu kỳ mong đợi [0.85 * T, 1.15 * T]
    const size_t min_lag = std::max<size_t>(2, static_cast<size_t>(period_samples * 0.85));
    const size_t max_lag = std::min<size_t>(wave.size() - 5, static_cast<size_t>(period_samples * 1.15));

    double best_corr = 0.0;
    for (size_t lag = min_lag; lag <= max_lag; ++lag) {
        const size_t overlap = wave.size() - lag;
        if (overlap < 10)
            break;

        double sum_x = 0.0;
        double sum_y = 0.0;
        for (size_t i = 0; i < overlap; ++i) {
            sum_x += wave[i];
            sum_y += wave[i + lag];
        }
        const double mean_x = sum_x / static_cast<double>(overlap);
        const double mean_y = sum_y / static_cast<double>(overlap);

        double covariance = 0.0;
        double energy_x = 0.0;
        double energy_y = 0.0;
        for (size_t i = 0; i < overlap; ++i) {
            const double dx = wave[i] - mean_x;
            const double dy = wave[i + lag] - mean_y;
            covariance += dx * dy;
            energy_x += dx * dx;
            energy_y += dy * dy;
        }

        const double denom = std::sqrt(energy_x * energy_y);
        if (denom > 1.0e-9) {
            const double corr = std::clamp(covariance / denom, 0.0, 1.0);
            best_corr = std::max(best_corr, corr);
        }
    }

    const double observed_cycles =
        static_cast<double>(wave.size()) / static_cast<double>(period_samples);
    const double cycle_coverage = std::clamp(observed_cycles - 1.0, 0.0, 1.0);
    return static_cast<float>(best_corr * cycle_coverage);
}

WaveMorphology BreathDetector::analyzeWaveform() {
    WaveMorphology morph;
    morph.is_human = false;
    morph.is_sleeping = false;
    morph.regularity = 0.0f;
    morph.classification = "empty_room";

    if (breath_wave_history_.size() < 30) {
        morph.classification = "calibrating";
        return morph;
    }

    const uint64_t duration_ns =
        breath_wave_history_.back().first - breath_wave_history_.front().first;
    if (duration_ns < 7000000000ULL) {
        morph.classification = "calibrating";
        return morph;
    }

    const uint64_t start_ns = breath_wave_history_.front().first;
    const size_t n = breath_wave_history_.size();
    std::vector<double> time_s(n);
    std::vector<double> wave(n);

    for (size_t i = 0; i < n; ++i) {
        time_s[i] = static_cast<double>(breath_wave_history_[i].first - start_ns) * 1.0e-9;
        wave[i] = breath_wave_history_[i].second;
    }

    // Làm mượt nhẹ 3 điểm để khử jitter
    std::vector<double> smoothed = wave;
    for (size_t i = 1; i + 1 < n; ++i) {
        smoothed[i] = (wave[i - 1] + 2.0 * wave[i] + wave[i + 1]) * 0.25;
    }

    // Tính Phase Span đỉnh-đáy
    double min_val = smoothed[0];
    double max_val = smoothed[0];
    double sum_val = 0.0;
    for (double v : smoothed) {
        min_val = std::min(min_val, v);
        max_val = std::max(max_val, v);
        sum_val += v;
    }
    const double mean_val = sum_val / static_cast<double>(n);
    morph.phase_span = static_cast<float>(max_val - min_val);

    double variance = 0.0;
    for (double v : smoothed)
        variance += (v - mean_val) * (v - mean_val);
    const double std_val = std::sqrt(variance / static_cast<double>(n));

    // Tìm các đỉnh (Peaks) và đáy (Troughs)
    struct ExtremaPoint {
        double time;
        double value;
        bool is_peak;
    };
    std::vector<ExtremaPoint> extrema;

    const double peak_threshold = mean_val + 0.10 * std_val;
    const double trough_threshold = mean_val - 0.10 * std_val;

    for (size_t i = 2; i + 2 < n; ++i) {
        if (smoothed[i] > smoothed[i - 1] && smoothed[i] > smoothed[i + 1] &&
            smoothed[i] >= smoothed[i - 2] && smoothed[i] >= smoothed[i + 2] &&
            smoothed[i] > peak_threshold) {
            if (extrema.empty() || (!extrema.back().is_peak &&
                                    (time_s[i] - extrema.back().time >= 1.2))) {
                extrema.push_back({time_s[i], smoothed[i], true});
            } else if (extrema.back().is_peak && smoothed[i] > extrema.back().value) {
                extrema.back() = {time_s[i], smoothed[i], true};
            }
        } else if (smoothed[i] < smoothed[i - 1] && smoothed[i] < smoothed[i + 1] &&
                   smoothed[i] <= smoothed[i - 2] && smoothed[i] <= smoothed[i + 2] &&
                   smoothed[i] < trough_threshold) {
            if (extrema.empty() || (extrema.back().is_peak &&
                                    (time_s[i] - extrema.back().time >= 1.2))) {
                extrema.push_back({time_s[i], smoothed[i], false});
            } else if (!extrema.back().is_peak && smoothed[i] < extrema.back().value) {
                extrema.back() = {time_s[i], smoothed[i], false};
            }
        }
    }

    std::vector<double> peak_times;
    for (const auto &ex : extrema) {
        if (ex.is_peak)
            peak_times.push_back(ex.time);
    }
    morph.peak_count = peak_times.size();

    // 1. Phân tích biến thiên chu kỳ sinh lý (RRV)
    double mean_ibi = 0.0;
    if (peak_times.size() >= 2) {
        std::vector<double> ibis;
        for (size_t i = 1; i < peak_times.size(); ++i)
            ibis.push_back(peak_times[i] - peak_times[i - 1]);
        mean_ibi = std::accumulate(ibis.begin(), ibis.end(), 0.0) / ibis.size();
        morph.average_ibi_s = static_cast<float>(mean_ibi);

        double ibi_var = 0.0;
        for (double d : ibis)
            ibi_var += (d - mean_ibi) * (d - mean_ibi);
        const double ibi_std = std::sqrt(ibi_var / ibis.size());
        morph.rrv_cv = mean_ibi > 0.0 ? static_cast<float>(ibi_std / mean_ibi) : 0.0f;
    }

    // 2. Phân tích độ bất đối xứng Hít vào / Thở ra
    std::vector<double> cycle_asymmetries;
    for (size_t i = 1; i + 1 < extrema.size(); ++i) {
        if (extrema[i].is_peak && !extrema[i - 1].is_peak && !extrema[i + 1].is_peak) {
            const double t_rise = extrema[i].time - extrema[i - 1].time;
            const double t_fall = extrema[i + 1].time - extrema[i].time;
            if (t_rise > 0.4 && t_fall > 0.4) {
                const double slope_rise = (extrema[i].value - extrema[i - 1].value) / t_rise;
                const double slope_fall = (extrema[i].value - extrema[i + 1].value) / t_fall;
                const double denom = std::abs(slope_rise) + std::abs(slope_fall);
                if (denom > 1.0e-5) {
                    cycle_asymmetries.push_back(
                        (std::abs(slope_rise) - std::abs(slope_fall)) / denom);
                }
            }
        }
    }

    if (!cycle_asymmetries.empty()) {
        morph.asymmetry_ratio = static_cast<float>(
            std::accumulate(cycle_asymmetries.begin(), cycle_asymmetries.end(), 0.0) /
            cycle_asymmetries.size());
    }

    // 3. Độ lặp lại chu kỳ (Autocorrelation)
    const double total_duration_s = time_s.back() - time_s.front();
    const double estimated_fs =
        total_duration_s > 0.0 ? (static_cast<double>(n - 1) / total_duration_s) : 10.0;
    const float est_freq = morph.average_ibi_s > 0.0f
                               ? (1.0f / morph.average_ibi_s)
                               : (current_bpm_ > 0.0f ? current_bpm_ / 60.0f : 0.25f);
    morph.regularity = computeAutocorrelationRegularity(smoothed, est_freq, estimated_fs);

    return morph;
}

void BreathDetector::updateBreathingEstimate(CSIData &frame) {
    frame.breath_valid = false;
    frame.breath_bpm = current_bpm_;
    frame.breath_quality = current_quality_;
    frame.breath_phase_span = current_phase_span_;
    frame.breath_asymmetry = current_asymmetry_;
    frame.breath_rrv = current_rrv_;
    frame.breath_regularity = current_regularity_;
    frame.is_human_presence = is_human_presence_;
    frame.is_sleeping = is_sleeping_;
    frame.presence_status = presence_status_;

    if (phase_window_.size() < kMinimumSamples ||
        timestamp_window_ns_.back() - timestamp_window_ns_.front() < kMinimumWindowNs) {
        presence_status_ = "calibrating";
        frame.presence_status = "calibrating";
        return;
    }

    const uint64_t start_ns = timestamp_window_ns_.front();
    const size_t sample_count = phase_window_.size();

    // 1. Phân tích phổ hô hấp trên tín hiệu tổng hợp composite (0.10 Hz - 0.55 Hz)
    std::vector<float> composite_wave(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        composite_wave[i] = synthesizeCompositePhase(phase_window_[i]);
    }

    // Khử xu hướng tuyến tính (Linear Detrending)
    double sum_t = 0.0, sum_y = 0.0, sum_ty = 0.0, sum_tt = 0.0;
    for (size_t i = 0; i < sample_count; ++i) {
        const double t = static_cast<double>(timestamp_window_ns_[i] - start_ns) * 1.0e-9;
        const double y = composite_wave[i];
        sum_t += t;
        sum_y += y;
        sum_ty += t * y;
        sum_tt += t * t;
    }
    const double denom_fit = sample_count * sum_tt - sum_t * sum_t;
    const double slope = denom_fit > 1.0e-9 ? (sample_count * sum_ty - sum_t * sum_y) / denom_fit : 0.0;
    const double intercept = (sum_y - slope * sum_t) / static_cast<double>(sample_count);

    std::vector<double> detrended(sample_count);
    double total_residual_energy = 0.0;
    for (size_t i = 0; i < sample_count; ++i) {
        const double t = static_cast<double>(timestamp_window_ns_[i] - start_ns) * 1.0e-9;
        detrended[i] = composite_wave[i] - (intercept + slope * t);
        total_residual_energy += detrended[i] * detrended[i];
    }

    // Quét phổ năng lượng
    double max_spectral_power = 0.0;
    float best_freq = 0.0f;
    double spectral_power_sum = 0.0;
    size_t freq_steps = 0;

    for (float freq = kMinBreathHz; freq <= kMaxBreathHz; freq += kBreathFrequencyStepHz) {
        double cos_sum = 0.0;
        double sin_sum = 0.0;
        for (size_t i = 0; i < sample_count; ++i) {
            const double t = static_cast<double>(timestamp_window_ns_[i] - start_ns) * 1.0e-9;
            const double angle = 2.0 * static_cast<double>(kPi) * freq * t;
            cos_sum += detrended[i] * std::cos(angle);
            sin_sum += detrended[i] * std::sin(angle);
        }
        const double power = (cos_sum * cos_sum + sin_sum * sin_sum) * (2.0 / static_cast<double>(sample_count));
        spectral_power_sum += power;
        ++freq_steps;
        if (power > max_spectral_power) {
            max_spectral_power = power;
            best_freq = freq;
        }
    }

    const double mean_spectral_power = freq_steps > 0 ? (spectral_power_sum / freq_steps) : 0.0;
    // Độ nhọn đỉnh phổ (Spectral Prominence): phòng trống ~ 1.2 - 2.0; có người thở ~ 3.0 - 8.0+
    current_prominence_ = mean_spectral_power > 1.0e-9
                              ? static_cast<float>(max_spectral_power / mean_spectral_power)
                              : 1.0f;
    current_quality_ = total_residual_energy > 1.0e-9
                           ? static_cast<float>(max_spectral_power / (total_residual_energy / sample_count))
                           : 0.0f;

    // 2. Độ đồng pha giữa các subcarrier (Cross-Subcarrier Coherence)
    current_coherence_ = computeSubcarrierCoherence();

    // 3. Phân tích hình thái sóng hơi thở (Morphology, Span, RRV, Regularity)
    WaveMorphology morph = analyzeWaveform();
    current_phase_span_ = morph.phase_span;
    current_asymmetry_ = morph.asymmetry_ratio;
    current_rrv_ = morph.rrv_cv;
    current_regularity_ = morph.regularity;

    // 4. Động cơ tính điểm bằng chứng sinh học đa chiều (Fuzzy Evidence Scoring Engine)
    // - Coherence: có người thở làm di chuyển lồng ngực nên các subcarrier dao động đồng pha (> 0.50).
    //   Phòng trống nhiễu độc lập thì Coherence rất thấp (< 0.20).
    const float score_coherence = std::clamp((current_coherence_ - 0.22f) / 0.35f, 0.0f, 1.0f);
    // - Prominence: đỉnh phổ nhọn tập trung tại 1 tần số nhịp thở (> 2.8).
    const float score_prominence = std::clamp((current_prominence_ - 2.0f) / 2.5f, 0.0f, 1.0f);
    // - Regularity: độ tự tương quan chu kỳ (> 0.30).
    const float score_autocorr = std::clamp((current_regularity_ - 0.22f) / 0.30f, 0.0f, 1.0f);
    // - Span: biên độ dao động pha đủ vượt qua sàn nhiễu nhiệt (> 0.02 rad).
    const float score_span = std::clamp((current_phase_span_ - kMinHumanPhaseSpan) / 0.040f, 0.0f, 1.0f);

    const float evidence_score = 0.35f * score_coherence +
                                 0.30f * score_prominence +
                                 0.20f * score_autocorr +
                                 0.15f * score_span;

    // 5. Cập nhật độ tin cậy bằng bộ tích lũy rò rỉ (Leaky Integrator):
    // - Khi có bằng chứng mạnh (E >= 0.45): Tăng nhanh (Fast attack ~ 2 - 3s).
    // - Khi bằng chứng yếu (E < 0.45): Giảm chậm (Slow decay ~ 6 - 8s grace period, chống chập chờn khi nuốt nước bọt/ngưng thở tạm thời).
    if (evidence_score >= 0.45f && current_phase_span_ >= kMinHumanPhaseSpan) {
        presence_confidence_ = std::min(1.0f, presence_confidence_ + 0.15f);
    } else {
        presence_confidence_ = std::max(0.0f, presence_confidence_ - 0.035f);
    }

    // 6. Quyết định trạng thái hiện diện (Hysteresis chống giật nháy)
    if (presence_confidence_ >= 0.50f) {
        is_human_presence_ = true;
        current_breath_valid_ = true;
        presence_status_ = "human_present";

        // Cập nhật nhịp thở BPM ổn định
        if (best_freq >= kMinBreathHz && best_freq <= kMaxBreathHz) {
            const float est_bpm = best_freq * 60.0f;
            current_bpm_ = (current_bpm_ == 0.0f) ? est_bpm : (0.85f * current_bpm_ + 0.15f * est_bpm);
        }

        // Nhận diện trạng thái ngủ: nhịp thở chậm đều (8 - 18 BPM), RRV thấp, tính chu kỳ cao
        if (current_bpm_ >= 8.0f && current_bpm_ <= 18.0f &&
            current_rrv_ <= 0.20f && current_regularity_ >= 0.40f) {
            if (++sleep_qualify_counter_ >= 4) { // Duy trì ổn định > 10 giây
                is_sleeping_ = true;
            }
        } else {
            sleep_qualify_counter_ = 0;
            is_sleeping_ = false;
        }
    } else if (presence_confidence_ <= 0.18f) {
        is_human_presence_ = false;
        current_breath_valid_ = false;
        is_sleeping_ = false;
        sleep_qualify_counter_ = 0;
        presence_status_ = "empty_room";
    }

    frame.breath_valid = current_breath_valid_;
    frame.breath_bpm = current_bpm_;
    frame.breath_quality = current_quality_;
    frame.breath_phase_span = current_phase_span_;
    frame.breath_asymmetry = current_asymmetry_;
    frame.breath_rrv = current_rrv_;
    frame.breath_regularity = current_regularity_;
    frame.is_human_presence = is_human_presence_;
    frame.is_sleeping = is_sleeping_;
    frame.presence_status = presence_status_;
}

void BreathDetector::update(CSIData &frame) {
    const std::vector<float> wrapped_phase = getWrappedPhaseDiff(frame);
    if (wrapped_phase.empty()) {
        reset();
        return;
    }

    if (wrapped_phase.size() != previous_wrapped_phase_.size()) {
        reset();
        previous_wrapped_phase_ = wrapped_phase;
        previous_unwrapped_phase_ = wrapped_phase;
        return;
    }

    const uint64_t timestamp_ns = frame.timestamp_ns;
    if (!timestamp_window_ns_.empty() &&
        (timestamp_ns <= timestamp_window_ns_.back() ||
         timestamp_ns - timestamp_window_ns_.back() > 6000000000ULL)) {
        reset();
        previous_wrapped_phase_ = wrapped_phase;
        previous_unwrapped_phase_ = wrapped_phase;
        return;
    }

    // Phase unwrapping
    std::vector<float> unwrapped_phase(wrapped_phase.size());
    for (size_t tone = 0; tone < wrapped_phase.size(); ++tone) {
        unwrapped_phase[tone] =
            previous_unwrapped_phase_[tone] +
            wrapPhase(wrapped_phase[tone] - previous_wrapped_phase_[tone]);
    }
    previous_wrapped_phase_ = wrapped_phase;
    previous_unwrapped_phase_ = unwrapped_phase;

    // Tính bước thời gian dt
    float dt_s = 0.033f;
    if (last_frame_time_ns_ > 0 && timestamp_ns > last_frame_time_ns_) {
        dt_s = static_cast<float>(timestamp_ns - last_frame_time_ns_) * 1.0e-9f;
        if (dt_s <= 0.0001f || dt_s > 1.0f)
            dt_s = 0.033f;
    }
    last_frame_time_ns_ = timestamp_ns;

    // Xử lý chuyển động vĩ mô (Gross Motion):
    // Người đang bước đi hoặc cử động mạnh. Người di chuyển chắc chắn là có người,
    // do đó giữ độ tin cậy hiện diện cao (0.75), hoãn tính nhịp thở vi sai.
    if (frame.motion_ready && frame.motion_score > kMaximumGrossMotionScore) {
        settling_frames_ = 20; // 20 frame đệm chuyển tiếp khi ngừng di chuyển
        presence_confidence_ = std::max(presence_confidence_, 0.75f);
        current_quality_ = 0.0f;
        estimate_counter_ = 0;

        frame.breath_valid = false;
        frame.breath_bpm = current_bpm_;
        frame.breath_wave = 0.0f;
        frame.breath_regularity = 0.0f;
        frame.is_human_presence = true; // Người đang di chuyển trong phòng
        frame.is_sleeping = false;
        frame.presence_status = "human_moving";
        return;
    }

    if (settling_frames_ > 0) {
        --settling_frames_;
    }

    // 1. Lọc Bandpass thời gian thực chạy trên TỪNG FRAME để sóng mượt mà 60Hz không giật bậc thang
    const float composite_x = synthesizeCompositePhase(unwrapped_phase);
    current_wave_ = real_time_filter_.process(composite_x, dt_s);
    frame.breath_wave = current_wave_;

    // 2. Lưu vào lịch sử cửa sổ
    phase_window_.push_back(std::move(unwrapped_phase));
    timestamp_window_ns_.push_back(timestamp_ns);
    while (phase_window_.size() > window_size_ ||
           (!timestamp_window_ns_.empty() &&
            timestamp_ns - timestamp_window_ns_.front() > kMaximumWindowNs)) {
        phase_window_.pop_front();
        timestamp_window_ns_.pop_front();
    }

    // Lưu vào lịch sử dạng sóng đã lọc
    breath_wave_history_.push_back({timestamp_ns, current_wave_});
    while (breath_wave_history_.size() > 400 ||
           (!breath_wave_history_.empty() &&
            timestamp_ns - breath_wave_history_.front().first > 25000000000ULL)) {
        breath_wave_history_.pop_front();
    }

    // 3. Cập nhật ước lượng phổ định kỳ (mỗi 5 frame ~ 6 lần/giây)
    if (++estimate_counter_ >= kEstimateInterval) {
        estimate_counter_ = 0;
        updateSensitiveTones(frame);
        updateBreathingEstimate(frame);
    } else {
        frame.breath_valid = current_breath_valid_;
        frame.breath_bpm = current_bpm_;
        frame.breath_wave = current_wave_;
        frame.breath_quality = current_quality_;
        frame.breath_phase_span = current_phase_span_;
        frame.breath_asymmetry = current_asymmetry_;
        frame.breath_rrv = current_rrv_;
        frame.breath_regularity = current_regularity_;
        frame.is_human_presence = is_human_presence_;
        frame.is_sleeping = is_sleeping_;
        frame.presence_status = presence_status_;
    }
}
