#ifndef WAKEUP_DETECTOR_HPP
#define WAKEUP_DETECTOR_HPP

#include "csi_parser.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <deque>
#include <string>
#include <vector>

/**
 * ScoreConfig - Thông số luật thời gian cố định chuẩn Model Wake 8082 V6
 */
struct ScoreConfig {
    struct Timing {
        double min, max;
    };
    Timing down{0.15, 1.50};
    Timing pause{0.15, 2.00};
    Timing up{0.15, 2.00};
    double finishMin = 0.50;
    double drawMax = 5.0;
};

/**
 * ScoreWake - Cỗ máy trạng thái nhịp điệu (Rhythm State Machine) chính thức của V6
 */
struct ScoreWake {
    enum Phase { WAIT_READY, READY, DOWN, BOTTOM, UP, FINISH, PASS, FAIL };
    static constexpr double gapLimit = 0.50;
    static constexpr double resultHold = 1.0;
    static constexpr double rearmStill = 0.50;

    ScoreConfig config;
    Phase phase = WAIT_READY;
    double since = -1, startedAt = -1, finishedAt = -1;
    int progress = 0;
    const char *result = nullptr;
    bool accepted = false;

    const char *state() const {
        switch (phase) {
        case WAIT_READY:
            return "WAIT_READY";
        case READY:
            return "READY";
        case DOWN:
            return progress >= 1 ? "DOWN_HOLD" : "DOWN";
        case BOTTOM:
            return progress >= 2 ? "UP_CUE" : "BOTTOM";
        case UP:
            return progress >= 3 ? "UP_HOLD" : "UP";
        case FINISH:
            return "FINISH";
        case PASS:
            return "PASS";
        case FAIL:
            return "FAIL";
        }
        return "WAIT_READY";
    }

    bool drawing() const { return phase >= DOWN && phase <= FINISH; }

    double strokeFraction(double time) const {
        double minimum;
        switch (phase) {
        case DOWN:
            minimum = config.down.min;
            break;
        case BOTTOM:
            minimum = config.pause.min;
            break;
        case UP:
            minimum = config.up.min;
            break;
        default:
            return 0;
        }
        if (since < 0) return 0;
        return std::clamp((time - since) / minimum, 0.0, 1.0);
    }

    void finish(double time, const char *reason, bool ok = false) {
        accepted = ok;
        result = reason;
        phase = ok ? PASS : FAIL;
        finishedAt = time;
        since = -1;
    }

    void step(double time, int label, int expected, ScoreConfig::Timing timing,
              int mark, Phase next, const char *tooShort, const char *tooLong) {
        if (label != expected) {
            if (progress < mark) {
                finish(time, tooShort);
                return;
            }
            phase = next;
            since = time;
            return;
        }
        const double duration = time - since;
        if (duration > timing.max + 1e-9)
            finish(time, tooLong);
        else if (duration + 1e-9 >= timing.min)
            progress = mark;
    }

    void push(double time, int label) {
        result = nullptr;
        if (drawing() && time - startedAt > config.drawMax + 1e-9) {
            finish(time, "draw_timeout");
            return;
        }
        switch (phase) {
        case WAIT_READY:
            if (label == 0)
                phase = READY;
            break;
        case READY:
            if (label == 1) {
                phase = DOWN;
                since = startedAt = time;
                progress = 0;
            }
            break;
        case DOWN:
            step(time, label, 1, config.down, 1, BOTTOM, "down_short", "down_long");
            break;
        case BOTTOM:
            step(time, label, 0, config.pause, 2, UP, "pause_short", "pause_long");
            break;
        case UP:
            step(time, label, 1, config.up, 3, FINISH, "up_short", "up_long");
            if (phase == FINISH && config.finishMin == 0)
                finish(time, "label_rhythm", true);
            break;
        case FINISH:
            if (label != 0)
                finish(time, "finish_moved");
            else if (time - since + 1e-9 >= config.finishMin)
                finish(time, "label_rhythm", true);
            break;
        case PASS:
        case FAIL:
            if (label != 0)
                since = -1;
            else if (since < 0)
                since = time;
            if (since >= 0 && time - since + 1e-9 >= rearmStill &&
                time - finishedAt + 1e-9 >= resultHold) {
                phase = READY;
                progress = 0;
                since = -1;
            }
            break;
        }
    }
};

/**
 * ScoreSession - Quản lý phiên nhịp và tiến trình quan sát thời gian
 */
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

    void push(double source, double received, int label, bool valid) {
        if (previous >= 0 && (source <= previous || source - previous > gapTimeout))
            gap();
        if (valid && !missing && previous >= 0)
            observedTime += source - previous;
        previous = source;
        lastReceived = received;
        ++sequence;
        if (!valid) {
            gap();
            return;
        }
        missing = false;
        wake.push(observedTime, label);
        if (wake.result) {
            resultReason = wake.result;
            if (wake.accepted)
                ++accepted;
            else
                ++rejected;
        }
    }

    void clear() {
        resultReason.clear();
        accepted = rejected = 0;
    }
};

/**
 * WakeupDetector - Động cơ nhận diện cử chỉ Model Wake 8082 V6 (M5R) duy nhất
 */
class WakeupDetector {
  public:
    explicit WakeupDetector(const std::string &models_dir = "models");

    void update(CSIData &frame);
    void reset(const std::string &reason = "manual_reset");

    std::string getActiveModel() const { return "M5R_V6"; }
    std::vector<std::string> getAvailableModels() const { return {"M5R_V6"}; }

    std::string getState() const;
    std::string getReason() const;
    std::string getEvent() const;
    float getScore() const;
    int getLabel() const;
    bool isTriggered() const;
    float getProgress() const;
    float getStrokeFraction() const;
    int getAcceptedCount() const;
    int getRejectedCount() const;

    float getThreshold() const { return 0.70f; }
    int getStep() const { return 10; }

  private:
    ScoreSession session_;
    ScoreConfig config_;

    struct CSIFrameSample {
        double timestamp_s;
        std::vector<float> amp_c0;
        std::vector<float> amp_c1;
        std::vector<std::complex<float>> tone_c0;
        std::vector<std::complex<float>> tone_c1;
    };
    std::deque<CSIFrameSample> history_;
    static constexpr size_t kHistoryCapacity = 40;

    float last_score_ = 0.0f;
    int last_label_ = 0;
    double last_time_s_ = 0.0;
    double motion_start_time_ = -1.0;

    float computeM5RScore(const CSIData &frame);
};

#endif // WAKEUP_DETECTOR_HPP
