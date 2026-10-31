#include "air_draw.hpp"
#include "csi_parser.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

void testStrokeRightClassification() {
    std::cout << "[Test 1] Synthetic right-stroke heuristic..." << std::endl;
    AirDrawTracker tracker;

    uint64_t ts_us = 10000000ULL;

    // Calibrate anchor first (first 6 frames static)
    for (size_t f = 0; f < 6; ++f) {
        CSIData frame;
        frame.chip_tsf_us = ts_us + f * 50000ULL;
        frame.timestamp_ns = frame.chip_tsf_us * 1000ULL;
        frame.motion_calibrated = true;
        frame.presence_detected = false;
        frame.motion_score = 0.01f;
        frame.motion_threshold_low = 0.05f;
        frame.chains.resize(2);
        for (size_t c = 0; c < 2; ++c) {
            frame.chains[c].amplitudes.assign(208, 1000.0f);
            frame.chains[c].phases.assign(208, 0.0f);
        }
        tracker.update(frame);
    }
    assert(tracker.hasAnchor());

    // Simulate deliberate Right sweep (clockwise phase rotation dphi/dt > 0)
    // 8 frames at 25ms = 200ms active stroke
    for (size_t f = 6; f < 14; ++f) {
        CSIData frame;
        frame.chip_tsf_us = ts_us + f * 25000ULL;
        frame.timestamp_ns = frame.chip_tsf_us * 1000ULL;
        frame.motion_calibrated = true;
        frame.presence_detected = true;
        frame.motion_score = 0.30f;
        frame.motion_threshold_low = 0.05f;
        frame.chains.resize(2);

        float phase_spin = static_cast<float>(f - 6) * 0.20f;
        for (size_t c = 0; c < 2; ++c) {
            frame.chains[c].amplitudes.assign(208, 1000.0f);
            frame.chains[c].phases.assign(208, (c == 0) ? phase_spin : 0.0f);
        }
        tracker.update(frame);
    }

    // Now user stops sweeping (deceleration to end stroke)
    for (size_t f = 14; f < 20; ++f) {
        CSIData frame;
        frame.chip_tsf_us = ts_us + f * 25000ULL;
        frame.timestamp_ns = frame.chip_tsf_us * 1000ULL;
        frame.motion_calibrated = true;
        frame.presence_detected = false;
        frame.motion_score = 0.02f;
        frame.motion_threshold_low = 0.05f;
        frame.chains.resize(2);

        float final_phase = static_cast<float>(14 - 6) * 0.20f;
        for (size_t c = 0; c < 2; ++c) {
            frame.chains[c].amplitudes.assign(208, 1000.0f);
            frame.chains[c].phases.assign(208, (c == 0) ? final_phase : 0.0f);
        }
        tracker.update(frame);
    }

    assert(tracker.getLastPrimitive() == StrokePrimitive::RIGHT);
    assert(tracker.getCursorX() > 0.55f);
    std::cout << "  PASS: Stroke RIGHT committed successfully! Final cursor X: " 
              << tracker.getCursorX() << " (Initial: 0.5, Primitive: " 
              << AirDrawTracker::primitiveToString(tracker.getLastPrimitive()) << ")" << std::endl;
}

void testStrokeLeftClassification() {
    std::cout << "[Test 2] Synthetic left-stroke heuristic..." << std::endl;
    AirDrawTracker tracker;

    uint64_t ts_us = 20000000ULL;

    for (size_t f = 0; f < 6; ++f) {
        CSIData frame;
        frame.chip_tsf_us = ts_us + f * 50000ULL;
        frame.timestamp_ns = frame.chip_tsf_us * 1000ULL;
        frame.motion_calibrated = true;
        frame.presence_detected = false;
        frame.motion_score = 0.01f;
        frame.motion_threshold_low = 0.05f;
        frame.chains.resize(2);
        for (size_t c = 0; c < 2; ++c) {
            frame.chains[c].amplitudes.assign(208, 1000.0f);
            frame.chains[c].phases.assign(208, 0.0f);
        }
        tracker.update(frame);
    }

    // Simulate deliberate Left sweep (counter-clockwise phase rotation dphi/dt < 0)
    for (size_t f = 6; f < 14; ++f) {
        CSIData frame;
        frame.chip_tsf_us = ts_us + f * 25000ULL;
        frame.timestamp_ns = frame.chip_tsf_us * 1000ULL;
        frame.motion_calibrated = true;
        frame.presence_detected = true;
        frame.motion_score = 0.30f;
        frame.motion_threshold_low = 0.05f;
        frame.chains.resize(2);

        float phase_spin = -static_cast<float>(f - 6) * 0.20f;
        for (size_t c = 0; c < 2; ++c) {
            frame.chains[c].amplitudes.assign(208, 1000.0f);
            frame.chains[c].phases.assign(208, (c == 0) ? phase_spin : 0.0f);
        }
        tracker.update(frame);
    }

    // Stop sweeping
    for (size_t f = 14; f < 20; ++f) {
        CSIData frame;
        frame.chip_tsf_us = ts_us + f * 25000ULL;
        frame.timestamp_ns = frame.chip_tsf_us * 1000ULL;
        frame.motion_calibrated = true;
        frame.presence_detected = false;
        frame.motion_score = 0.02f;
        frame.motion_threshold_low = 0.05f;
        frame.chains.resize(2);

        float final_phase = -static_cast<float>(14 - 6) * 0.20f;
        for (size_t c = 0; c < 2; ++c) {
            frame.chains[c].amplitudes.assign(208, 1000.0f);
            frame.chains[c].phases.assign(208, (c == 0) ? final_phase : 0.0f);
        }
        tracker.update(frame);
    }

    assert(tracker.getLastPrimitive() == StrokePrimitive::LEFT);
    assert(tracker.getCursorX() < 0.45f);
    std::cout << "  PASS: Stroke LEFT committed successfully! Final cursor X: " 
              << tracker.getCursorX() << " (Initial: 0.5, Primitive: " 
              << AirDrawTracker::primitiveToString(tracker.getLastPrimitive()) << ")" << std::endl;
}

void testRejectReversingStroke() {
    std::cout << "[Test 3] Reject Reversing, Low-Coherence Stroke..." << std::endl;
    AirDrawTracker tracker;
    uint64_t ts_us = 25000000ULL;

    for (size_t f = 0; f < 6; ++f) {
        CSIData frame;
        frame.chip_tsf_us = ts_us + f * 25000ULL;
        frame.timestamp_ns = frame.chip_tsf_us * 1000ULL;
        frame.motion_calibrated = true;
        frame.motion_score = 0.01f;
        frame.motion_threshold_low = 0.05f;
        frame.chains.resize(2);
        for (auto &chain : frame.chains) {
            chain.amplitudes.assign(208, 1000.0f);
            chain.phases.assign(208, 0.0f);
        }
        tracker.update(frame);
    }

    for (size_t f = 6; f < 22; ++f) {
        const size_t step = (f < 14) ? f - 6 : 22 - f;
        const float phase_spin = static_cast<float>(step) * 0.20f;
        CSIData frame;
        frame.chip_tsf_us = ts_us + f * 25000ULL;
        frame.timestamp_ns = frame.chip_tsf_us * 1000ULL;
        frame.motion_calibrated = true;
        frame.presence_detected = true;
        frame.motion_score = 0.30f;
        frame.motion_threshold_low = 0.05f;
        frame.chains.resize(2);
        for (size_t c = 0; c < 2; ++c) {
            frame.chains[c].amplitudes.assign(208, 1000.0f);
            frame.chains[c].phases.assign(208, c == 0 ? phase_spin : 0.0f);
        }
        tracker.update(frame);
    }

    for (size_t f = 22; f < 28; ++f) {
        CSIData frame;
        frame.chip_tsf_us = ts_us + f * 25000ULL;
        frame.timestamp_ns = frame.chip_tsf_us * 1000ULL;
        frame.motion_calibrated = true;
        frame.motion_score = 0.01f;
        frame.motion_threshold_low = 0.05f;
        frame.chains.resize(2);
        for (size_t c = 0; c < 2; ++c) {
            frame.chains[c].amplitudes.assign(208, 1000.0f);
            frame.chains[c].phases.assign(208, 0.0f);
        }
        tracker.update(frame);
    }

    assert(tracker.getStrokeCount() == 0);
    assert(tracker.getLastPrimitive() == StrokePrimitive::NONE);
    std::cout << "  PASS: Reversing motion was rejected instead of guessed as a direction." << std::endl;
}

void testModuloPiUnwrapping() {
    std::cout << "[Test 4] ath12k BPSK 180-degree Polarity Flip Immunity..." << std::endl;
    AirDrawTracker tracker;

    uint64_t ts_us = 30000000ULL;
    for (size_t f = 0; f < 6; ++f) {
        CSIData frame;
        frame.chip_tsf_us = ts_us + f * 10000ULL;
        frame.timestamp_ns = frame.chip_tsf_us * 1000ULL;
        frame.motion_calibrated = true;
        frame.presence_detected = false;
        frame.motion_score = 0.01f;
        frame.motion_threshold_low = 0.05f;
        frame.chains.resize(2);
        for (size_t c = 0; c < 2; ++c) {
            frame.chains[c].amplitudes.assign(208, 1000.0f);
            frame.chains[c].phases.assign(208, 0.0f);
        }
        tracker.update(frame);
    }

    // Alternate phase between +2.1 rad and -0.8 rad while idle
    for (size_t f = 6; f < 30; ++f) {
        CSIData frame;
        frame.chip_tsf_us = ts_us + f * 10000ULL;
        frame.timestamp_ns = frame.chip_tsf_us * 1000ULL;
        frame.motion_calibrated = true;
        frame.presence_detected = false; // Still idle!
        frame.motion_score = 0.02f;
        frame.motion_threshold_low = 0.05f;
        frame.chains.resize(2);

        float p0 = (f % 2 == 0) ? 2.10f : -0.80f;
        for (size_t c = 0; c < 2; ++c) {
            frame.chains[c].amplitudes.assign(208, 1000.0f);
            frame.chains[c].phases.assign(208, (c == 0) ? p0 : 0.0f);
        }
        tracker.update(frame);
    }

    // No false strokes must be committed due to BPSK carrier flips!
    assert(tracker.getStrokeCount() == 0);
    assert(std::abs(tracker.getCursorX() - 0.5f) < 0.001f);
    assert(std::abs(tracker.getCursorY() - 0.5f) < 0.001f);
    std::cout << "  PASS: Zero false strokes triggered during 24 alternating polarity flips." << std::endl;
}

void testIdleZeroDriftLock() {
    std::cout << "[Test 5] Idle Motion Gating & Zero-Drift Deadband..." << std::endl;
    AirDrawTracker tracker;

    uint64_t ts_us = 40000000ULL;
    // 60 frames = 3.0 seconds of idle signal
    for (size_t f = 0; f < 60; ++f) {
        CSIData frame;
        frame.chip_tsf_us = ts_us + f * 50000ULL;
        frame.timestamp_ns = frame.chip_tsf_us * 1000ULL;
        frame.motion_calibrated = true;
        frame.presence_detected = false;
        frame.motion_score = 0.02f;
        frame.motion_threshold_low = 0.05f;
        frame.chains.resize(2);
        for (size_t c = 0; c < 2; ++c) {
            frame.chains[c].amplitudes.resize(208);
            frame.chains[c].phases.resize(208);
            for (size_t k = 0; k < 208; ++k) {
                float noise = std::sin(static_cast<float>(f + k)) * 0.5f;
                frame.chains[c].amplitudes[k] = 100.0f + noise;
                frame.chains[c].phases[k] = noise * 0.01f;
            }
        }
        tracker.update(frame);

        if (f > 10) {
            assert(!tracker.isDrawing());
            assert(tracker.getSpeed() == 0.0f);
        }
    }

    assert(tracker.getStrokeCount() == 0);
    assert(std::abs(tracker.getCursorX() - 0.5f) < 0.001f);
    assert(std::abs(tracker.getCursorY() - 0.5f) < 0.001f);
    std::cout << "  PASS: Zero-drift maintained over 3.0s idle period. (Cursor: "
              << tracker.getCursorX() << ", " << tracker.getCursorY() << ")" << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << " Running synthetic Air Draw heuristic tests" << std::endl;
    std::cout << "==========================================================" << std::endl;

    testStrokeRightClassification();
    testStrokeLeftClassification();
    testRejectReversingStroke();
    testModuloPiUnwrapping();
    testIdleZeroDriftLock();

    std::cout << "==========================================================" << std::endl;
    std::cout << " ALL SYNTHETIC AIR DRAW HEURISTIC TESTS PASSED!             " << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
