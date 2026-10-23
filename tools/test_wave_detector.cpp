#include "csi_parser.hpp"
#include "wave_detector.hpp"
#include "csi_rf_model1.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

void testOriginalModelPredictionDirect() {
    std::cout << "[Test] Direct Original Model prediction test (5 features: wave_rf)..." << std::endl;
    assert(CSI_RF1_N_INPUT == 5);

    // Test 1: Quiet room input
    // Features: relative_delta_rate_c0, c1, c2, shape_step_rate_c0, c2
    float quiet_input[5] = {0.05f, 0.05f, 0.05f, 0.0005f, 0.0005f};
    double proba_quiet[2] = {0.0, 0.0};
    int pred_quiet = csi_rf1_predict(quiet_input, proba_quiet);
    std::cout << "  Quiet: P(no_wave) = " << proba_quiet[0] << ", P(wave) = " << proba_quiet[1]
              << ", pred = " << pred_quiet << std::endl;
    assert(proba_quiet[1] < 0.20);
    assert(pred_quiet == 0);
    assert(std::abs(proba_quiet[0] + proba_quiet[1] - 1.0) < 1e-4);

    // Test 2: Active waving input
    float wave_input[5] = {0.45f, 0.35f, 0.40f, 0.008f, 0.008f};
    double proba_wave[2] = {0.0, 0.0};
    int pred_wave = csi_rf1_predict(wave_input, proba_wave);
    std::cout << "  Wave:  P(no_wave) = " << proba_wave[0] << ", P(wave) = " << proba_wave[1]
              << ", pred = " << pred_wave << std::endl;
    assert(proba_wave[1] >= 0.35);
    assert(pred_wave == 1);
    assert(std::abs(proba_wave[0] + proba_wave[1] - 1.0) < 1e-4);

    std::cout << "  PASS Original Model direct prediction." << std::endl;
}

void testWaveDetectorPipelineSynthetic() {
    std::cout << "[Test] WaveDetector pipeline synthetic test (3 Antennas)..." << std::endl;
    WaveDetector detector;
    assert(!detector.isWaving());

    // Generate simulated frames (80 frames @ 50ms = 4.0s duration)
    uint64_t start_ts_us = 10000000ULL;
    for (size_t f = 0; f < 80; ++f) {
        CSIData frame;
        frame.timestamp_ns = (start_ts_us + f * 50000ULL) * 1000ULL;
        frame.chip_tsf_us = start_ts_us + f * 50000ULL;
        frame.num_chains = 3;
        frame.packet_bw = 2; // 80 MHz
        frame.chains.resize(3);

        for (size_t c = 0; c < 3; ++c) {
            frame.chains[c].amplitudes.resize(212);

            for (size_t k = 0; k < 212; ++k) {
                float amp = 100.0f + 10.0f * std::sin(k * 0.1f);

                // Simulate hand movement disturbance
                if (f > 30 && f < 55) {
                    float wave_mod = 45.0f * std::sin((f - 30) * 0.8f + k * 0.2f);
                    amp += wave_mod;
                }

                frame.chains[c].amplitudes[k] = amp;
            }
        }

        detector.update(frame);

        if (f < 9) {
            assert(frame.wave_status == "unavailable");
        } else {
            assert(frame.wave_status != "unavailable");
            assert(frame.wave_reason == "ok");
        }
    }

    std::cout << "  PASS WaveDetector pipeline synthetic test." << std::endl;
}

void testWaveDetectorPipeline2Antennas() {
    std::cout << "[Test] WaveDetector pipeline 2-antenna fallback test (2 chains, 20 MHz)..." << std::endl;
    WaveDetector detector;
    assert(!detector.isWaving());

    uint64_t start_ts_us = 10000000ULL;
    for (size_t f = 0; f < 80; ++f) {
        CSIData frame;
        frame.timestamp_ns = (start_ts_us + f * 50000ULL) * 1000ULL;
        frame.chip_tsf_us = start_ts_us + f * 50000ULL;
        frame.num_chains = 2; // EXACTLY 2 ANTENNAS
        frame.packet_bw = 0;  // 20 MHz
        frame.chains.resize(2);

        for (size_t c = 0; c < 2; ++c) {
            frame.chains[c].amplitudes.resize(52);

            for (size_t k = 0; k < 52; ++k) {
                float amp = 100.0f + 10.0f * std::sin(k * 0.1f);

                if (f > 30 && f < 55) {
                    float wave_mod = 45.0f * std::sin((f - 30) * 0.8f + k * 0.2f);
                    amp += wave_mod;
                }

                frame.chains[c].amplitudes[k] = amp;
            }
        }

        detector.update(frame);

        if (f >= 10) {
            assert(frame.wave_status != "unavailable");
            assert(frame.wave_reason == "ok");
        }
    }

    std::cout << "  PASS WaveDetector pipeline 2-antenna test." << std::endl;
}

int main() {
    std::cout << "=== Running Original Wave Detection Unit Tests ===" << std::endl;
    testOriginalModelPredictionDirect();
    testWaveDetectorPipelineSynthetic();
    testWaveDetectorPipeline2Antennas();
    std::cout << "=== ALL TESTS PASSED SUCCESSFULLY ===" << std::endl;
    return 0;
}
