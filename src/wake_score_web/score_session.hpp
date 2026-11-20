#pragma once
#include <string>
#include "score_wake.hpp"

struct ScoreSession {
    ScoreWake wake;
    std::string resultReason;
    int session = 0, sequence = 0, accepted = 0, rejected = 0;
    double previous = -1, lastReceived = -1, gapTimeout = ScoreWake::gapLimit;
    double observedTime = 0;
    bool missing = true;

    void start(ScoreConfig config) {
        ++session;
        wake = ScoreWake{};
        wake.config = config;
        resultReason.clear();
        previous = lastReceived = -1;
        observedTime = 0;
        gapTimeout = ScoreWake::gapLimit;
        missing = true;
    }

    void gap() { missing = true; }

    void push(double source, double received, int label, bool valid, double score = 0.0) {
        if (previous >= 0 && (source <= previous || source - previous > gapTimeout))
            gap();
        // Advance the gesture clock only between consecutive valid scores.
        if (valid && !missing && previous >= 0) observedTime += source - previous;
        previous = source;
        lastReceived = received;
        ++sequence;
        if (!valid) {
            gap();
            return;
        }
        missing = false;
        wake.push(observedTime, label, score);
        if (wake.result) {
            resultReason = wake.result;
            if (wake.accepted) ++accepted;
            else ++rejected;
        }
    }

    void clear() {
        resultReason.clear();
        accepted = rejected = 0;
    }
};
