#include "wave_detector.hpp"
#include "csi_rf_model1.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

WaveDetector::WaveDetector() {
    reset();
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

float WaveDetector::computeMedian(std::vector<float> &values) {
    if (values.empty())
        return 0.0f;
    std::sort(values.begin(), values.end());
    const size_t mid = values.size() / 2;
    if (values.size() % 2 == 0) {
        return (values[mid - 1] + values[mid]) * 0.5f;
    }
    return values[mid];
}

void WaveDetector::update(CSIData &frame) {
    if (frame.chains.size() < kMinChains) {
        status_ = "unavailable";
        reason_ = "waiting_2chains";
        frame.wave_detected = false;
        frame.wave_confidence = 0.0f;
        frame.wave_status = status_;
        frame.wave_samples_in_window = 0;
        frame.wave_raw_prob = 0.0f;
        frame.wave_reason = reason_;
        return;
    }

    const size_t available_tones = frame.chains[0].amplitudes.empty()
                                       ? frame.chains[0].i.size()
                                       : frame.chains[0].amplitudes.size();

    if (available_tones < 16) {
        status_ = "unavailable";
        reason_ = "waiting_tones";
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

    const size_t available_chains = frame.chains.size();
    const size_t chains_to_extract = std::min(available_chains, static_cast<size_t>(3));

    for (size_t ch = 0; ch < chains_to_extract; ++ch) {
        const auto &amp = frame.chains[ch].amplitudes;
        const auto &i_vec = frame.chains[ch].i;
        const auto &q_vec = frame.chains[ch].q;

        if (!amp.empty()) {
            if (amp.size() >= 212) {
                // Qualcomm 80 MHz raw CFR: 4 blocks of 53 tones. DC tones at k % 53 == 26
                size_t o = 0;
                for (size_t k = 0; k < 212 && o < kNonDcTones; ++k) {
                    if (k % 53 == 26)
                        continue;
                    sample.amp[ch][o++] = amp[k];
                }
            } else if (amp.size() >= kNonDcTones) {
                for (size_t k = 0; k < kNonDcTones; ++k) {
                    sample.amp[ch][k] = amp[k];
                }
            } else {
                // 20 MHz (52 tones) or 40 MHz (108 tones): resample across 208 tone grid
                const size_t n_in = amp.size();
                for (size_t o = 0; o < kNonDcTones; ++o) {
                    const size_t src_k = (o * n_in) / kNonDcTones;
                    sample.amp[ch][o] = amp[src_k];
                }
            }
        } else if (!i_vec.empty() && !q_vec.empty()) {
            if (i_vec.size() >= 212) {
                size_t o = 0;
                for (size_t k = 0; k < 212 && o < kNonDcTones; ++k) {
                    if (k % 53 == 26)
                        continue;
                    sample.amp[ch][o++] = std::hypot(static_cast<float>(i_vec[k]),
                                                     static_cast<float>(q_vec[k]));
                }
            } else if (i_vec.size() >= kNonDcTones) {
                for (size_t k = 0; k < kNonDcTones; ++k) {
                    sample.amp[ch][k] = std::hypot(static_cast<float>(i_vec[k]),
                                                   static_cast<float>(q_vec[k]));
                }
            } else {
                const size_t n_in = i_vec.size();
                for (size_t o = 0; o < kNonDcTones; ++o) {
                    const size_t src_k = (o * n_in) / kNonDcTones;
                    sample.amp[ch][o] = std::hypot(static_cast<float>(i_vec[src_k]),
                                                   static_cast<float>(q_vec[src_k]));
                }
            }
        }
    }

    // 2-ANTENNA FALLBACK: If only 2 chains are available, mirror chain 1 to chain 2
    if (available_chains == 2) {
        std::memcpy(sample.amp[2], sample.amp[1], sizeof(sample.amp[2]));
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

    // Prune history older than 3.5 seconds
    while (!history_.empty() && (ts_us - history_.front().timestamp_us > 3500000ULL)) {
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
        if (is_waving_ && (current_ts_us > last_active_ts_us_) &&
            (current_ts_us - last_active_ts_us_ >= kHoldDurationUs)) {
            is_waving_ = false;
        }
        return;
    }

    // Filter consecutive sample pairs with dt <= 100ms (matching sparse_wave.py MAX_PAIR_GAP_S = 0.1)
    struct PairData {
        float delta_rate[3];
        float shape_step_rate[3];
    };
    std::vector<PairData> pairs;
    pairs.reserve(n);

    for (size_t i = 0; i + 1 < n; ++i) {
        const Sample *s0 = window_samples[i];
        const Sample *s1 = window_samples[i + 1];
        if (s1->timestamp_us <= s0->timestamp_us)
            continue;
        const uint64_t dt_us = s1->timestamp_us - s0->timestamp_us;
        if (dt_us > kMaxPairGapUs)
            continue;

        const double dt_s = static_cast<double>(dt_us) / 1000000.0;
        const double dt_factor = dt_s / 0.03125;

        PairData pd{};
        for (size_t ch = 0; ch < 3; ++ch) {
            double sum_prev = 0.0;
            double diff_abs_sum = 0.0;
            double norm0_sq = 0.0;
            double norm1_sq = 0.0;
            double dot = 0.0;

            for (size_t q = 0; q < kNonDcTones; ++q) {
                const double a0 = s0->amp[ch][q];
                const double a1 = s1->amp[ch][q];
                sum_prev += a0;
                diff_abs_sum += std::abs(a1 - a0);
                norm0_sq += a0 * a0;
                norm1_sq += a1 * a1;
                dot += a0 * a1;
            }

            const double mean_prev = sum_prev / static_cast<double>(kNonDcTones);
            const double mean_diff = diff_abs_sum / static_cast<double>(kNonDcTones);
            const double delta = mean_diff / (mean_prev + 1e-9);
            pd.delta_rate[ch] = static_cast<float>(delta / (dt_factor + 1e-9));

            const double norm0 = std::sqrt(norm0_sq) + 1e-9;
            const double norm1 = std::sqrt(norm1_sq) + 1e-9;
            const double cos_sim = dot / (norm0 * norm1);
            const double shape_step = (1.0 - cos_sim) / (dt_factor + 1e-9);
            pd.shape_step_rate[ch] = static_cast<float>(shape_step);
        }
        pairs.push_back(pd);
    }

    if (pairs.size() < kMinShortPairs) {
        status_ = "unavailable";
        reason_ = "too_few_local_pairs";
        if (is_waving_ && (current_ts_us > last_active_ts_us_) &&
            (current_ts_us - last_active_ts_us_ >= kHoldDurationUs)) {
            is_waving_ = false;
        }
        return;
    }

    // Extract 5 features matching feature_columns of sparse_wave:
    // 0: relative_delta_rate_c0
    // 1: relative_delta_rate_c1
    // 2: relative_delta_rate_c2
    // 3: shape_step_rate_c0
    // 4: shape_step_rate_c2
    std::vector<float> v_dr0, v_dr1, v_dr2, v_sr0, v_sr2;
    v_dr0.reserve(pairs.size());
    v_dr1.reserve(pairs.size());
    v_dr2.reserve(pairs.size());
    v_sr0.reserve(pairs.size());
    v_sr2.reserve(pairs.size());

    for (const auto &p : pairs) {
        v_dr0.push_back(p.delta_rate[0]);
        v_dr1.push_back(p.delta_rate[1]);
        v_dr2.push_back(p.delta_rate[2]);
        v_sr0.push_back(p.shape_step_rate[0]);
        v_sr2.push_back(p.shape_step_rate[2]);
    }

    float features[CSI_RF1_N_INPUT];
    features[0] = computeMedian(v_dr0);
    features[1] = computeMedian(v_dr1);
    features[2] = computeMedian(v_dr2);
    features[3] = computeMedian(v_sr0);
    features[4] = computeMedian(v_sr2);

    double proba[CSI_RF1_N_CLASSES] = {0.0, 0.0};
    csi_rf1_predict(features, proba);
    raw_prob_ = static_cast<float>(proba[1]);
    wave_confidence_ = raw_prob_;

    // Instant trigger when P >= 0.35 with 1.5s debounce hold time
    if (raw_prob_ >= trigger_threshold_) {
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
