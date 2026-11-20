#pragma once
#include <algorithm>

struct ScoreConfig {
    struct Timing { double min, max, peakMin; };
    // Ngưỡng thời gian nét vẽ thực tế của bàn tay trong không gian:
    // Cả nét xuống và nét lên đều phải vẽ trọn vẹn (tối thiểu 0.20s hoặc 0.16s khi xung cực mạnh >= 0.85).
    Timing down{.20, 1.80, .16}, pause{.10, 2.00, .08}, up{.20, 2.00, .16};
    double finishMin = .40; // Giữ yên 0.40s xác nhận sau khi vẽ đủ cả 3 nét
    double drawMax = 5.5;   // Giới hạn toàn lượt vẽ 5.5 giây
};

struct ScoreWake {
    enum Phase { WAIT_READY, READY, DOWN, BOTTOM, UP, FINISH, PASS, FAIL };
    static constexpr double gapLimit = .50;
    static constexpr double resultHold = 1.0;
    static constexpr double rearmStill = .50;
    ScoreConfig config;
    Phase phase = WAIT_READY;
    double since = -1, startedAt = -1, finishedAt = -1;
    int progress = 0;
    const char* result = nullptr;
    bool accepted = false;
    double peakScore = 0.0;

    const char* state() const {
        switch (phase) {
        case WAIT_READY: return "WAIT_READY";
        case READY: return "READY";
        case DOWN: return progress >= 1 ? "DOWN_HOLD" : "DOWN";
        case BOTTOM: return progress >= 2 ? "UP_CUE" : "BOTTOM";
        case UP: return progress >= 3 ? "UP_HOLD" : "UP";
        case FINISH: return "FINISH";
        case PASS: return "PASS";
        case FAIL: return "FAIL";
        }
        return "WAIT_READY";
    }

    bool drawing() const { return phase >= DOWN && phase <= FINISH; }

    double strokeFraction(double time) const {
        double minimum;
        switch (phase) {
        case DOWN: minimum = (peakScore >= 0.85) ? config.down.peakMin : config.down.min; break;
        case BOTTOM: minimum = config.pause.min; break;
        case UP: minimum = (peakScore >= 0.85) ? config.up.peakMin : config.up.min; break;
        default: return 0;
        }
        return std::clamp((time - since) / minimum, 0.0, 1.0);
    }

    void finish(double time, const char* reason, bool ok = false) {
        accepted = ok;
        result = reason;
        phase = ok ? PASS : FAIL;
        finishedAt = time;
        since = -1;
        peakScore = 0.0;
    }

    void step(double time, int label, int expected, ScoreConfig::Timing timing,
              int mark, Phase next, const char* tooShort, const char* tooLong, double currentPeak = 0.0) {
        const double requiredMin = (currentPeak >= 0.85) ? timing.peakMin : timing.min;
        if (label != expected) {
            const double duration = time - since;
            if (progress < mark && duration + 1e-9 < requiredMin) {
                finish(time, tooShort);
                return;
            }
            progress = std::max(progress, mark);
            phase = next;
            since = time;
            peakScore = 0.0;
            return;
        }
        const double duration = time - since;
        if (duration > timing.max + 1e-9) finish(time, tooLong);
        else if (duration + 1e-9 >= requiredMin) progress = mark;
    }

    void push(double time, int label, double score = 0.0) {
        result = nullptr;
        if (drawing() && time - startedAt > config.drawMax + 1e-9) {
            finish(time, "draw_timeout");
            return;
        }
        switch (phase) {
        case WAIT_READY:
            if (label == 0) phase = READY;
            break;
        case READY:
            if (label == 1) {
                phase = DOWN;
                since = startedAt = time;
                peakScore = score;
            }
            break;
        case DOWN:
            if (label == 1) peakScore = std::max(peakScore, score);
            step(time, label, 1, config.down, 1, BOTTOM, "down_short", "down_long", peakScore);
            break;
        case BOTTOM:
            step(time, label, 0, config.pause, 2, UP, "pause_short", "pause_long", 0.0);
            if (phase == UP) peakScore = score;
            break;
        case UP:
            if (label == 1) peakScore = std::max(peakScore, score);
            step(time, label, 1, config.up, 3, FINISH, "up_short", "up_long", peakScore);
            break;
        case FINISH:
            if (label != 0) {
                finish(time, "finish_moved");
            } else if (progress < 3) {
                // Bắt buộc phải hoàn thành đủ cả 3 nét mới được xem xét PASS
                finish(time, "up_short");
            } else if (time - since + 1e-9 >= config.finishMin) {
                progress = 3;
                finish(time, "label_rhythm", true);
            }
            break;
        case PASS:
        case FAIL:
            if (label != 0) since = -1;
            else if (since < 0) since = time;
            if (since >= 0 && time - since + 1e-9 >= rearmStill &&
                time - finishedAt + 1e-9 >= resultHold) {
                phase = READY;
                progress = 0;
                since = -1;
                peakScore = 0.0;
            }
            break;
        }
    }
};
