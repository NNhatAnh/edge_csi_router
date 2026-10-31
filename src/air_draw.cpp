#include "air_draw.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

float AirDrawTracker::wrapPhase(float phase) {
    while (phase > kPi)
        phase -= 2.0f * kPi;
    while (phase < -kPi)
        phase += 2.0f * kPi;
    return phase;
}

float AirDrawTracker::wrapPhasePi(float dphi) {
    while (dphi > 0.5f * kPi)
        dphi -= kPi;
    while (dphi < -0.5f * kPi)
        dphi += kPi;
    return dphi;
}

float AirDrawTracker::computeMedian(std::vector<float> &vec) {
    if (vec.empty()) return 0.0f;
    const size_t mid = vec.size() / 2;
    std::nth_element(vec.begin(), vec.begin() + mid, vec.end());
    return vec[mid];
}

float AirDrawTracker::computeDequeMedian(const std::deque<float> &q) {
    if (q.empty()) return 0.0f;
    std::vector<float> vec(q.begin(), q.end()); 
    const size_t mid = vec.size() / 2;
    std::nth_element(vec.begin(), vec.begin() + mid, vec.end());
    return vec[mid];
}

std::string AirDrawTracker::primitiveToString(StrokePrimitive prim) {
    switch (prim) {
        case StrokePrimitive::RIGHT:      return "right";
        case StrokePrimitive::LEFT:       return "left";
        case StrokePrimitive::UP:         return "up";
        case StrokePrimitive::DOWN:       return "down";
        case StrokePrimitive::UP_RIGHT:   return "up_right";
        case StrokePrimitive::UP_LEFT:    return "up_left";
        case StrokePrimitive::DOWN_RIGHT: return "down_right";
        case StrokePrimitive::DOWN_LEFT:  return "down_left";
        case StrokePrimitive::ARC_CW:     return "arc_cw";
        case StrokePrimitive::ARC_CCW:    return "arc_ccw";
        default:                          return "none";
    }
}

AirDrawTracker::AirDrawTracker() {
    reset();
}

void AirDrawTracker::reset() {
    is_drawing_ = false;
    stroke_finished_ = false;
    cursor_x_ = 0.5f;
    cursor_y_ = 0.5f;
    target_x_ = 0.5f;
    target_y_ = 0.5f;
    vel_x_ = 0.0f;
    vel_y_ = 0.0f;
    smooth_vx_ = 0.0f;
    smooth_vy_ = 0.0f;
    current_speed_ = 0.0f;
    heading_deg_ = 0.0f;
    heading_dir_ = "none";
    locked_heading_deg_ = 0.0f;
    locked_heading_dir_ = "none";
    heading_lock_count_ = 0;
    delta_theta_ = 0.0f;
    delta_pwr_db_ = 0.0f;

    stroke_state_ = StrokeState::IDLE;
    last_primitive_ = StrokePrimitive::NONE;
    total_strokes_committed_ = 0;
    stroke_start_ts_us_ = 0;
    stroke_start_x_ = 0.5f;
    stroke_start_y_ = 0.5f;
    current_stroke_features_ = StrokeFeatures{};

    anchor_u_ = 0.0f;
    anchor_v_ = 0.0f;
    anchor_pwr_ = 0.0f;
    has_anchor_ = false;
    anchor_calib_remaining_ = 5;

    calib_u_samples_.clear();
    calib_v_samples_.clear();
    calib_pwr_samples_.clear();

    prev_centered_c0_.clear();
    prev_centered_c1_.clear();
    has_prev_centered_ = false;

    med_vx_.clear();
    med_vy_.clear();

    prev_cross_phase_ = 0.0f;
    prev_pwr_ratio_db_ = 0.0f;
    prev_diff_slope_ = 0.0f;
    prev_diff_phase_12_ = 0.0f;
    has_prev_cross_ = false;

    last_motion_ts_us_ = 0;
    prev_frame_ts_us_ = 0;
    activity_energy_ = 0;

    current_stroke_.clear();
    last_shape_ = "idle";
    shape_confidence_ = 0.0f;
}

void AirDrawTracker::centerCursor() {
    calibrateAnchor();
}

void AirDrawTracker::calibrateAnchor() {
    cursor_x_ = 0.5f;
    cursor_y_ = 0.5f;
    target_x_ = 0.5f;
    target_y_ = 0.5f;
    vel_x_ = 0.0f;
    vel_y_ = 0.0f;
    smooth_vx_ = 0.0f;
    smooth_vy_ = 0.0f;
    current_speed_ = 0.0f;
    heading_deg_ = 0.0f;
    heading_dir_ = "none";
    locked_heading_deg_ = 0.0f;
    locked_heading_dir_ = "none";
    heading_lock_count_ = 0;
    delta_theta_ = 0.0f;
    delta_pwr_db_ = 0.0f;
    activity_energy_ = 0;
    is_drawing_ = false;
    has_anchor_ = false;
    anchor_calib_remaining_ = 5;

    stroke_state_ = StrokeState::IDLE;
    last_primitive_ = StrokePrimitive::NONE;
    current_stroke_features_ = StrokeFeatures{};

    calib_u_samples_.clear();
    calib_v_samples_.clear();
    calib_pwr_samples_.clear();

    med_vx_.clear();
    med_vy_.clear();

    current_stroke_.clear();
    last_shape_ = "idle";
    shape_confidence_ = 0.0f;
}

void AirDrawTracker::update(CSIData &frame) {
    stroke_finished_ = false;

    // Kiểm tra tính hợp lệ cơ bản của dữ liệu CSI
    if (frame.chains.size() < 2 || frame.tonesPerChain() == 0) {
        frame.draw_active = is_drawing_;
        frame.draw_cursor_x = cursor_x_;
        frame.draw_cursor_y = cursor_y_;
        frame.draw_stroke_completed = stroke_finished_;
        frame.draw_shape = last_shape_;
        frame.draw_shape_confidence = shape_confidence_;
        frame.draw_delta_theta = delta_theta_;
        frame.draw_delta_pwr = delta_pwr_db_;
        frame.draw_has_anchor = has_anchor_;
        frame.draw_heading_deg = heading_deg_;
        frame.draw_heading_dir = heading_dir_;
        return;
    }

    uint64_t current_ts_us = frame.chip_tsf_us;
    if (current_ts_us == 0) {
        current_ts_us = frame.timestamp_ns / 1000ULL;
    }

    float dt_s = 0.033f;
    if (prev_frame_ts_us_ > 0 && current_ts_us > prev_frame_ts_us_) {
        dt_s = static_cast<float>(current_ts_us - prev_frame_ts_us_) / 1000000.0f;
        dt_s = std::clamp(dt_s, kMinDtSec, kMaxDtSec);
    }
    prev_frame_ts_us_ = current_ts_us;

    const size_t tone_count = frame.tonesPerChain();
    const size_t guard_start = tone_count / 10;
    const size_t guard_end = tone_count - guard_start;
    const size_t valid_count = (guard_end > guard_start) ? (guard_end - guard_start) : 0;

    if (valid_count < 8) {
        frame.draw_active = is_drawing_;
        frame.draw_cursor_x = cursor_x_;
        frame.draw_cursor_y = cursor_y_;
        frame.draw_stroke_completed = stroke_finished_;
        frame.draw_shape = last_shape_;
        frame.draw_shape_confidence = shape_confidence_;
        frame.draw_delta_theta = delta_theta_;
        frame.draw_delta_pwr = delta_pwr_db_;
        frame.draw_has_anchor = has_anchor_;
        frame.draw_heading_deg = heading_deg_;
        frame.draw_heading_dir = heading_dir_;
        return;
    }

    // -------------------------------------------------------------------------
    // 1. Common Level Centering: giảm ảnh hưởng của biến đổi gain chung.
    // -------------------------------------------------------------------------
    std::vector<float> db_c0(valid_count);
    std::vector<float> db_c1(valid_count);
    for (size_t i = 0; i < valid_count; ++i) {
        size_t t = guard_start + i;
        float a0 = (t < frame.chains[0].amplitudes.size()) ? frame.chains[0].amplitudes[t] : 0.0f;
        float a1 = (t < frame.chains[1].amplitudes.size()) ? frame.chains[1].amplitudes[t] : 0.0f;
        db_c0[i] = 20.0f * std::log10(a0 + 1.0f);
        db_c1[i] = 20.0f * std::log10(a1 + 1.0f);
    }

    std::vector<float> tmp_c0 = db_c0;
    std::vector<float> tmp_c1 = db_c1;
    const float med_c0 = computeMedian(tmp_c0);
    const float med_c1 = computeMedian(tmp_c1);

    std::vector<float> centered_c0(valid_count);
    std::vector<float> centered_c1(valid_count);
    for (size_t i = 0; i < valid_count; ++i) {
        centered_c0[i] = db_c0[i] - med_c0;
        centered_c1[i] = db_c1[i] - med_c1;
    }

    float kinetic_rate = 0.0f;
    if (has_prev_centered_ && prev_centered_c0_.size() == valid_count && prev_centered_c1_.size() == valid_count) {
        float sum_diff = 0.0f;
        for (size_t i = 0; i < valid_count; ++i) {
            sum_diff += std::abs(centered_c0[i] - prev_centered_c0_[i]);
            sum_diff += std::abs(centered_c1[i] - prev_centered_c1_[i]);
        }
        kinetic_rate = (sum_diff / static_cast<float>(valid_count * 2)) / dt_s;
    }
    prev_centered_c0_ = centered_c0;
    prev_centered_c1_ = centered_c1;
    has_prev_centered_ = true;

    // -------------------------------------------------------------------------
    // 2. Đặc trưng tương quan phức liên anten và độ dốc pha vi sai.
    // -------------------------------------------------------------------------
    float sum_re = 0.0f;
    float sum_im = 0.0f;
    float pwr0 = 0.0f;
    float pwr1 = 0.0f;

    float sum_t_sq = 0.0f;
    float sum_t_dp = 0.0f;
    float unwrapped_dp = 0.0f;
    float prev_dp = 0.0f;
    const float t_c = static_cast<float>(guard_start + guard_end) * 0.5f;

    for (size_t t = guard_start; t < guard_end; ++t) {
        if (t >= frame.chains[0].amplitudes.size() || t >= frame.chains[1].amplitudes.size() ||
            t >= frame.chains[0].phases.size() || t >= frame.chains[1].phases.size())
            break;

        float a0 = frame.chains[0].amplitudes[t];
        float a1 = frame.chains[1].amplitudes[t];
        float p0 = frame.chains[0].phases[t];
        float p1 = frame.chains[1].phases[t];
        float dp = wrapPhase(p0 - p1);

        sum_re += a0 * a1 * std::cos(dp);
        sum_im += a0 * a1 * std::sin(dp);
        pwr0 += a0 * a0;
        pwr1 += a1 * a1;

        if (t == guard_start) {
            unwrapped_dp = dp;
        } else {
            unwrapped_dp += wrapPhase(dp - prev_dp);
        }
        prev_dp = dp;

        float dt_tone = static_cast<float>(t) - t_c;
        sum_t_sq += dt_tone * dt_tone;
        sum_t_dp += dt_tone * unwrapped_dp;
    }

    const float sum_pwr = 0.5f * (pwr0 + pwr1);
    if (sum_pwr <= 1e-4f) return;

    const float diff_slope = (sum_t_sq > 1e-3f) ? (sum_t_dp / sum_t_sq) : 0.0f;
    const float current_cross_phase = std::atan2(sum_im, sum_re);
    const float pwr_ratio_db = 10.0f * std::log10((pwr0 + 1e-4f) / (pwr1 + 1e-4f));

    // Anten thứ 3 (mảng 3x3 nếu có)
    float current_cross_phase_12 = 0.0f;
    const bool has_3_chains = (frame.chains.size() >= 3 && frame.chains[2].phases.size() >= guard_end);
    if (has_3_chains) {
        float sum_re_12 = 0.0f;
        float sum_im_12 = 0.0f;
        for (size_t t = guard_start; t < guard_end; ++t) {
            float a1 = frame.chains[1].amplitudes[t];
            float a2 = frame.chains[2].amplitudes[t];
            float dp12 = wrapPhase(frame.chains[1].phases[t] - frame.chains[2].phases[t]);
            sum_re_12 += a1 * a2 * std::cos(dp12);
            sum_im_12 += a1 * a2 * std::sin(dp12);
        }
        current_cross_phase_12 = std::atan2(sum_im_12, sum_re_12);
    }

    // -------------------------------------------------------------------------
    // 3. HIỆU CHUẨN GỐC THAM CHIẾU (ANCHOR CALIBRATION)
    // -------------------------------------------------------------------------
    float u = sum_re / sum_pwr;
    float v = sum_im / sum_pwr;

    if (!has_anchor_) {
        calib_u_samples_.push_back(u);
        calib_v_samples_.push_back(v);
        calib_pwr_samples_.push_back(sum_pwr);
        if (anchor_calib_remaining_ > 0) --anchor_calib_remaining_;

        if (anchor_calib_remaining_ == 0 && calib_u_samples_.size() >= 5) {
            anchor_u_ = computeMedian(calib_u_samples_);
            anchor_v_ = computeMedian(calib_v_samples_);
            anchor_pwr_ = computeMedian(calib_pwr_samples_);
            has_anchor_ = true;
            cursor_x_ = 0.5f;
            cursor_y_ = 0.5f;
            target_x_ = 0.5f;
            target_y_ = 0.5f;
        }
    }

    if (has_anchor_) {
        float u_rel = u * anchor_u_ + v * anchor_v_;
        float v_rel = v * anchor_u_ - u * anchor_v_;
        if (u_rel < 0.0f) {
            u_rel = -u_rel;
            v_rel = -v_rel;
        }
        delta_theta_ = std::atan2(v_rel, u_rel);
        delta_pwr_db_ = 10.0f * std::log10((sum_pwr + 1e-4f) / (anchor_pwr_ + 1e-4f));
    }

    // -------------------------------------------------------------------------
    // 4. ƯỚC LƯỢNG VẬN TỐC TỨC THỜI & MODULO-PI UNWRAPPING
    // -------------------------------------------------------------------------
    float raw_vx = 0.0f;
    float raw_vy = 0.0f;

    if (has_prev_cross_) {
        // wrapPhasePi giới hạn bước pha modulo pi do BPSK carrier ambiguity.
        float d_cross_phase = wrapPhasePi(current_cross_phase - prev_cross_phase_);
        raw_vx = (d_cross_phase / dt_s) * kScaleVelX;

        float d_slope = (diff_slope - prev_diff_slope_) / dt_s;
        float d_ratio = (pwr_ratio_db - prev_pwr_ratio_db_) / dt_s;

        if (has_3_chains) {
            float d_cross_12 = wrapPhasePi(current_cross_phase_12 - prev_diff_phase_12_) / dt_s;
            raw_vy = -(d_ratio * 0.010f + d_slope * 3.0f + d_cross_12 * 0.06f) * kScaleVelY;
        } else {
            raw_vy = -(d_ratio * 0.012f + d_slope * 4.0f) * kScaleVelY;
        }
    }

    prev_cross_phase_ = current_cross_phase;
    prev_pwr_ratio_db_ = pwr_ratio_db;
    prev_diff_slope_ = diff_slope;
    prev_diff_phase_12_ = current_cross_phase_12;
    has_prev_cross_ = true;

    // Giới hạn tốc độ sinh học WiDraw (Equation 5, max 0.91 m/s = 3 ft/s)
    float measured_speed = std::sqrt(raw_vx * raw_vx + raw_vy * raw_vy);
    if (measured_speed > kMaxHandSpeed) {
        float scale = kMaxHandSpeed / measured_speed;
        raw_vx *= scale;
        raw_vy *= scale;
        measured_speed = kMaxHandSpeed;
    }

    // Lọc trung vị trượt 5 frame (~50ms)
    med_vx_.push_back(raw_vx);
    if (med_vx_.size() > kMedianWindowSize) med_vx_.pop_front();
    med_vy_.push_back(raw_vy);
    if (med_vy_.size() > kMedianWindowSize) med_vy_.pop_front();

    const float median_vx = computeDequeMedian(med_vx_);
    const float median_vy = computeDequeMedian(med_vy_);

    // Bộ lọc thông thấp vận tốc (Low-Pass Filter fc = 3.0 Hz)
    const float alpha = std::clamp(2.0f * kPi * kLpfCutoffHz * dt_s, 0.05f, 0.30f);
    smooth_vx_ = smooth_vx_ * (1.0f - alpha) + median_vx * alpha;
    smooth_vy_ = smooth_vy_ * (1.0f - alpha) + median_vy * alpha;
    current_speed_ = std::sqrt(smooth_vx_ * smooth_vx_ + smooth_vy_ * smooth_vy_);

    // -------------------------------------------------------------------------
    // 5. CỔNG CHUYỂN ĐỘNG & MÁY TRẠNG THÁI NÉT VẼ ĐỘNG HỌC (STROKE STATE MACHINE)
    // -------------------------------------------------------------------------
    bool is_macro_moving = false;
    if (frame.motion_calibrated) {
        is_macro_moving = frame.presence_detected && (frame.motion_score >= frame.motion_threshold_low);
    } else {
        is_macro_moving = (kinetic_rate > 3.0f && current_speed_ > 0.12f);
    }

    const bool has_stroke_speed = (current_speed_ >= kMotionStartSpeed);

    switch (stroke_state_) {
        case StrokeState::IDLE: {
            if (is_macro_moving && has_stroke_speed) {
                // BẮT ĐẦU NÉT VẼ MỚI (Stroke Onset)
                stroke_state_ = StrokeState::ACTIVE;
                is_drawing_ = true;
                stroke_start_ts_us_ = current_ts_us;
                stroke_start_x_ = cursor_x_;
                stroke_start_y_ = cursor_y_;

                current_stroke_features_ = StrokeFeatures{};
                current_stroke_features_.stroke_duration_s = 0.0f;
                current_stroke_features_.max_speed = current_speed_;
                current_stroke_features_.mean_energy = kinetic_rate;
                current_stroke_features_.sample_count = 1;

                last_motion_ts_us_ = current_ts_us;
                activity_energy_ = 6;
            } else {
                // ĐỨNG YÊN HOÀN TOÀN: Neo cứng con trỏ, triệt tiêu mọi vận tốc
                is_drawing_ = false;
                smooth_vx_ = 0.0f;
                smooth_vy_ = 0.0f;
                current_speed_ = 0.0f;
                heading_dir_ = "none";
                locked_heading_dir_ = "none";
                heading_lock_count_ = 0;

                // Lò xo hồi quy chậm sau 4.0s tĩnh lặng
                if (current_ts_us > last_motion_ts_us_ && (current_ts_us - last_motion_ts_us_) > 4000000ULL) {
                    cursor_x_ += (0.5f - cursor_x_) * 0.003f;
                    cursor_y_ += (0.5f - cursor_y_) * 0.003f;
                    target_x_ = cursor_x_;
                    target_y_ = cursor_y_;
                }
            }
            break;
        }

        case StrokeState::ACTIVE: {
            is_drawing_ = true;
            last_motion_ts_us_ = current_ts_us;
            activity_energy_ = std::min(activity_energy_ + 1, 10);

            const float dt_int = std::min(dt_s, kMaxIntegrationDt);
            current_stroke_features_.net_dx += smooth_vx_ * dt_int;
            current_stroke_features_.net_dy += smooth_vy_ * dt_int;
            current_stroke_features_.path_length += current_speed_ * dt_int;
            current_stroke_features_.stroke_duration_s += dt_s;
            current_stroke_features_.sample_count++;
            current_stroke_features_.max_speed = std::max(current_stroke_features_.max_speed, current_speed_);
            current_stroke_features_.mean_energy += kinetic_rate;

            // Cập nhật góc hướng tức thời trong khi vẽ
            float raw_heading_rad = std::atan2(smooth_vy_, smooth_vx_);
            heading_deg_ = raw_heading_rad * (180.0f / kPi);

            auto getDirFromDeg = [](float deg) -> std::string {
                if (deg >= -22.5f && deg < 22.5f) return "right";
                if (deg >= 22.5f && deg < 67.5f) return "down_right";
                if (deg >= 67.5f && deg < 112.5f) return "down";
                if (deg >= 112.5f && deg < 157.5f) return "down_left";
                if (deg >= 157.5f || deg < -157.5f) return "left";
                if (deg >= -157.5f && deg < -112.5f) return "up_left";
                if (deg >= -112.5f && deg < -67.5f) return "up";
                return "up_right";
            };
            heading_dir_ = getDirFromDeg(heading_deg_);

            // Di chuyển con trỏ nhẹ nhàng theo hướng vung tay để người dùng có phản hồi trực quan
            cursor_x_ = std::clamp(cursor_x_ + smooth_vx_ * dt_int * 0.5f, 0.04f, 0.96f);
            cursor_y_ = std::clamp(cursor_y_ + smooth_vy_ * dt_int * 0.5f, 0.04f, 0.96f);

            // Kiểm tra điều kiện HOÀN TẤT NÉT VẼ (Stroke End / Deceleration):
            // 1. Tốc độ giảm xuống dưới ngưỡng kStrokeEndSpeed
            // 2. Hoặc thời lượng nét vẽ đã đạt tối đa (1.0s)
            // 3. Hoặc người dùng dừng cử động (is_macro_moving = false)
            const bool speed_dropped = (current_speed_ < kStrokeEndSpeed);
            const bool timeout = (current_stroke_features_.stroke_duration_s >= kMaxStrokeDurationSec);
            const bool motion_ceased = (!is_macro_moving);

            if (current_stroke_features_.stroke_duration_s >= kMinStrokeDurationSec && (speed_dropped || timeout || motion_ceased)) {
                // KÍCH HOẠT PHÂN LOẠI & KHÓA VECTOR HÌNH HỌC (Stroke Commit)
                float confidence = 0.0f;
                if (current_stroke_features_.sample_count > 0) {
                    current_stroke_features_.mean_energy /= static_cast<float>(current_stroke_features_.sample_count);
                }

                StrokePrimitive detected_primitive = classifyStroke(current_stroke_features_, confidence);
                if (detected_primitive != StrokePrimitive::NONE) {
                    commitStroke(detected_primitive, confidence, current_stroke_features_.max_speed);
                }

                stroke_state_ = StrokeState::IDLE;
                is_drawing_ = false;
            }
            break;
        }

        default: {
            stroke_state_ = StrokeState::IDLE;
            break;
        }
    }

    // Gán dữ liệu telemetry lên frame CSIData
    frame.draw_active = is_drawing_;
    frame.draw_cursor_x = cursor_x_;
    frame.draw_cursor_y = cursor_y_;
    frame.draw_stroke_completed = stroke_finished_;
    frame.draw_shape = last_shape_;
    frame.draw_shape_confidence = shape_confidence_;
    frame.draw_delta_theta = delta_theta_;
    frame.draw_delta_pwr = delta_pwr_db_;
    frame.draw_has_anchor = has_anchor_;
    frame.draw_heading_deg = heading_deg_;
    frame.draw_heading_dir = heading_dir_;
}

// -----------------------------------------------------------------------------
// 6. Heuristic phân loại hướng nét bằng đặc trưng động học.
// -----------------------------------------------------------------------------
StrokePrimitive AirDrawTracker::classifyStroke(const StrokeFeatures &feats, float &confidence) {
    const float net_mag = std::sqrt(feats.net_dx * feats.net_dx + feats.net_dy * feats.net_dy);
    const float direction_coherence =
        net_mag / std::max(feats.path_length, net_mag);
    if (net_mag < kMinStrokeDisplacement ||
        feats.max_speed < kMotionStartSpeed ||
        direction_coherence < kMinDirectionCoherence) {
        confidence = 0.0f;
        return StrokePrimitive::NONE;
    }

    const float angle_rad = std::atan2(feats.net_dy, feats.net_dx);
    float angle_deg = angle_rad * (180.0f / kPi);

    const float speed_quality = std::clamp(feats.max_speed / 0.35f, 0.0f, 1.0f);
    const float displacement_quality =
        std::clamp(net_mag / (2.0f * kMinStrokeDisplacement), 0.0f, 1.0f);
    confidence = std::clamp(direction_coherence *
                                (0.5f + 0.25f * speed_quality +
                                 0.25f * displacement_quality),
                            0.0f, 0.98f);

    // Phân loại 8 hướng hình học cơ bản
    if (angle_deg >= -22.5f && angle_deg < 22.5f) {
        return StrokePrimitive::RIGHT;
    } else if (angle_deg >= 22.5f && angle_deg < 67.5f) {
        return StrokePrimitive::DOWN_RIGHT;
    } else if (angle_deg >= 67.5f && angle_deg < 112.5f) {
        return StrokePrimitive::DOWN;
    } else if (angle_deg >= 112.5f && angle_deg < 157.5f) {
        return StrokePrimitive::DOWN_LEFT;
    } else if (angle_deg >= 157.5f || angle_deg < -157.5f) {
        return StrokePrimitive::LEFT;
    } else if (angle_deg >= -157.5f && angle_deg < -112.5f) {
        return StrokePrimitive::UP_LEFT;
    } else if (angle_deg >= -112.5f && angle_deg < -67.5f) {
        return StrokePrimitive::UP;
    } else {
        return StrokePrimitive::UP_RIGHT;
    }
}

// -----------------------------------------------------------------------------
// 7. BỘ TẠO VECTOR HÌNH HỌC CHUẨN XÁC (VECTOR STROKE ASSEMBLER)
// -----------------------------------------------------------------------------
void AirDrawTracker::commitStroke(StrokePrimitive primitive, float confidence, float intensity) {
    last_primitive_ = primitive;
    total_strokes_committed_++;
    stroke_finished_ = true;

    // Chiều dài nét vẽ chuẩn tỉ lệ với tốc độ vung tay (tối thiểu 0.08, tối đa 0.20 đơn vị Canvas)
    const float stroke_len = std::clamp(0.08f + intensity * 0.12f, 0.08f, 0.20f);
    constexpr float kInvSqrt2 = 0.70710678f;

    float dx = 0.0f;
    float dy = 0.0f;

    switch (primitive) {
        case StrokePrimitive::RIGHT:
            dx = stroke_len;
            dy = 0.0f;
            break;
        case StrokePrimitive::LEFT:
            dx = -stroke_len;
            dy = 0.0f;
            break;
        case StrokePrimitive::UP:
            dx = 0.0f;
            dy = -stroke_len; // Hướng lên trên màn hình (Y giảm)
            break;
        case StrokePrimitive::DOWN:
            dx = 0.0f;
            dy = stroke_len;  // Hướng xuống dưới màn hình (Y tăng)
            break;
        case StrokePrimitive::UP_RIGHT:
            dx = stroke_len * kInvSqrt2;
            dy = -stroke_len * kInvSqrt2;
            break;
        case StrokePrimitive::UP_LEFT:
            dx = -stroke_len * kInvSqrt2;
            dy = -stroke_len * kInvSqrt2;
            break;
        case StrokePrimitive::DOWN_RIGHT:
            dx = stroke_len * kInvSqrt2;
            dy = stroke_len * kInvSqrt2;
            break;
        case StrokePrimitive::DOWN_LEFT:
            dx = -stroke_len * kInvSqrt2;
            dy = stroke_len * kInvSqrt2;
            break;
        default:
            break;
    }

    // Tọa độ điểm đích thẳng tắp, không răng cưa
    target_x_ = std::clamp(stroke_start_x_ + dx, 0.04f, 0.96f);
    target_y_ = std::clamp(stroke_start_y_ + dy, 0.04f, 0.96f);

    // Lưu điểm đỉnh (Vertex A và Vertex B) vào chuỗi nét vẽ
    AirPoint pt_start;
    pt_start.x = stroke_start_x_;
    pt_start.y = stroke_start_y_;
    pt_start.timestamp_us = stroke_start_ts_us_;
    pt_start.is_stroke_start = true;

    AirPoint pt_end;
    pt_end.x = target_x_;
    pt_end.y = target_y_;
    pt_end.timestamp_us = last_motion_ts_us_;
    pt_end.is_stroke_end = true;

    if (current_stroke_.size() + 2 > kMaxStrokePoints) {
        current_stroke_.erase(current_stroke_.begin(), current_stroke_.begin() + 2);
    }
    current_stroke_.push_back(pt_start);
    current_stroke_.push_back(pt_end);

    cursor_x_ = target_x_;
    cursor_y_ = target_y_;

    last_shape_ = primitiveToString(primitive);
    shape_confidence_ = confidence;
}
