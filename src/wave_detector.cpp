#include "wave_detector.hpp"
#include "csi_rf_model1.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr double kEps = 1e-9;
} // namespace

WaveDetector::WaveDetector() {
    initTables();
}

void WaveDetector::initTables() {
    for (size_t k = 0; k < kGridSamples; ++k) {
        hann_[k] = 0.5 - 0.5 * std::cos(2.0 * M_PI * static_cast<double>(k) /
                                       static_cast<double>(kGridSamples - 1));
    }
    for (int f = 0; f < 33; ++f) {
        for (size_t k = 0; k < kGridSamples; ++k) {
            const double angle = 2.0 * M_PI * static_cast<double>(f * k) /
                                 static_cast<double>(kGridSamples);
            twc_[f][k] = std::cos(angle);
            tws_[f][k] = std::sin(angle);
        }
    }
}

void WaveDetector::reset() {
    history_.clear();
    is_waving_ = false;
    wave_confidence_ = 0.0f;
    raw_prob_ = 0.0f;
    samples_in_window_ = 0;
    status_ = "idle";
    reason_ = "idle";
    last_active_ts_us_ = 0;
    last_infer_ts_us_ = 0;
}

void WaveDetector::update(CSIData &frame) {
    if (frame.chains.size() < kRequiredChains) {
        status_ = "unavailable";
        reason_ = "waiting_3chains";
        frame.wave_detected = false;
        frame.wave_confidence = 0.0f;
        frame.wave_status = status_;
        frame.wave_samples_in_window = 0;
        frame.wave_raw_prob = 0.0f;
        frame.wave_reason = reason_;
        return;
    }

    const size_t tones = frame.chains[0].amplitudes.size();
    if (tones < kNonDcTones) {
        status_ = "unavailable";
        reason_ = "waiting_80mhz";
        frame.wave_detected = false;
        frame.wave_confidence = 0.0f;
        frame.wave_status = status_;
        frame.wave_samples_in_window = 0;
        frame.wave_raw_prob = 0.0f;
        frame.wave_reason = reason_;
        return;
    }

    // Determine microsecond timestamp
    uint64_t ts_us = frame.chip_tsf_us;
    if (ts_us == 0) {
        ts_us = frame.timestamp_ns / 1000ULL;
    }
    if (ts_us == 0) {
        frame.wave_detected = is_waving_;
        frame.wave_confidence = wave_confidence_;
        frame.wave_status = status_;
        frame.wave_samples_in_window = samples_in_window_;
        frame.wave_raw_prob = raw_prob_;
        frame.wave_reason = reason_;
        return;
    }

    Sample sample;
    sample.timestamp_us = ts_us;

    for (size_t ch = 0; ch < kRequiredChains; ++ch) {
        const auto &amp = frame.chains[ch].amplitudes;
        sample.amplitudes[ch].reserve(kNonDcTones);

        if (amp.size() >= 212) {
            // Qualcomm 80 MHz raw CFR: 4 blocks of 53 tones. DC tones at k % 53 == 26
            for (size_t k = 0; k < 212 && sample.amplitudes[ch].size() < kNonDcTones; ++k) {
                if (k % 53 == 26)
                    continue;
                sample.amplitudes[ch].push_back(amp[k]);
            }
        } else {
            // Already 208 non-DC tones
            for (size_t k = 0; k < kNonDcTones && k < amp.size(); ++k) {
                sample.amplitudes[ch].push_back(amp[k]);
            }
        }
    }

    // Monotonic timestamp handling
    if (!history_.empty() && ts_us <= history_.back().timestamp_us) {
        if (history_.back().timestamp_us - ts_us > 1000000ULL) {
            history_.clear(); // Clock wrap / reset
        } else {
            ts_us = history_.back().timestamp_us + 1;
            sample.timestamp_us = ts_us;
        }
    }

    history_.push_back(std::move(sample));

    // Prune history older than 4.5 seconds
    while (!history_.empty() && (ts_us - history_.front().timestamp_us > 4500000ULL)) {
        history_.pop_front();
    }

    // Run inference every ~100ms
    if (last_infer_ts_us_ == 0 || (ts_us - last_infer_ts_us_ >= 100000ULL)) {
        last_infer_ts_us_ = ts_us;
        processWindow(ts_us);
    }

    frame.wave_detected = is_waving_;
    frame.wave_confidence = wave_confidence_;
    frame.wave_status = status_;
    frame.wave_samples_in_window = samples_in_window_;
    frame.wave_raw_prob = raw_prob_;
    frame.wave_reason = reason_;
}

void WaveDetector::processWindow(uint64_t current_ts_us) {
    const uint64_t window_start_us =
        (current_ts_us > kWindowUs) ? (current_ts_us - kWindowUs) : 0ULL;

    std::vector<const Sample *> window_samples;
    window_samples.reserve(history_.size());
    for (const auto &s : history_) {
        if (s.timestamp_us >= window_start_us && s.timestamp_us <= current_ts_us) {
            window_samples.push_back(&s);
        }
    }

    samples_in_window_ = static_cast<uint32_t>(window_samples.size());
    const size_t n = window_samples.size();

    if (n < kMinSamples) {
        status_ = "unavailable";
        reason_ = "too_few_samples";
        // Maintain hold timer even if brief gap occurs
        if (is_waving_ && (current_ts_us > last_active_ts_us_) &&
            (current_ts_us - last_active_ts_us_ >= kHoldDurationUs)) {
            is_waving_ = false;
        }
        return;
    }

    const double t0 = static_cast<double>(window_start_us) / 1000000.0;
    constexpr double fs = 20.0;

    double tc[kGridSamples];
    double tcc = 0.0;
    for (size_t k = 0; k < kGridSamples; ++k) {
        tc[k] = static_cast<double>(k) - (kGridSamples - 1) / 2.0;
        tcc += tc[k] * tc[k];
    }

    float features[CSI_RF1_N_INPUT];

    for (size_t ch = 0; ch < kRequiredChains; ++ch) {
        // 1. Calculate per-record inverse mean amplitude to eliminate AGC variations
        std::vector<double> inv(n);
        for (size_t r = 0; r < n; ++r) {
            double s = 0.0;
            const auto &amp = window_samples[r]->amplitudes[ch];
            for (size_t q = 0; q < kNonDcTones; ++q) {
                s += amp[q];
            }
            inv[r] = 1.0 / (s / kNonDcTones + kEps);
        }

        // 2. Interpolate linearly onto uniform 20 Hz grid (64 time points)
        double g[kGridSamples][kNonDcTones];
        size_t r = 0;
        for (size_t k = 0; k < kGridSamples; ++k) {
            const double x = t0 + static_cast<double>(k) / fs;
            while (r + 1 < n && (static_cast<double>(window_samples[r + 1]->timestamp_us) / 1e6) <= x) {
                ++r;
            }
            double w = 0.0;
            size_t r2 = r;
            const double tr = static_cast<double>(window_samples[r]->timestamp_us) / 1e6;
            if (x <= static_cast<double>(window_samples[0]->timestamp_us) / 1e6) {
                r = r2 = 0;
            } else if (r + 1 < n) {
                r2 = r + 1;
                const double tr2 = static_cast<double>(window_samples[r2]->timestamp_us) / 1e6;
                if (tr2 > tr)
                    w = (x - tr) / (tr2 - tr);
            }

            const auto &amp_r = window_samples[r]->amplitudes[ch];
            const auto &amp_r2 = window_samples[r2]->amplitudes[ch];
            for (size_t q = 0; q < kNonDcTones; ++q) {
                g[k][q] = amp_r[q] * inv[r] * (1.0 - w) + amp_r2[q] * inv[r2] * w;
            }
        }

        // 3. Detrend, standard deviation, step, and 32-bin Fourier power
        double sstd = 0.0;
        double sstep = 0.0;
        double power[33] = {0.0};

        for (size_t q = 0; q < kNonDcTones; ++q) {
            double m = 0.0;
            double d[kGridSamples];
            double b = 0.0;
            double v = 0.0;
            double dm = 0.0;

            for (size_t k = 0; k < kGridSamples; ++k) {
                m += g[k][q];
            }
            m /= static_cast<double>(kGridSamples);

            for (size_t k = 0; k < kGridSamples; ++k) {
                d[k] = g[k][q] - m;
                b += tc[k] * d[k];
            }
            b /= (tcc + 1e-12);

            for (size_t k = 0; k < kGridSamples; ++k) {
                d[k] -= tc[k] * b;
                dm += d[k];
            }
            dm /= static_cast<double>(kGridSamples);

            for (size_t k = 0; k < kGridSamples; ++k) {
                v += (d[k] - dm) * (d[k] - dm);
            }
            sstd += std::sqrt(v / static_cast<double>(kGridSamples));

            for (size_t k = 1; k < kGridSamples; ++k) {
                sstep += std::abs(g[k][q] - g[k - 1][q]);
            }

            for (int f = 1; f < 33; ++f) {
                double re = 0.0;
                double im = 0.0;
                for (size_t k = 0; k < kGridSamples; ++k) {
                    const double x_val = d[k] * hann_[k];
                    re += x_val * twc_[f][k];
                    im -= x_val * tws_[f][k];
                }
                power[f] += re * re + im * im;
            }
        }

        double total_power = 0.0;
        for (int f = 1; f < 33; ++f) {
            total_power += power[f] / static_cast<double>(kNonDcTones);
        }

        features[2 * ch] = static_cast<float>(sstd / kNonDcTones);
        features[2 * ch + 1] = static_cast<float>(sstep / (kNonDcTones * (kGridSamples - 1)));
        features[2 * kRequiredChains + ch] = static_cast<float>(std::log(total_power + kEps));
    }

    double proba[CSI_RF1_N_CLASSES] = {0.0, 0.0};
    csi_rf1_predict(features, proba);
    raw_prob_ = static_cast<float>(proba[1]);
    wave_confidence_ = raw_prob_;

    // Instant trigger when P >= 0.40 with 1.5s debounce hold time
    if (raw_prob_ >= kTriggerThreshold) {
        is_waving_ = true;
        last_active_ts_us_ = current_ts_us;
    } else if (is_waving_) {
        if (current_ts_us > last_active_ts_us_ &&
            (current_ts_us - last_active_ts_us_ >= kHoldDurationUs)) {
            is_waving_ = false;
        }
    }

    status_ = is_waving_ ? "waving" : "idle";
    reason_ = "ok";
}
