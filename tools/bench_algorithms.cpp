/**
 * tools/bench_algorithms.cpp
 * -----------------------------------------------------------------------------
 * Micro-benchmarking harness for Edge CSI Router algorithmic pipeline.
 * Measures execution time in nanoseconds/microseconds for:
 *   1. MotionDetector::update
 *   2. BreathDetector::update
 *   3. WaveDetector::update (Feature extraction + Random Forest 300 Trees)
 *   4. csi_rf1_predict (Direct 300-tree inference)
 *   5. CSIData::toJson (Full subcarrier JSON formatting)
 * -----------------------------------------------------------------------------
 */

#include "csi_parser.hpp"
#include "motion.hpp"
#include "breath.hpp"
#include "wave_detector.hpp"
#include "csi_rf_model1.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

static CSIData createSampleFrame(uint64_t ts_us, size_t num_chains = 2, size_t tones = 212) {
    CSIData frame;
    frame.timestamp_ns = ts_us * 1000ULL;
    frame.chip_tsf_us = ts_us;
    frame.phy_ppdu_id = 45000;
    frame.num_chains = static_cast<uint8_t>(num_chains);
    frame.nss = 1;
    frame.channel_bw = 2; // 80 MHz
    frame.packet_bw = 2;
    frame.chains.resize(num_chains);

    for (size_t c = 0; c < num_chains; ++c) {
        frame.chains[c].i.resize(tones);
        frame.chains[c].q.resize(tones);
        frame.chains[c].amplitudes.resize(tones);
        frame.chains[c].phases.resize(tones);

        for (size_t k = 0; k < tones; ++k) {
            float phase = static_cast<float>(k) * 0.05f + static_cast<float>(c) * 1.2f;
            float amp = 800.0f + 150.0f * std::sin(static_cast<float>(k) * 0.1f);
            int16_t ival = static_cast<int16_t>(amp * std::cos(phase));
            int16_t qval = static_cast<int16_t>(amp * std::sin(phase));

            frame.chains[c].i[k] = ival;
            frame.chains[c].q[k] = qval;
            frame.chains[c].amplitudes[k] = amp;
            frame.chains[c].phases[k] = phase;
        }
    }
    return frame;
}

int main(int argc, char **argv) {
    constexpr size_t N_WARMUP = 100;
    constexpr size_t N_ITER = 2000;

    std::cout << "================================================================================" << std::endl;
    std::cout << "      EDGE CSI ALGORITHMIC MICRO-BENCHMARK (80 MHz, 424 Subcarriers)" << std::endl;
    std::cout << "      Iterations: " << N_ITER << " per component" << std::endl;
    std::cout << "================================================================================" << std::endl;

    MotionDetector motion_det;
    BreathDetector breath_det;
    WaveDetector wave_det;

    // Warm up
    uint64_t ts = 1000000ULL;
    for (size_t i = 0; i < N_WARMUP; ++i) {
        CSIData f = createSampleFrame(ts += 50000ULL);
        motion_det.update(f);
        breath_det.update(f);
        wave_det.update(f);
        f.toJson();
    }

    // 1. Benchmark MotionDetector
    std::vector<double> motion_times_us;
    motion_times_us.reserve(N_ITER);
    for (size_t i = 0; i < N_ITER; ++i) {
        CSIData f = createSampleFrame(ts += 50000ULL);
        auto t0 = std::chrono::high_resolution_clock::now();
        motion_det.update(f);
        auto t1 = std::chrono::high_resolution_clock::now();
        motion_times_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }

    // 2. Benchmark BreathDetector
    std::vector<double> breath_times_us;
    breath_times_us.reserve(N_ITER);
    for (size_t i = 0; i < N_ITER; ++i) {
        CSIData f = createSampleFrame(ts += 50000ULL);
        auto t0 = std::chrono::high_resolution_clock::now();
        breath_det.update(f);
        auto t1 = std::chrono::high_resolution_clock::now();
        breath_times_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }

    // 3. Benchmark WaveDetector (Feature extraction + 300 Trees)
    std::vector<double> wave_times_us;
    wave_times_us.reserve(N_ITER);
    for (size_t i = 0; i < N_ITER; ++i) {
        CSIData f = createSampleFrame(ts += 50000ULL);
        auto t0 = std::chrono::high_resolution_clock::now();
        wave_det.update(f);
        auto t1 = std::chrono::high_resolution_clock::now();
        wave_times_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }

    // 4. Benchmark Direct Random Forest (300 Trees)
    std::vector<double> rf_times_us;
    rf_times_us.reserve(N_ITER);
    float dummy_feat[5] = {0.25f, 0.20f, 0.22f, 0.005f, 0.004f};
    double dummy_proba[2] = {0.0, 0.0};
    for (size_t i = 0; i < N_ITER; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        csi_rf1_predict(dummy_feat, dummy_proba);
        auto t1 = std::chrono::high_resolution_clock::now();
        rf_times_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }

    // 5. Benchmark CSIData::toJson
    std::vector<double> json_times_us;
    json_times_us.reserve(N_ITER);
    CSIData sample_f = createSampleFrame(ts += 50000ULL);
    for (size_t i = 0; i < N_ITER; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        std::string s = sample_f.toJson();
        auto t1 = std::chrono::high_resolution_clock::now();
        json_times_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }

    auto calc_stats = [](const std::vector<double> &vec) {
        double sum = std::accumulate(vec.begin(), vec.end(), 0.0);
        double mean = sum / vec.size();
        double sq_sum = 0.0;
        double min_val = vec[0];
        double max_val = vec[0];
        for (double v : vec) {
            sq_sum += (v - mean) * (v - mean);
            if (v < min_val) min_val = v;
            if (v > max_val) max_val = v;
        }
        double stdev = std::sqrt(sq_sum / vec.size());
        return std::make_tuple(mean, stdev, min_val, max_val);
    };

    auto [m_mean, m_std, m_min, m_max] = calc_stats(motion_times_us);
    auto [b_mean, b_std, b_min, b_max] = calc_stats(breath_times_us);
    auto [w_mean, w_std, w_min, w_max] = calc_stats(wave_times_us);
    auto [rf_mean, rf_std, rf_min, rf_max] = calc_stats(rf_times_us);
    auto [j_mean, j_std, j_min, j_max] = calc_stats(json_times_us);

    double total_pipeline_mean_us = m_mean + b_mean + w_mean + j_mean;
    constexpr double CYCLE_BUDGET_US = 50000.0; // 50ms at 20 Hz
    double cpu_budget_pct = (total_pipeline_mean_us / CYCLE_BUDGET_US) * 100.0;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "| " << std::left << std::setw(38) << "Component / Stage"
              << "| " << std::right << std::setw(10) << "Mean (us)"
              << "| " << std::right << std::setw(10) << "Min (us)"
              << "| " << std::right << std::setw(10) << "Max (us)"
              << "| " << std::right << std::setw(10) << "StdDev (us)"
              << " |" << std::endl;
    std::cout << "|" << std::string(40, '-') << "|" << std::string(12, '-')
              << "|" << std::string(12, '-') << "|" << std::string(12, '-')
              << "|" << std::string(13, '-') << "|" << std::endl;

    std::cout << "| " << std::left << std::setw(38) << "1. MotionDetector::update"
              << "| " << std::right << std::setw(10) << m_mean
              << "| " << std::right << std::setw(10) << m_min
              << "| " << std::right << std::setw(10) << m_max
              << "| " << std::right << std::setw(10) << m_std << " |" << std::endl;

    std::cout << "| " << std::left << std::setw(38) << "2. BreathDetector::update"
              << "| " << std::right << std::setw(10) << b_mean
              << "| " << std::right << std::setw(10) << b_min
              << "| " << std::right << std::setw(10) << b_max
              << "| " << std::right << std::setw(10) << b_std << " |" << std::endl;

    std::cout << "| " << std::left << std::setw(38) << "3. WaveDetector::update (Total)"
              << "| " << std::right << std::setw(10) << w_mean
              << "| " << std::right << std::setw(10) << w_min
              << "| " << std::right << std::setw(10) << w_max
              << "| " << std::right << std::setw(10) << w_std << " |" << std::endl;

    std::cout << "| " << std::left << std::setw(38) << "   -> csi_rf1_predict (300 Trees)"
              << "| " << std::right << std::setw(10) << rf_mean
              << "| " << std::right << std::setw(10) << rf_min
              << "| " << std::right << std::setw(10) << rf_max
              << "| " << std::right << std::setw(10) << rf_std << " |" << std::endl;

    std::cout << "| " << std::left << std::setw(38) << "4. CSIData::toJson (424 tones)"
              << "| " << std::right << std::setw(10) << j_mean
              << "| " << std::right << std::setw(10) << j_min
              << "| " << std::right << std::setw(10) << j_max
              << "| " << std::right << std::setw(10) << j_std << " |" << std::endl;

    std::cout << "================================================================================" << std::endl;
    std::cout << "  TOTAL ALGORITHMIC PIPELINE LATENCY: " << total_pipeline_mean_us << " us ("
              << (total_pipeline_mean_us / 1000.0) << " ms per frame)" << std::endl;
    std::cout << "  AVAILABLE CYCLE BUDGET (20 Hz):     " << (CYCLE_BUDGET_US / 1000.0) << " ms" << std::endl;
    std::cout << "  SINGLE-CORE CPU CONSUMPTION:        " << cpu_budget_pct << " %" << std::endl;
    std::cout << "  THEORETICAL MAX FRAME RATE:         " << (1000000.0 / total_pipeline_mean_us) << " FPS / Hz" << std::endl;
    std::cout << "================================================================================" << std::endl;

    return 0;
}
