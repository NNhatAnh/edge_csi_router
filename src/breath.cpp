#include "breath.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kMinBreathHz = 0.10f;
    constexpr float kMaxBreathHz = 0.55f;
    constexpr float kBreathFrequencyStepHz = 0.01f;
    constexpr float kFrequencyAgreementHz = 0.03f;
    constexpr float kMinimumPeriodicity = 0.30f;
    constexpr uint64_t kMinimumWindowNs = 15000000000ULL;
    constexpr uint64_t kMaximumWindowNs = 30000000000ULL;
    constexpr size_t kMinimumSamples = 60;
    constexpr float kMaximumGrossMotionScore = 0.35f;
    constexpr size_t kEstimateInterval = 15;

    float wrapPhase(float phase)
    {
        while (phase > kPi)
            phase -= 2.0f * kPi;
        while (phase < -kPi)
            phase += 2.0f * kPi;
        return phase;
    }
} // namespace

BreathDetector::BreathDetector(size_t window_size)
    : window_size_(window_size)
{
}

void BreathDetector::reset()
{
    phase_window_.clear();
    timestamp_window_ns_.clear();
    previous_wrapped_phase_.clear();
    previous_unwrapped_phase_.clear();
    estimate_counter_ = 0;
    current_bpm_ = 0.0f;
    current_quality_ = 0.0f;
}

std::vector<float> BreathDetector::getWrappedPhaseDiff(const CSIData &frame) const
{
    if (frame.chains.empty())
        return {};

    size_t tone_count = frame.tonesPerChain();
    tone_count = std::min(tone_count, frame.chains[0].phases.size());
    if (frame.chains.size() >= 2)
        tone_count = std::min(tone_count, frame.chains[1].phases.size());
    if (tone_count == 0)
        return {};

    std::vector<float> phase(tone_count);
    for (size_t tone = 0; tone < tone_count; ++tone)
    {
        if (frame.chains.size() >= 2 &&
            tone < frame.chains[1].phases.size())
        {
            phase[tone] = wrapPhase(frame.chains[0].phases[tone] -
                                    frame.chains[1].phases[tone]);
        }
        else
        {
            phase[tone] = wrapPhase(frame.chains[0].phases[tone]);
        }
    }
    return phase;
}

void BreathDetector::updateBreathingEstimate(CSIData &frame)
{
    frame.breath_valid = false;
    frame.breath_bpm = current_bpm_;
    frame.breath_wave = 0.0f;
    current_quality_ = 0.0f;

    if (phase_window_.size() < kMinimumSamples ||
        timestamp_window_ns_.back() - timestamp_window_ns_.front() < kMinimumWindowNs)
        return;

    const size_t tone_count = phase_window_.front().size();
    const size_t first_tone = tone_count / 4;
    const size_t last_tone = (tone_count * 3) / 4;
    if (last_tone <= first_tone)
        return;

    const uint64_t start_ns = timestamp_window_ns_.front();
    const size_t sample_count = phase_window_.size();
    const double mean_time = std::accumulate(
                                 timestamp_window_ns_.begin(), timestamp_window_ns_.end(), 0.0,
                                 [start_ns](double sum, uint64_t timestamp)
                                 { return sum + static_cast<double>(timestamp - start_ns) * 1.0e-9; }) /
                             static_cast<double>(sample_count);

    std::vector<double> tone_quality(tone_count, 0.0);
    std::vector<float> tone_frequency(tone_count, 0.0f);
    std::vector<float> tone_latest_value(tone_count, 0.0f);
    std::vector<float> frequencies;
    std::vector<std::vector<double>> cosine_basis;
    std::vector<std::vector<double>> sine_basis;
    for (float frequency = kMinBreathHz; frequency <= kMaxBreathHz;
         frequency += kBreathFrequencyStepHz)
    {
        frequencies.push_back(frequency);
        cosine_basis.emplace_back(sample_count);
        sine_basis.emplace_back(sample_count);
        for (size_t sample_index = 0; sample_index < sample_count; ++sample_index)
        {
            const double time = static_cast<double>(timestamp_window_ns_[sample_index] - start_ns) * 1.0e-9;
            const double angle = 2.0 * static_cast<double>(kPi) * frequency * time;
            cosine_basis.back()[sample_index] = std::cos(angle);
            sine_basis.back()[sample_index] = std::sin(angle);
        }
    }

    for (size_t tone = first_tone; tone < last_tone; ++tone)
    {
        double mean_value = 0.0;
        for (const auto &sample : phase_window_)
            mean_value += sample[tone];
        mean_value /= static_cast<double>(sample_count);

        double covariance_time_value = 0.0;
        double variance_time = 0.0;
        for (size_t sample_index = 0; sample_index < sample_count; ++sample_index)
        {
            const double time = static_cast<double>(timestamp_window_ns_[sample_index] - start_ns) * 1.0e-9;
            const double value = phase_window_[sample_index][tone] - mean_value;
            covariance_time_value += (time - mean_time) * value;
            variance_time += (time - mean_time) * (time - mean_time);
        }
        const double slope = variance_time > 0.0 ? covariance_time_value / variance_time : 0.0;

        std::vector<double> residuals(sample_count);
        double residual_energy = 0.0;
        for (size_t sample_index = 0; sample_index < sample_count; ++sample_index)
        {
            const double time = static_cast<double>(timestamp_window_ns_[sample_index] - start_ns) * 1.0e-9;
            residuals[sample_index] = phase_window_[sample_index][tone] - mean_value -
                                      slope * (time - mean_time);
            residual_energy += residuals[sample_index] * residuals[sample_index];
        }
        if (residual_energy < 1.0e-8)
            continue;

        for (size_t frequency_index = 0; frequency_index < frequencies.size(); ++frequency_index)
        {
            double cosine_sum = 0.0;
            double sine_sum = 0.0;
            for (size_t sample_index = 0; sample_index < sample_count; ++sample_index)
            {
                cosine_sum += residuals[sample_index] * cosine_basis[frequency_index][sample_index];
                sine_sum += residuals[sample_index] * sine_basis[frequency_index][sample_index];
            }

            const double explained_energy =
                2.0 * (cosine_sum * cosine_sum + sine_sum * sine_sum) /
                static_cast<double>(sample_count);
            const double quality = explained_energy / residual_energy;
            if (quality > tone_quality[tone])
            {
                tone_quality[tone] = quality;
                tone_frequency[tone] = frequencies[frequency_index];
                tone_latest_value[tone] = static_cast<float>(residuals.back());
            }
        }
    }

    double best_quality = 0.0;
    double best_frequency = 0.0;
    double best_latest_value = 0.0;
    size_t best_tone_count = 0;
    const size_t minimum_agreeing_tones = std::max<size_t>(3, (last_tone - first_tone) / 8);
    for (size_t candidate_tone = first_tone; candidate_tone < last_tone; ++candidate_tone)
    {
        if (tone_quality[candidate_tone] < kMinimumPeriodicity)
            continue;

        double quality_sum = 0.0;
        double frequency_sum = 0.0;
        double wave_sum = 0.0;
        size_t agreeing_tones = 0;
        for (size_t tone = first_tone; tone < last_tone; ++tone)
        {
            if (tone_quality[tone] < kMinimumPeriodicity ||
                std::abs(tone_frequency[tone] - tone_frequency[candidate_tone]) > kFrequencyAgreementHz)
                continue;
            quality_sum += tone_quality[tone];
            frequency_sum += tone_frequency[tone];
            wave_sum += tone_latest_value[tone];
            ++agreeing_tones;
        }

        if (agreeing_tones >= minimum_agreeing_tones &&
            quality_sum / agreeing_tones > best_quality)
        {
            best_tone_count = agreeing_tones;
            best_quality = quality_sum / agreeing_tones;
            best_frequency = frequency_sum / agreeing_tones;
            best_latest_value = wave_sum / agreeing_tones;
        }
    }

    current_quality_ = static_cast<float>(best_quality);
    frame.breath_wave = static_cast<float>(best_latest_value);
    if (best_quality >= kMinimumPeriodicity &&
        best_tone_count >= minimum_agreeing_tones &&
        best_frequency > 0.0)
    {
        const float estimate = static_cast<float>(best_frequency * 60.0);
        current_bpm_ = current_bpm_ == 0.0f ? estimate : 0.8f * current_bpm_ + 0.2f * estimate;
        frame.breath_valid = true;
        frame.breath_bpm = current_bpm_;
    }
}

void BreathDetector::update(CSIData &frame)
{
    frame.breath_valid = false;
    frame.breath_bpm = current_bpm_;
    frame.breath_wave = 0.0f;
    const std::vector<float> wrapped_phase = getWrappedPhaseDiff(frame);
    if (wrapped_phase.empty())
    {
        reset();
        frame.breath_valid = false;
        frame.breath_bpm = 0.0f;
        frame.breath_wave = 0.0f;
        return;
    }

    if (wrapped_phase.size() != previous_wrapped_phase_.size())
    {
        reset();
        previous_wrapped_phase_ = wrapped_phase;
        previous_unwrapped_phase_ = wrapped_phase;
        return;
    }

    const uint64_t timestamp_ns = frame.timestamp_ns;
    if (!timestamp_window_ns_.empty() &&
        (timestamp_ns <= timestamp_window_ns_.back() ||
         timestamp_ns - timestamp_window_ns_.back() > 2000000000ULL))
    {
        reset();
        previous_wrapped_phase_ = wrapped_phase;
        previous_unwrapped_phase_ = wrapped_phase;
        return;
    }

    std::vector<float> unwrapped_phase(wrapped_phase.size());
    for (size_t tone = 0; tone < wrapped_phase.size(); ++tone)
    {
        unwrapped_phase[tone] = previous_unwrapped_phase_[tone] +
                                wrapPhase(wrapped_phase[tone] - previous_wrapped_phase_[tone]);
    }
    previous_wrapped_phase_ = wrapped_phase;
    previous_unwrapped_phase_ = unwrapped_phase;

    if (frame.motion_ready && frame.motion_score > kMaximumGrossMotionScore)
    {
        current_quality_ = 0.0f;
        estimate_counter_ = 0;
        frame.breath_valid = false;
        frame.breath_bpm = current_bpm_;
        frame.breath_wave = 0.0f;
        return;
    }

    phase_window_.push_back(std::move(unwrapped_phase));
    timestamp_window_ns_.push_back(timestamp_ns);
    while (phase_window_.size() > window_size_ ||
           (!timestamp_window_ns_.empty() &&
            timestamp_ns - timestamp_window_ns_.front() > kMaximumWindowNs))
    {
        phase_window_.pop_front();
        timestamp_window_ns_.pop_front();
    }

    if (++estimate_counter_ >= kEstimateInterval)
    {
        estimate_counter_ = 0;
        updateBreathingEstimate(frame);
    }
    else
    {
        frame.breath_valid = current_quality_ >= kMinimumPeriodicity;
        frame.breath_bpm = current_bpm_;
    }
}
