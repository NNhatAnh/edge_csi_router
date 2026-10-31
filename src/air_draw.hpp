#ifndef AIR_DRAW_HPP
#define AIR_DRAW_HPP

#include "csi_parser.hpp"
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

/**
 * Các lớp hướng nét của prototype hiện tại. Classifier đang dùng luật hình học,
 * chưa nhận diện cung tròn hoặc hình vẽ khép kín.
 */
enum class StrokePrimitive {
    NONE = 0,
    RIGHT,       // Sang Phải (➔)
    LEFT,        // Sang Trái (⬅)
    UP,          // Tiến Lên (⬆)
    DOWN,        // Lùi Xuống (⬇)
    UP_RIGHT,    // Chéo Lên-Phải (↗)
    UP_LEFT,     // Chéo Lên-Trái (↖)
    DOWN_RIGHT,  // Chéo Xuống-Phải (↘)
    DOWN_LEFT,   // Chéo Xuống-Trái (↙)
    ARC_CW,      // Vòng Cung Thuận Chiều Kim Đồng Hồ (↻)
    ARC_CCW      // Vòng Cung Ngược Chiều Kim Đồng Hồ (↺)
};

/**
 * Trạng thái của Máy Trạng Thái Nét Vẽ (Kinematic Stroke State Machine).
 */
enum class StrokeState {
    IDLE = 0,   // Không tích phân vận tốc khi detector không báo chuyển động
    ONSET,      // Bắt đầu phát hiện cử động vung tay
    ACTIVE,     // Đang tích lũy đặc trưng cho nét
    COMMITTED   // Trạng thái dành cho bước commit nét
};

struct AirPoint {
    float x = 0.5f;
    float y = 0.5f;
    uint64_t timestamp_us = 0;
    bool is_stroke_start = false;
    bool is_stroke_end = false;
};

/**
 * Bộ đặc trưng động học tích lũy trong một nét vẽ.
 */
struct StrokeFeatures {
    float net_dx = 0.0f;
    float net_dy = 0.0f;
    float path_length = 0.0f;
    float mean_energy = 0.0f;
    float max_speed = 0.0f;
    float stroke_duration_s = 0.0f;
    float curvature = 0.0f;
    float pwr_divergence = 0.0f;
    float diff_slope_shift = 0.0f;
    size_t sample_count = 0;
};

/**
 * Prototype trích xuất đặc trưng CSI và phân loại hướng nét bằng heuristic.
 * Đây không phải mô hình đã được xác nhận cho nhận diện hình tròn/tam giác,
 * cũng không tái tạo quỹ đạo 2D vật lý.
 */
class AirDrawTracker {
  public:
    AirDrawTracker();

    void update(CSIData &frame);
    void reset();

    bool isDrawing() const { return is_drawing_; }
    float getCursorX() const { return cursor_x_; }
    float getCursorY() const { return cursor_y_; }
    bool isStrokeFinished() const { return stroke_finished_; }
    const std::string &getLastShape() const { return last_shape_; }
    float getShapeConfidence() const { return shape_confidence_; }
    float getDeltaTheta() const { return delta_theta_; }
    float getDeltaPwrDb() const { return delta_pwr_db_; }
    bool hasAnchor() const { return has_anchor_; }
    float getHeadingDeg() const { return heading_deg_; }
    const std::string &getHeadingDir() const { return heading_dir_; }
    float getVelX() const { return smooth_vx_; }
    float getVelY() const { return smooth_vy_; }
    float getSpeed() const { return current_speed_; }
    StrokePrimitive getLastPrimitive() const { return last_primitive_; }
    size_t getStrokeCount() const { return total_strokes_committed_; }

    void centerCursor();
    void calibrateAnchor();

    static std::string primitiveToString(StrokePrimitive prim);

  private:
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kMinDtSec = 0.005f;          // 5ms min
    static constexpr float kMaxDtSec = 0.200f;          // 200ms max
    static constexpr float kMaxIntegrationDt = 0.050f;  // 50ms max integration step
    static constexpr float kMaxHandSpeed = 0.91f;       // WiDraw Eq. 5: 0.91 m/s (3 ft/s)
    static constexpr float kMotionStartSpeed = 0.045f;  // Ngưỡng bắt đầu nét
    static constexpr float kStrokeEndSpeed = 0.025f;    // Ngưỡng kết thúc nét
    static constexpr float kMinStrokeDurationSec = 0.15f; // Nét tối thiểu 150ms
    static constexpr float kMaxStrokeDurationSec = 1.00f; // Nét tối đa 1.0s
    static constexpr float kMinStrokeDisplacement = 0.03f;
    static constexpr float kMinDirectionCoherence = 0.65f;
    static constexpr float kScaleVelX = 0.08f;          // Tỉ lệ vận tốc góc pha vi sai
    static constexpr float kScaleVelY = 0.08f;          // Tỉ lệ vận tốc ToF vi sai
    static constexpr float kLpfCutoffHz = 3.0f;         // Tần số cắt LPF (WiDraw Section 2.4)
    static constexpr size_t kMedianWindowSize = 5;      // Lọc trung vị trượt 5 frame (~50ms)
    static constexpr size_t kMaxStrokePoints = 600;

    // Trạng thái tọa độ và vận tốc động học
    bool is_drawing_ = false;
    bool stroke_finished_ = false;
    float cursor_x_ = 0.5f; // [0.0, 1.0]
    float cursor_y_ = 0.5f; // [0.0, 1.0]
    float target_x_ = 0.5f;
    float target_y_ = 0.5f;
    float vel_x_ = 0.0f;
    float vel_y_ = 0.0f;
    float smooth_vx_ = 0.0f;
    float smooth_vy_ = 0.0f;
    float current_speed_ = 0.0f;

    // Vi sai góc pha & công suất
    float delta_theta_ = 0.0f;
    float delta_pwr_db_ = 0.0f;

    // Hướng chuyển động tức thời
    float heading_deg_ = 0.0f;
    std::string heading_dir_ = "none";
    float locked_heading_deg_ = 0.0f;
    std::string locked_heading_dir_ = "none";
    int heading_lock_count_ = 0;

    // Máy trạng thái phân đoạn nét vẽ
    StrokeState stroke_state_ = StrokeState::IDLE;
    StrokePrimitive last_primitive_ = StrokePrimitive::NONE;
    size_t total_strokes_committed_ = 0;
    uint64_t stroke_start_ts_us_ = 0;
    float stroke_start_x_ = 0.5f;
    float stroke_start_y_ = 0.5f;
    StrokeFeatures current_stroke_features_;

    // Điểm neo tham chiếu (Anchor)
    float anchor_u_ = 0.0f;
    float anchor_v_ = 0.0f;
    float anchor_pwr_ = 0.0f;
    bool has_anchor_ = false;
    int anchor_calib_remaining_ = 5;

    std::vector<float> calib_u_samples_;
    std::vector<float> calib_v_samples_;
    std::vector<float> calib_pwr_samples_;

    // Bộ đệm Common Level Centering để giảm ảnh hưởng của biến đổi gain chung.
    std::vector<float> prev_centered_c0_;
    std::vector<float> prev_centered_c1_;
    bool has_prev_centered_ = false;

    // Lọc trung vị trượt (Sliding Window Median)
    std::deque<float> med_vx_;
    std::deque<float> med_vy_;

    // Bám vết pha vi sai, độ dốc ToF vi sai và tỉ số công suất
    float prev_cross_phase_ = 0.0f;
    float prev_pwr_ratio_db_ = 0.0f;
    float prev_diff_slope_ = 0.0f;
    float prev_diff_phase_12_ = 0.0f;
    bool has_prev_cross_ = false;

    uint64_t last_motion_ts_us_ = 0;
    uint64_t prev_frame_ts_us_ = 0;
    int activity_energy_ = 0;

    // Chuỗi điểm quỹ đạo và các đoạn vector đã vẽ
    std::vector<AirPoint> current_stroke_;
    std::string last_shape_ = "idle";
    float shape_confidence_ = 0.0f;

    // Hàm toán học nội bộ
    static float wrapPhase(float phase);
    static float wrapPhasePi(float dphi);
    static float computeMedian(std::vector<float> &vec);
    static float computeDequeMedian(const std::deque<float> &q);

    // Heuristic phân loại hướng nét
    StrokePrimitive classifyStroke(const StrokeFeatures &feats, float &confidence);
    void commitStroke(StrokePrimitive primitive, float confidence, float intensity);
};

#endif // AIR_DRAW_HPP
