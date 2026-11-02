#include "distance_estimator.hpp"

#include <cmath>
#include <iostream>

namespace {
bool check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}
} // namespace

int main() {
    bool passed = true;
    DistanceEstimator estimator;

    passed &= check(std::fabs(estimator.getFrequencyMHz() - 5200.0f) < 0.01f,
                    "default frequency is 5200 MHz");
    passed &= check(std::fabs(estimator.getReferencePathLoss1m() - 46.77f) < 0.1f,
                    "5.2 GHz free-space path loss at 1 m");

    estimator.setBand(WifiBand::Band24GHz);
    passed &= check(std::fabs(estimator.getFrequencyMHz() - 2442.0f) < 0.01f,
                    "2.4 GHz band selects its default frequency");
    passed &= check(std::fabs(estimator.getReferencePathLoss1m() - 40.21f) < 0.1f,
                    "2.4 GHz free-space path loss at 1 m");

    estimator.setFrequencyMHz(5745.0f);
    passed &= check(estimator.getBand() == WifiBand::Band52GHz &&
                        std::fabs(estimator.getFrequencyMHz() - 5745.0f) < 0.01f,
                    "explicit frequency updates the band");
    estimator.setFrequencyMHz(8000.0f);
    passed &= check(std::fabs(estimator.getFrequencyMHz() - 5745.0f) < 0.01f,
                    "out-of-range frequency is ignored");

    estimator.setBand(WifiBand::Band52GHz);
    CSIData frame;
    frame.rssi_comb_db = -45;
    frame.chip_tsf_us = 1000000;
    frame.chains.emplace_back();
    frame.chains[0].amplitudes = {1.0f, 2.0f, 3.0f, 4.0f};
    estimator.process(frame);
    passed &= check(std::isfinite(frame.estimated_distance_m) &&
                        frame.estimated_distance_m >= 0.15f &&
                        frame.estimated_distance_m <= 12.0f,
                    "distance estimate is finite and within configured limits");
    passed &= check(frame.distance_zone == "near",
                    "RSSI -45 dBm at 5.2 GHz is classified as near");
    passed &= check(frame.distance_confidence >= 0.1f &&
                        frame.distance_confidence <= 0.98f,
                    "distance confidence is within configured limits");

    if (!passed)
        return 1;
    std::cout << "Distance estimator checks passed\n";
    return 0;
}
