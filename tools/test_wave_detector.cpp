#include "csi_parser.hpp"
#include "wave_detector.hpp"
#include "csi_rf_model1.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

void testVNPTModelPredictionDirect() {
    std::cout << "[Test] Direct VNPT Model prediction test (9 features)..." << std::endl;
    // motion_core features: std_c0, step_c0, std_c1, step_c1, std_c2, step_c2, c0_log, c1_log, c2_log
    float test_input[9] = {0.22f, 0.08f, 0.15f, 0.06f, 0.13f, 0.06f, 3.6f, 2.8f, 2.5f};
    double proba[2] = {0.0, 0.0};
    int pred = csi_rf1_predict(test_input, proba);
    std::cout << "  VNPT: P(no_wave) = " << proba[0] << ", P(wave) = " << proba[1] << ", pred = " << pred << std::endl;
    assert(proba[0] >= 0.0 && proba[0] <= 1.0);
    assert(proba[1] >= 0.0 && proba[1] <= 1.0);
    assert(std::abs(proba[0] + proba[1] - 1.0) < 1e-4);
    std::cout << "  PASS VNPT Model direct prediction." << std::endl;
}

void testWaveDetectorPipelineSynthetic() {
    std::cout << "[Test] WaveDetector pipeline synthetic test..." << std::endl;
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
                float base = 100.0f + 10.0f * std::sin(k * 0.1f);
                // Introduce synthetic disturbance after frame 30 to simulate hand movement
                if (f > 30 && f < 50) {
                    base += 30.0f * std::sin((f - 30) * 0.8f + k * 0.2f);
                }
                frame.chains[c].amplitudes[k] = base;
            }
        }

        detector.update(frame);

        if (f < 16) {
            assert(frame.wave_status == "unavailable");
            assert(frame.wave_reason == "too_few_samples");
        } else {
            assert(frame.wave_samples_in_window >= 16);
            assert(frame.wave_reason == "ok");
        }
    }

    std::cout << "  PASS WaveDetector pipeline synthetic test." << std::endl;
}

int main() {
    std::cout << "=== Running VNPT Wave Detection Unit Tests ===" << std::endl;
    testVNPTModelPredictionDirect();
    testWaveDetectorPipelineSynthetic();
    std::cout << "=== ALL TESTS PASSED SUCCESSFULLY ===" << std::endl;
    return 0;
}
