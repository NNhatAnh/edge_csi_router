#include "breath.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinBreathHz = 0.10f; // 6 nhịp/phút
constexpr float kMaxBreathHz = 0.55f; // 33 nhịp/phút
constexpr float kBreathFrequencyStepHz = 0.01f;
constexpr float kFrequencyAgreementHz = 0.045f;
constexpr float kMinimumPeriodicity = 0.18f;
constexpr uint64_t kMinimumWindowNs =
    8000000000ULL; // 8 giây tối thiểu để bắt đầu có sóng
constexpr uint64_t kMaximumWindowNs = 30000000000ULL; // 30 giây tối đa
constexpr size_t kMinimumSamples = 30;
constexpr float kMaximumGrossMotionScore = 0.40f;
constexpr size_t kEstimateInterval = 5;

// Ngưỡng độ lệch pha vi sai:
// Thở nhẹ/bình thường ở xa tạo độ lệch pha nhỏ (~0.02 - 0.10 rad).
// Thở sâu hoặc người gần tạo độ lệch pha lớn hơn.
constexpr float kMinHumanPhaseSpan =
    0.02f; // Hạ ngưỡng để nhận diện cả thở nông/nhẹ
constexpr float kMaxHumanPhaseSpan =
    15.0f; // Không chặn cứng, chỉ cần có nhịp điệu sinh học

float wrapPhase(float phase) {
    while (phase > kPi)
        phase -= 2.0f * kPi;
    while (phase < -kPi)
        phase += 2.0f * kPi;
    return phase;
}
} // namespace

BreathDetector::BreathDetector(size_t window_size)
    : window_size_(window_size) {}

void BreathDetector::reset() {
    phase_window_.clear();
    timestamp_window_ns_.clear();
    previous_wrapped_phase_.clear();
    previous_unwrapped_phase_.clear();
    estimate_counter_ = 0;
    current_bpm_ = 0.0f;
    current_wave_ = 0.0f;
    current_quality_ = 0.0f;
    current_phase_span_ = 0.0f;
    current_asymmetry_ = 0.0f;
    current_rrv_ = 0.0f;
    current_regularity_ = 0.0f;
    current_breath_valid_ = false;
    is_human_presence_ = false;
    is_sleeping_ = false;
    settling_frames_ = 0;
    presence_status_ = "empty_room";
    breath_wave_history_.clear();
    human_confirm_streak_ = 0;
    reject_streak_ = 0;
}

std::vector<float>
BreathDetector::getWrappedPhaseDiff(const CSIData &frame) const {
    if (frame.chains.empty())
        return {};

    size_t tone_count = frame.tonesPerChain();
    tone_count = std::min(tone_count, frame.chains[0].phases.size());
    if (frame.chains.size() >= 2)
        tone_count = std::min(tone_count, frame.chains[1].phases.size());
    if (tone_count == 0)
        return {};

    std::vector<float> phase(tone_count);
    for (size_t tone = 0; tone < tone_count; ++tone) {
        if (frame.chains.size() >= 2 && tone < frame.chains[1].phases.size()) {
            // Hiệu pha giữa 2 antenna chain loại bỏ hoàn toàn Carrier Frequency
            // Offset (CFO) và Sampling Offset (SFO)
            phase[tone] = wrapPhase(frame.chains[0].phases[tone] -
                                    frame.chains[1].phases[tone]);
        } else {
            phase[tone] = wrapPhase(frame.chains[0].phases[tone]);
        }
    }
    return phase;
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

    const size_t overlap = wave.size() - period_samples;
    if (overlap < 10)
        return 0.0f;

    double sum_x = 0.0;
    double sum_y = 0.0;
    for (size_t i = 0; i < overlap; ++i) {
        sum_x += wave[i];
        sum_y += wave[i + period_samples];
    }
    const double mean_x = sum_x / static_cast<double>(overlap);
    const double mean_y = sum_y / static_cast<double>(overlap);

    double covariance = 0.0;
    double energy_x = 0.0;
    double energy_y = 0.0;
    for (size_t i = 0; i < overlap; ++i) {
        const double dx = wave[i] - mean_x;
        const double dy = wave[i + period_samples] - mean_y;
        covariance += dx * dy;
        energy_x += dx * dx;
        energy_y += dy * dy;
    }

    const double denom = std::sqrt(energy_x * energy_y);
    if (denom < 1.0e-9)
        return 0.0f;

    const double corr = std::clamp(covariance / denom, 0.0, 1.0);
    const double observed_cycles =
        static_cast<double>(wave.size()) / static_cast<double>(period_samples);
    const double cycle_coverage = std::clamp(observed_cycles - 1.0, 0.0, 1.0);
    return static_cast<float>(corr * cycle_coverage);
}

WaveMorphology BreathDetector::analyzeWaveform() {
    WaveMorphology morph;
    morph.is_human = false;
    morph.is_sleeping = false;
    morph.regularity = 0.0f;
    morph.classification = "empty_room";

    if (breath_wave_history_.size() < 40) {
        morph.classification = "calibrating";
        return morph;
    }

    const uint64_t duration_ns =
        breath_wave_history_.back().first - breath_wave_history_.front().first;
    if (duration_ns < 10000000000ULL) // Cần ít nhất 10 giây lịch sử sóng
    {
        morph.classification = "calibrating";
        return morph;
    }

    // Trích xuất chuỗi thời gian (giây) và giá trị sóng
    const uint64_t start_ns = breath_wave_history_.front().first;
    const size_t n = breath_wave_history_.size();
    std::vector<double> time_s(n);
    std::vector<double> wave(n);

    for (size_t i = 0; i < n; ++i) {
        time_s[i] =
            static_cast<double>(breath_wave_history_[i].first - start_ns) *
            1.0e-9;
        wave[i] = breath_wave_history_[i].second;
    }

    // Làm mượt nhẹ bằng Moving Average 3 điểm để khử jitter tần số cao
    std::vector<double> smoothed = wave;
    for (size_t i = 1; i + 1 < n; ++i) {
        smoothed[i] = (wave[i - 1] + 2.0 * wave[i] + wave[i + 1]) * 0.25;
    }

    // Tính Phase Span đỉnh-đáy của tín hiệu sóng
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

    // Tính độ lệch chuẩn
    double variance = 0.0;
    for (double v : smoothed) {
        variance += (v - mean_val) * (v - mean_val);
    }
    const double std_val = std::sqrt(variance / static_cast<double>(n));

    // Bộ lọc sơ bộ: nếu biên độ sóng quá bé (< 0.02 rad), coi như phòng tĩnh
    if (morph.phase_span < kMinHumanPhaseSpan) {
        morph.classification = "empty_room";
        return morph;
    }

    // Tìm các đỉnh (Peaks) và đáy (Troughs)
    // Ngưỡng phát hiện đỉnh: cao hơn lân cận và cách đỉnh trước ít nhất 1.2s
    // (tương đương max 50 BPM)
    struct ExtremaPoint {
        double time;
        double value;
        bool is_peak;
    };
    std::vector<ExtremaPoint> extrema;

    const double peak_threshold = mean_val + 0.10 * std_val;
    const double trough_threshold = mean_val - 0.10 * std_val;

    for (size_t i = 2; i + 2 < n; ++i) {
        // Peak detection
        if (smoothed[i] > smoothed[i - 1] && smoothed[i] > smoothed[i + 1] &&
            smoothed[i] >= smoothed[i - 2] && smoothed[i] >= smoothed[i + 2] &&
            smoothed[i] > peak_threshold) {
            if (extrema.empty() || (!extrema.back().is_peak &&
                                    (time_s[i] - extrema.back().time >= 1.2))) {
                extrema.push_back({time_s[i], smoothed[i], true});
            } else if (extrema.back().is_peak &&
                       smoothed[i] > extrema.back().value) {
                extrema.back() = {time_s[i], smoothed[i], true};
            }
        }
        // Trough detection
        else if (smoothed[i] < smoothed[i - 1] &&
                 smoothed[i] < smoothed[i + 1] &&
                 smoothed[i] <= smoothed[i - 2] &&
                 smoothed[i] <= smoothed[i + 2] &&
                 smoothed[i] < trough_threshold) {
            if (extrema.empty() || (extrema.back().is_peak &&
                                    (time_s[i] - extrema.back().time >= 1.2))) {
                extrema.push_back({time_s[i], smoothed[i], false});
            } else if (!extrema.back().is_peak &&
                       smoothed[i] < extrema.back().value) {
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

    // 1. Phân tích biến thiên chu kỳ sinh lý (RRV - Respiration Rate
    // Variability)
    double mean_ibi = 0.0;
    if (peak_times.size() >= 2) {
        std::vector<double> ibis;
        for (size_t i = 1; i < peak_times.size(); ++i) {
            ibis.push_back(peak_times[i] - peak_times[i - 1]);
        }
        mean_ibi = std::accumulate(ibis.begin(), ibis.end(), 0.0) / ibis.size();
        morph.average_ibi_s = static_cast<float>(mean_ibi);

        double ibi_var = 0.0;
        for (double d : ibis) {
            ibi_var += (d - mean_ibi) * (d - mean_ibi);
        }
        const double ibi_std = std::sqrt(ibi_var / ibis.size());
        morph.rrv_cv =
            mean_ibi > 0.0 ? static_cast<float>(ibi_std / mean_ibi) : 0.0f;
    }

    // 2. Phân tích độ bất đối xứng Hít vào / Thở ra (Asymmetry Ratio)
    std::vector<double> cycle_asymmetries;
    for (size_t i = 1; i + 1 < extrema.size(); ++i) {
        if (extrema[i].is_peak && !extrema[i - 1].is_peak &&
            !extrema[i + 1].is_peak) {
            const double t_rise = extrema[i].time - extrema[i - 1].time;
            const double t_fall = extrema[i + 1].time - extrema[i].time;
            if (t_rise > 0.4 && t_fall > 0.4) {
                const double slope_rise =
                    (extrema[i].value - extrema[i - 1].value) / t_rise;
                const double slope_fall =
                    (extrema[i].value - extrema[i + 1].value) / t_fall;
                const double denom =
                    std::abs(slope_rise) + std::abs(slope_fall);
                if (denom > 1.0e-5) {
                    cycle_asymmetries.push_back(
                        (std::abs(slope_rise) - std::abs(slope_fall)) / denom);
                }
            }
        }
    }

    if (!cycle_asymmetries.empty()) {
        morph.asymmetry_ratio =
            static_cast<float>(std::accumulate(cycle_asymmetries.begin(),
                                               cycle_asymmetries.end(), 0.0) /
                               cycle_asymmetries.size());
    }

    // Tính tần số lấy mẫu ước lượng và độ lặp lại chu kỳ (Autocorrelation
    // Regularity)
    const double total_duration_s = time_s.back() - time_s.front();
    const double estimated_fs =
        total_duration_s > 0.0 ? (static_cast<double>(n - 1) / total_duration_s)
                               : 10.0;
    const float est_freq =
        morph.average_ibi_s > 0.0f
            ? (1.0f / morph.average_ibi_s)
            : (current_bpm_ > 0.0f ? current_bpm_ / 60.0f : 0.25f);
    morph.regularity =
        computeAutocorrelationRegularity(smoothed, est_freq, estimated_fs);

    // 3. Phân loại có người:
    // Dù biên độ sóng không cao (nhỏ nhưng > 0.02 rad), nếu biểu hiện nhịp điệu
    // sinh học (khoảng cách đỉnh nằm trong dải 1.5s - 10s hoặc độ tự tương quan
    // >= 0.25) thì xác nhận CÓ NGƯỜI
    const bool has_rhythm =
        (mean_ibi >= 1.5 && mean_ibi <= 10.0) || (morph.regularity >= 0.25f);
    if (has_rhythm && morph.phase_span >= kMinHumanPhaseSpan) {
        morph.is_human = true;
        morph.classification = "human_present";

        // Nhận diện trạng thái ngủ: nhịp thở chậm đều (8 - 20 BPM), tính lặp
        // lại chu kỳ cao
        if (morph.average_ibi_s >= 3.0f && morph.average_ibi_s <= 7.5f &&
            morph.rrv_cv <= 0.25f && morph.regularity >= 0.40f) {
            morph.is_sleeping = true;
        }
    } else {
        morph.is_human = false;
        morph.is_sleeping = false;
        morph.classification = "empty_room";
    }

    return morph;
}

void BreathDetector::updateBreathingEstimate(CSIData &frame) {
    frame.breath_valid = false;
    frame.breath_bpm = current_bpm_;
    frame.breath_wave = 0.0f;
    frame.breath_quality = current_quality_;
    frame.breath_phase_span = current_phase_span_;
    frame.breath_asymmetry = current_asymmetry_;
    frame.breath_rrv = current_rrv_;
    frame.is_human_presence = is_human_presence_;
    frame.presence_status = presence_status_;

    if (phase_window_.size() < kMinimumSamples ||
        timestamp_window_ns_.back() - timestamp_window_ns_.front() <
            kMinimumWindowNs)
        return;

    const size_t tone_count = phase_window_.front().size();
    const size_t first_tone = tone_count / 4;
    const size_t last_tone = (tone_count * 3) / 4;
    if (last_tone <= first_tone)
        return;

    const uint64_t start_ns = timestamp_window_ns_.front();
    const size_t sample_count = phase_window_.size();
    const double mean_time =
        std::accumulate(
            timestamp_window_ns_.begin(), timestamp_window_ns_.end(), 0.0,
            [start_ns](double sum, uint64_t timestamp) {
                return sum + static_cast<double>(timestamp - start_ns) * 1.0e-9;
            }) /
        static_cast<double>(sample_count);

    std::vector<double> tone_quality(tone_count, 0.0);
    std::vector<float> tone_frequency(tone_count, 0.0f);
    std::vector<float> tone_latest_value(tone_count, 0.0f);
    std::vector<float> tone_span(tone_count, 0.0f);
    std::vector<bool> tone_valid_span(tone_count, false);

    std::vector<float> frequencies;
    std::vector<std::vector<double>> cosine_basis;
    std::vector<std::vector<double>> sine_basis;
    for (float frequency = kMinBreathHz; frequency <= kMaxBreathHz;
         frequency += kBreathFrequencyStepHz) {
        frequencies.push_back(frequency);
        cosine_basis.emplace_back(sample_count);
        sine_basis.emplace_back(sample_count);
        for (size_t sample_index = 0; sample_index < sample_count;
             ++sample_index) {
            const double time =
                static_cast<double>(timestamp_window_ns_[sample_index] -
                                    start_ns) *
                1.0e-9;
            const double angle =
                2.0 * static_cast<double>(kPi) * frequency * time;
            cosine_basis.back()[sample_index] = std::cos(angle);
            sine_basis.back()[sample_index] = std::sin(angle);
        }
    }

    for (size_t tone = first_tone; tone < last_tone; ++tone) {
        double mean_value = 0.0;
        for (const auto &sample : phase_window_)
            mean_value += sample[tone];
        mean_value /= static_cast<double>(sample_count);

        double covariance_time_value = 0.0;
        double variance_time = 0.0;
        for (size_t sample_index = 0; sample_index < sample_count;
             ++sample_index) {
            const double time =
                static_cast<double>(timestamp_window_ns_[sample_index] -
                                    start_ns) *
                1.0e-9;
            const double value = phase_window_[sample_index][tone] - mean_value;
            covariance_time_value += (time - mean_time) * value;
            variance_time += (time - mean_time) * (time - mean_time);
        }
        const double slope =
            variance_time > 0.0 ? covariance_time_value / variance_time : 0.0;

        std::vector<double> residuals(sample_count);
        double residual_energy = 0.0;
        double min_res = 1.0e9;
        double max_res = -1.0e9;

        for (size_t sample_index = 0; sample_index < sample_count;
             ++sample_index) {
            const double time =
                static_cast<double>(timestamp_window_ns_[sample_index] -
                                    start_ns) *
                1.0e-9;
            residuals[sample_index] = phase_window_[sample_index][tone] -
                                      mean_value - slope * (time - mean_time);
            residual_energy +=
                residuals[sample_index] * residuals[sample_index];
            min_res = std::min(min_res, residuals[sample_index]);
            max_res = std::max(max_res, residuals[sample_index]);
        }
        if (residual_energy < 1.0e-8)
            continue;

        const float span = static_cast<float>(max_res - min_res);
        tone_span[tone] = span;

        // Đưa các subcarrier có dao động pha (>= 0.02 rad) vào phân tích phổ
        if (span >= kMinHumanPhaseSpan && span <= kMaxHumanPhaseSpan) {
            tone_valid_span[tone] = true;
            for (size_t frequency_index = 0;
                 frequency_index < frequencies.size(); ++frequency_index) {
                double cosine_sum = 0.0;
                double sine_sum = 0.0;
                for (size_t sample_index = 0; sample_index < sample_count;
                     ++sample_index) {
                    cosine_sum += residuals[sample_index] *
                                  cosine_basis[frequency_index][sample_index];
                    sine_sum += residuals[sample_index] *
                                sine_basis[frequency_index][sample_index];
                }

                const double explained_energy =
                    2.0 * (cosine_sum * cosine_sum + sine_sum * sine_sum) /
                    static_cast<double>(sample_count);
                const double quality = explained_energy / residual_energy;
                if (quality > tone_quality[tone]) {
                    tone_quality[tone] = quality;
                    tone_frequency[tone] = frequencies[frequency_index];
                    tone_latest_value[tone] =
                        static_cast<float>(residuals.back());
                }
            }
        }
    }

    const size_t evaluated_tones = last_tone - first_tone;
    double best_quality = 0.0;
    double best_frequency = 0.0;
    double best_latest_value = 0.0;
    double best_span = 0.0;
    size_t best_tone_count = 0;
    const size_t minimum_agreeing_tones =
        std::max<size_t>(2, evaluated_tones / 16);

    for (size_t candidate_tone = first_tone; candidate_tone < last_tone;
         ++candidate_tone) {
        if (!tone_valid_span[candidate_tone] ||
            tone_quality[candidate_tone] < kMinimumPeriodicity)
            continue;

        double quality_sum = 0.0;
        double frequency_sum = 0.0;
        double wave_sum = 0.0;
        double span_sum = 0.0;
        size_t agreeing_tones = 0;

        for (size_t tone = first_tone; tone < last_tone; ++tone) {
            if (!tone_valid_span[tone] ||
                tone_quality[tone] < kMinimumPeriodicity ||
                std::abs(tone_frequency[tone] -
                         tone_frequency[candidate_tone]) >
                    kFrequencyAgreementHz)
                continue;

            quality_sum += tone_quality[tone];
            frequency_sum += tone_frequency[tone];
            wave_sum += tone_latest_value[tone];
            span_sum += tone_span[tone];
            ++agreeing_tones;
        }

        if (agreeing_tones >= minimum_agreeing_tones &&
            quality_sum / agreeing_tones > best_quality) {
            best_tone_count = agreeing_tones;
            best_quality = quality_sum / agreeing_tones;
            best_frequency = frequency_sum / agreeing_tones;
            best_span = span_sum / agreeing_tones;

            // Hợp nhất có trọng số theo bình phương quality (Weighted
            // Subcarrier Fusion)
            double weight_sum = 0.0;
            double weighted_wave_sum = 0.0;
            for (size_t tone = first_tone; tone < last_tone; ++tone) {
                if (tone_valid_span[tone] &&
                    tone_quality[tone] >= kMinimumPeriodicity &&
                    std::abs(tone_frequency[tone] - best_frequency) <=
                        kFrequencyAgreementHz) {
                    const double w = tone_quality[tone] * tone_quality[tone];
                    weighted_wave_sum +=
                        w * static_cast<double>(tone_latest_value[tone]);
                    weight_sum += w;
                }
            }
            if (weight_sum > 1.0e-6) {
                best_latest_value = weighted_wave_sum / weight_sum;
            } else {
                best_latest_value = wave_sum / agreeing_tones;
            }
        }
    }

    current_quality_ = static_cast<float>(best_quality);
    current_phase_span_ = static_cast<float>(best_span);
    current_wave_ = static_cast<float>(best_latest_value);
    frame.breath_wave = current_wave_;

    // Lưu vào lịch sử dạng sóng để phân tích hình thái
    breath_wave_history_.push_back({frame.timestamp_ns, frame.breath_wave});
    while (breath_wave_history_.size() > 400 ||
           (!breath_wave_history_.empty() &&
            frame.timestamp_ns - breath_wave_history_.front().first >
                25000000000ULL)) {
        breath_wave_history_.pop_front();
    }

    // Tiến hành phân tích hình thái sinh học
    WaveMorphology morph = analyzeWaveform();
    current_asymmetry_ = morph.asymmetry_ratio;
    current_rrv_ = morph.rrv_cv;
    current_regularity_ = morph.regularity;
    is_sleeping_ = morph.is_sleeping;

    if (best_quality >= kMinimumPeriodicity &&
        best_tone_count >= minimum_agreeing_tones && best_frequency > 0.0) {
        const float estimate = static_cast<float>(best_frequency * 60.0);
        current_bpm_ = current_bpm_ == 0.0f
                           ? estimate
                           : 0.8f * current_bpm_ + 0.2f * estimate;

        // Chỉ chấp nhận nhịp thở người khi vượt qua bộ lọc hình thái sinh học
        if (morph.is_human) {
            reject_streak_ = 0;
            if (++human_confirm_streak_ >=
                2) // xác nhận liên tiếp để chống chập chờn
            {
                is_human_presence_ = true;
                current_breath_valid_ = true;
                presence_status_ = "human_present";
            }
        } else {
            human_confirm_streak_ = 0;
            presence_status_ = morph.classification;
            if (++reject_streak_ >= 2) {
                is_human_presence_ = false;
                current_breath_valid_ = false;
                is_sleeping_ = false;
            }
        }
    } else {
        human_confirm_streak_ = 0;
        if (++reject_streak_ >= 3) {
            is_human_presence_ = false;
            current_breath_valid_ = false;
            is_sleeping_ = false;
            presence_status_ = morph.classification.empty()
                                   ? "empty_room"
                                   : morph.classification;
        }
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
    frame.breath_valid = current_breath_valid_;
    frame.breath_bpm = current_bpm_;
    frame.breath_wave = 0.0f;
    frame.breath_quality = current_quality_;
    frame.breath_phase_span = current_phase_span_;
    frame.breath_asymmetry = current_asymmetry_;
    frame.breath_rrv = current_rrv_;
    frame.breath_regularity = current_regularity_;
    frame.is_human_presence = is_human_presence_;
    frame.is_sleeping = is_sleeping_;
    frame.presence_status = presence_status_;

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

    std::vector<float> unwrapped_phase(wrapped_phase.size());
    for (size_t tone = 0; tone < wrapped_phase.size(); ++tone) {
        unwrapped_phase[tone] =
            previous_unwrapped_phase_[tone] +
            wrapPhase(wrapped_phase[tone] - previous_wrapped_phase_[tone]);
    }
    previous_wrapped_phase_ = wrapped_phase;
    previous_unwrapped_phase_ = unwrapped_phase;

    // Nếu motion_score vĩ mô quá lớn (> 0.35), có chuyển động bước đi hoặc rung
    // lắc mạnh, tạm ngưng cập nhật nhịp thở
    if (frame.motion_ready && frame.motion_score > kMaximumGrossMotionScore) {
        settling_frames_ = 15; // 15 frame đệm chuyển tiếp khi dừng di chuyển
        current_quality_ = 0.0f;
        estimate_counter_ = 0;
        frame.breath_valid = false;
        frame.breath_bpm = current_bpm_;
        frame.breath_wave = 0.0f;
        frame.breath_regularity = 0.0f;
        frame.is_human_presence = false;
        frame.is_sleeping = false;
        frame.presence_status = "human_moving";
        return;
    }

    if (settling_frames_ > 0) {
        --settling_frames_;
    }

    phase_window_.push_back(std::move(unwrapped_phase));
    timestamp_window_ns_.push_back(timestamp_ns);
    while (phase_window_.size() > window_size_ ||
           (!timestamp_window_ns_.empty() &&
            timestamp_ns - timestamp_window_ns_.front() > kMaximumWindowNs)) {
        phase_window_.pop_front();
        timestamp_window_ns_.pop_front();
    }

    if (++estimate_counter_ >= kEstimateInterval) {
        estimate_counter_ = 0;
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
