// ============================================================================
// Edge CSI Dashboard · Bilingual & Multi-Device Active Prober
// ============================================================================

const i18n = {
    vi: {
        brand_sub: 'AI GATEWAY · EDGE CSI',
        nav_workspace: 'WORKSPACE',
        tab_overview: 'Wi-Fi Sensing',
        tab_gesture_v: 'Demo Wi-Fi Sensing',
        btn_v_reset: 'Vẽ lại / Reset',
        sidebar_footer: 'Edge processing<br>Local network only',
        topbar_eyebrow: 'ĐO ĐẠC ROUTER / GIÁM SÁT CSI',
        btn_calibrate: 'Hiệu chuẩn lại',
        status_live: 'ĐÃ BẬT · LIVE',
        status_waiting: 'ĐANG CHỜ DỮ LIỆU',
        status_offline: 'MẤT KẾT NỐI',
        status_connecting: 'ĐANG KẾT NỐI',
        status_connected: 'Đã nối backend · chờ CSI',
        status_disrupted: 'Stream gián đoạn · đang nối lại',
        status_calibrating: 'Đang chờ hiệu chuẩn nền mới',
        status_packet_wait: 'Đang chờ gói (Client ngủ hoặc chu kỳ beacon thưa)',
        metric_sc: 'SUBCARRIERS',
        metric_chains: 'ANTENNA CHAINS',
        metric_stability: 'ĐỘ ỔN ĐỊNH & CHẤT LƯỢNG',
        lbl_signal: 'Sóng:',
        lbl_smoothness: 'Độ mượt:',
        lbl_space: 'Không gian:',
        tech_details_show: '[+] Chi tiết kỹ thuật',
        tech_details_hide: '[-] Thu gọn',
        metric_motion: 'MOTION SCORE',
        metric_distance: 'KHOẢNG CÁCH',
        env_los: 'Truyền thẳng',
        env_standard: 'Tiêu chuẩn',
        env_nlos: 'Nhiều vật cản',
        path_loss_n: 'Hệ số n:',
        btn_calibrate: 'Hiệu chuẩn',
        metric_presence: 'NHẬN DIỆN HIỆN DIỆN',
        metric_wave: 'CỬ CHỈ VẪY TAY',
        wave_idle: 'KHÔNG CÓ',
        wave_detected: 'PHÁT HIỆN',
        wave_waiting: 'CHỜ DỮ LIỆU',
        caption_wave_idle: 'Chưa phát hiện cử chỉ',
        caption_wave_detected: 'Cử chỉ vẫy tay đang diễn ra',
        caption_wave_waiting: 'Cần luồng CSI 80 MHz (BW=2)',
        metric_framerate: 'TỐC ĐỘ GÓI',
        caption_waiting_csi: 'Chờ dữ liệu CSI',
        caption_framerate: 'Tốc độ nhận gần nhất',
        presence_moving: 'CÓ CHUYỂN ĐỘNG',
        presence_present: 'CÓ NGƯỜI',
        presence_empty: 'KHÔNG CÓ NGƯỜI',
        presence_calibrating: 'ĐANG HIỆU CHUẨN',
        caption_moving: 'Phát hiện chuyển động người / bước đi / cử động',
        caption_empty: 'Phòng trống tĩnh · Không phát hiện người',
        caption_calibrating: 'Đang tích lũy tín hiệu pha để phân tích',
        label_chain: 'CHAIN',
        chart_empty_title: 'Đang chờ dữ liệu CSI',
        chart_empty_desc: 'Kết nối relay stream trên router để bắt đầu quan sát',
        ctx_eyebrow: 'BỐI CẢNH THU THẬP',
        ctx_title: 'Frame hiện tại',
        ctx_bw: 'Mã băng thông kênh',
        note_eyebrow: 'GHI CHÚ TÍN HIỆU',
        note_title: 'Đọc dữ liệu chính xác',
        note_p1: 'Motion score là 1 trừ tương quan Pearson giữa vector biên độ hai frame, làm mượt bằng median 30 mẫu. Nền chỉ được hiệu chuẩn sau 40 score ổn định liên tiếp (≤ 0,40; độ lệch chuẩn ≤ 0,035). Bấm “Hiệu chuẩn lại” để xóa baseline và bắt đầu lấy nền mới.',
        note_p2: 'Biên độ là giá trị I/Q thô, chưa quy đổi sang dBm. Tính năng Active Probing duy trì xung nhịp liên tục 20-50 Hz kể cả khi người dùng không dùng mạng.',
        footer_title: 'EDGE CSI DASHBOARD',
        footer_right: 'DỮ LIỆU XỬ LÝ TRỰC TIẾP TRÊN ROUTER',
        // Prober panel
        prober_badge: 'ACTIVE PROBING',
        prober_title: 'Ping Giữ Nhịp Vô Tuyến (Device Keep-alive)',
        prober_desc: 'Router tự động phát gói tin thăm dò (ARP / UDP) để kích thích gói phản hồi từ điện thoại, duy trì luồng CSI liên tục ngay cả khi người dùng không chạm vào máy.',
        prober_on: 'Đang phát',
        prober_off: 'Đang tắt',
        target_client_label: 'Thiết bị Wi-Fi mục tiêu:',
        auto_select_wifi: '-- Tự động chọn client Wi-Fi mạnh nhất --',
        custom_ip_placeholder: 'Nhập IP thủ công (vd: 192.168.10.238)',
        cards_section_title: 'Danh Sách Thiết Bị Kết Nối Theo Card Wi-Fi',
        auto_strongest_btn: 'Tự động chọn sóng mạnh nhất',
        manual_ip_btn: 'Nhập IP thủ công',
        refresh_btn: 'Quét lại',
        col_select: 'Chọn',
        col_device: 'Tên thiết bị',
        col_ip: 'Địa chỉ IP',
        col_mac: 'Địa chỉ MAC',
        col_signal: 'RSSI',
        col_distance: 'Khoảng cách',
        col_bitrate: 'Tốc độ TX',
        col_activity: 'Hoạt động',
        badge_capturing: 'ĐANG THU',
        zone_immediate: 'Sát (<0.6m)',
        zone_near: 'Gần (0.6-1.8m)',
        zone_mid: 'Vừa (1.8-4m)',
        zone_far: 'Xa (>4m)',
        no_card_stations: 'Không có thiết bị nào đang kết nối trên card này',
        no_ip_assigned: 'Chưa cấp IP',
        selecting_auto: 'Chưa chọn thiết bị (Nhấp chọn thiết bị bên dưới để thu)',
        unselected_device: 'Chưa chọn thiết bị (Nhấp chọn thiết bị bên dưới để thu)',
        prompt_select_device: 'Vui lòng nhấp chọn một thiết bị Wi-Fi trong danh sách bên dưới trước khi bắt đầu thu!',
        option_default_tag: 'Mặc định',
        selecting_manual: 'Đang chọn thủ công: ',
        selecting_device: 'Đang chọn: ',
        devices_count: 'thiết bị',
        searching_wifi_clients: 'Đang tải thông tin Card Wi-Fi và thiết bị kết nối...',
        no_wifi_clients: 'Không tìm thấy Card Wi-Fi hoặc thiết bị kết nối.',
        interval_label: 'Chu kỳ phát (Interval):',
        apply_btn: 'Áp dụng',
        applying: 'Đang lưu…',
        saved_btn: 'Đã lưu!',
        stat_sent: 'Đã gửi',
        stat_status: 'Trạng thái',
        prober_active: 'Đang ping',
        prober_idle: 'Tạm dừng',
        // CFR Panel
        cfr_badge: 'VNPT TECHNOLOGY · SENSING PIPELINE',
        cfr_title: 'Điều Khiển Thu Thập CSI (CFR Capture Control)',
        cfr_desc: 'Tự động cấu hình thanh ghi debugfs nhân Linux (enable_cfr, cfr_capture) và điều khiển tiến trình cfr_test_app trên router để thu thập luồng CSI trực tiếp.',
        cfr_status_idle: 'Chưa thu thập',
        cfr_status_running: 'Đang thu thập',
        cfr_status_error: 'Lỗi khởi chạy',
        cfr_btn_start: 'Bắt đầu thu CSI',
        cfr_btn_stop: 'Dừng thu',
        cfr_btn_download: 'Tải file vừa thu (.bin)',
        cfr_bw_label: 'Băng thông CFR:',
        cfr_bw_20: '20 MHz (Chuẩn)',
        cfr_bw_320: '320 MHz (Wi-Fi 7)',
        cfr_bandwidth: 'Băng thông',
        cfr_file: 'Tệp tin vừa thu',
        cfr_no_file: 'Chưa có file',
        download_btn_title: 'Nhấp để tải file',
        bw_supported: 'Khả dụng',
        bw_unsupported: 'Không hỗ trợ',
        bw_available_prefix: 'Khả dụng',
        cfr_auto_probe_label: 'Tự động Ping giữ nhịp (Active Probing) để duy trì luồng CSI liên tục',
        cfr_target_device: 'Thiết bị mục tiêu',
        cfr_iface: 'Giao diện Wi-Fi',
        cfr_chipset: 'Phần cứng / Driver',
        cfr_pid: 'Tiến trình (PID)',
        cfr_uptime: 'Thời gian thu',
        btn_quick_capture: 'Thu CSI',
        pill_capturing: 'ĐANG THU',
        cfr_no_client_alert: 'Không tìm thấy thiết bị Wi-Fi nào đang kết nối. Vui lòng kết nối thiết bị vào Wi-Fi trước.',
        // Gesture V
        gesture_badge: 'GIÁM SÁT CỬ CHỈ THỜI GIAN THỰC',
        gesture_title: 'Demo công nghệ Wi-Fi Sensing (V-Shape Gesture)',
        gesture_subtitle: 'Nét xuống ➔ Dừng tại đáy ➔ Nét lên. Hệ thống tự động ghi nhận khi hoàn thành đúng chu kỳ cử chỉ.',
        gesture_state_idle: 'Chờ tín hiệu',
        gesture_state_not_capturing: 'Chờ thu CSI',
        gesture_guide_not_capturing: 'Vui lòng nhấn [Bắt đầu thu CSI] trên thiết bị mục tiêu để kích hoạt nhận diện cử chỉ',
        gesture_celebrate_title: 'GHI NHẬN CỬ CHỈ CHỮ V THÀNH CÔNG!',
        gesture_celebrate_desc: 'Đã hoàn thành đầy đủ chu kỳ cử chỉ (Xuống ➔ Đáy ➔ Lên) đạt độ chuẩn xác cao.',
        v_canvas_title: 'TRỰC QUAN HÓA QUỸ ĐẠO CỬ CHỈ',
        v_label_start: '① Bắt đầu nét xuống',
        v_label_pause: '② Dừng tại đáy',
        v_label_end: '③ Hoàn tất nét lên',
        v_guide_title: 'QUY TRÌNH THỰC HIỆN CỬ CHỈ',
        v_step_1_title: 'Nét vẽ xuống (Down-stroke)',
        v_step_1_desc: 'Thực hiện nét chuyển động từ trên xuống dưới liên tục ≥ 0.5s. Quỹ đạo bên trái sẽ sáng kích hoạt.',
        v_step_2_title: 'Dừng tại đáy (Apex Pause)',
        v_step_2_desc: 'Tạm dừng chuyển động rõ ràng tại đáy ≥ 0.2s. Điểm chóp đáy phát xung nhịp xác nhận.',
        v_step_3_title: 'Nét vẽ lên (Up-stroke)',
        v_step_3_desc: 'Thực hiện nét chuyển động từ dưới lên trên liên tục ≥ 0.5s. Quỹ đạo bên phải sẽ sáng vút lên.',
        v_step_4_title: 'Hoàn tất chu kỳ',
        v_step_4_desc: 'Hoàn tất cả 3 bước trong ≤ 5.0s ➔ Toàn bộ ký hiệu chữ V xác nhận thành công!',
        v_tele_model: 'LOẠI CỬ CHỈ',
        v_tele_threshold: 'NGƯỠNG KÍCH HOẠT',
        v_tele_score: 'ĐIỂM TIN CẬY (CONFIDENCE)',
        v_tele_label: 'TRẠNG THÁI VẬN ĐỘNG',
        v_tele_stage: 'PHA HIỆN TẠI',
        v_tele_timing: 'THỜI LƯỢNG (XUỐNG / ĐÁY / LÊN)',
        m5r_chart_kicker: 'THEO DÕI LIÊN TỤC',
        m5r_chart_title: 'Đồ Thị Điểm Tin Cậy & Ngưỡng Kích Hoạt',
        legend_m5r_score: 'Điểm tin cậy',
        legend_m5r_thresh: 'Ngưỡng kích hoạt (0.68)',
        v_guide_ready: 'Sẵn sàng thực hiện cử chỉ...',
        // Views
        views: {
            overview: { title: 'Wi-Fi Sensing', kicker: 'ROUTER TELEMETRY / CSI MONITOR', chart: 'Biến thiên mức độ chuyển động (Motion Score)' },
            gesture_v: { title: 'Demo công nghệ Wi-Fi Sensing', kicker: 'GESTURE TELEMETRY', chart: 'Biến thiên cử chỉ & Nhận diện' }
        }
    },
    en: {
        brand_sub: 'AI GATEWAY · EDGE CSI',
        nav_workspace: 'WORKSPACE',
        tab_overview: 'Wi-Fi Sensing',
        tab_gesture_v: 'Demo Wi-Fi Sensing',
        btn_v_reset: 'Redraw / Reset',
        sidebar_footer: 'Edge processing<br>Local network only',
        topbar_eyebrow: 'ROUTER TELEMETRY / CSI MONITOR',
        btn_calibrate: 'Recalibrate',
        status_live: 'ENABLED · LIVE',
        status_waiting: 'WAITING FOR DATA',
        status_offline: 'DISCONNECTED',
        status_connecting: 'CONNECTING',
        status_connected: 'Backend connected · waiting for CSI',
        status_disrupted: 'Stream disrupted · reconnecting',
        status_calibrating: 'Waiting for new baseline calibration',
        status_packet_wait: 'Waiting for packets (Client asleep or slow beacons)',
        metric_sc: 'SUBCARRIERS',
        metric_chains: 'ANTENNA CHAINS',
        metric_stability: 'STABILITY & QUALITY',
        lbl_signal: 'Signal:',
        lbl_smoothness: 'Smoothness:',
        lbl_space: 'Space:',
        tech_details_show: '[+] Tech details',
        tech_details_hide: '[-] Collapse',
        metric_motion: 'MOTION SCORE',
        metric_distance: 'DISTANCE',
        env_los: 'Direct LoS',
        env_standard: 'Standard',
        env_nlos: 'Dense NLoS',
        path_loss_n: 'Exponent n:',
        btn_calibrate: 'Calibrate',
        metric_presence: 'PRESENCE DETECTION',
        metric_wave: 'HAND-WAVE GESTURE',
        wave_idle: 'NONE',
        wave_detected: 'DETECTED',
        wave_waiting: 'WAITING',
        caption_wave_idle: 'No gesture detected',
        caption_wave_detected: 'Waving gesture in progress',
        caption_wave_waiting: 'Requires 80 MHz CSI stream',
        metric_framerate: 'FRAME RATE',
        caption_waiting_csi: 'Waiting for CSI data',
        caption_framerate: 'Latest received rate',
        presence_moving: 'MOTION DETECTED',
        presence_present: 'HUMAN PRESENT',
        presence_empty: 'EMPTY ROOM',
        presence_calibrating: 'CALIBRATING',
        caption_moving: 'Detected body movement / walking / gestures',
        caption_empty: 'Static empty room · No human detected',
        caption_calibrating: 'Accumulating phase signals for analysis',
        label_chain: 'CHAIN',
        chart_empty_title: 'Waiting for CSI data',
        chart_empty_desc: 'Connect the router relay stream to begin telemetry',
        ctx_eyebrow: 'CAPTURE CONTEXT',
        ctx_title: 'Current Frame',
        ctx_bw: 'Channel bandwidth code',
        note_eyebrow: 'SIGNAL NOTES',
        note_title: 'Accurate Data Reading',
        note_p1: 'Motion score is 1 minus Pearson correlation between amplitude vectors of consecutive frames, smoothed by median of 30 samples. Baseline calibrates after 40 stable scores (≤ 0.40; std deviation ≤ 0.035). Click “Recalibrate” to reset baseline.',
        note_p2: 'Amplitude values are raw I/Q without dBm conversion. Active Probing maintains continuous 20-50 Hz packet rate even when users are idle.',
        footer_title: 'EDGE CSI DASHBOARD',
        footer_right: 'DATA PROCESSED ON-ROUTER',
        // Prober panel
        prober_badge: 'METHOD 1 · ACTIVE PROBING',
        prober_title: 'Active Wi-Fi Keep-Alive Probing',
        prober_desc: 'Router automatically transmits probe packets (ARP / UDP) to trigger client responses, maintaining a continuous CSI stream even when phones are locked or sleeping.',
        prober_on: 'Probing',
        prober_off: 'Disabled',
        target_client_label: 'Target Wi-Fi Device(s):',
        auto_select_wifi: '-- Auto-select strongest Wi-Fi client --',
        custom_ip_placeholder: 'Custom IP (e.g. 192.168.10.238)',
        cards_section_title: 'Wi-Fi Interfaces & Connected Stations',
        auto_strongest_btn: 'Auto Strongest Signal',
        manual_ip_btn: 'Manual IP',
        refresh_btn: 'Refresh',
        col_select: 'Select',
        col_device: 'Device Name',
        col_ip: 'IP Address',
        col_mac: 'MAC Address',
        col_signal: 'RSSI',
        col_distance: 'Distance',
        col_bitrate: 'TX Rate',
        col_activity: 'Activity',
        badge_capturing: 'CAPTURING',
        zone_immediate: 'Immediate (<0.6m)',
        zone_near: 'Near (0.6-1.8m)',
        zone_mid: 'Mid (1.8-4m)',
        zone_far: 'Far (>4m)',
        no_card_stations: 'No stations connected on this interface',
        no_ip_assigned: 'No IP',
        selecting_auto: 'No device selected (Click a device below to capture)',
        unselected_device: 'No device selected (Click a device below to capture)',
        prompt_select_device: 'Please select a Wi-Fi device from the list below before starting capture!',
        option_default_tag: 'Default',
        selecting_manual: 'Target Manual IP: ',
        selecting_device: 'Target: ',
        devices_count: 'device(s)',
        searching_wifi_clients: 'Loading Wi-Fi cards and connected devices...',
        no_wifi_clients: 'No Wi-Fi cards or stations detected.',
        interval_label: 'Ping Interval:',
        apply_btn: 'Apply Settings',
        applying: 'Saving…',
        saved_btn: 'Saved!',
        stat_sent: 'Packets sent',
        stat_status: 'Status',
        prober_active: 'Probing',
        prober_idle: 'Idle',
        // CFR Panel
        cfr_badge: 'VNPT TECHNOLOGY · SENSING PIPELINE',
        cfr_title: 'CSI CFR Capture Control',
        cfr_desc: 'Automatically configures Linux kernel debugfs registers (enable_cfr, cfr_capture) and orchestrates cfr_test_app on the router for direct live CSI streaming.',
        cfr_status_idle: 'Idle',
        cfr_status_running: 'Capturing',
        cfr_status_error: 'Launch Error',
        cfr_btn_start: 'Start CSI Capture',
        cfr_btn_stop: 'Stop Capture',
        cfr_btn_download: 'Download capture (.bin)',
        cfr_bw_label: 'CFR Bandwidth:',
        cfr_bw_20: '20 MHz (Default)',
        cfr_bw_320: '320 MHz (Wi-Fi 7)',
        cfr_bandwidth: 'Bandwidth',
        cfr_file: 'Captured file',
        cfr_no_file: 'No file yet',
        download_btn_title: 'Click to download file',
        bw_supported: 'Supported',
        bw_unsupported: 'Not supported',
        bw_available_prefix: 'Available',
        cfr_auto_probe_label: 'Auto-enable Active Probing keep-alive to maintain continuous CSI stream',
        cfr_target_device: 'Target Station',
        cfr_iface: 'Wi-Fi Interface',
        cfr_chipset: 'Hardware / Driver',
        cfr_pid: 'Process PID',
        cfr_uptime: 'Capture Uptime',
        btn_quick_capture: 'Capture',
        pill_capturing: 'CAPTURING',
        cfr_no_client_alert: 'No connected Wi-Fi station found. Please connect a phone/laptop to Wi-Fi first.',
        // Gesture V
        gesture_badge: 'REALTIME GESTURE TELEMETRY',
        gesture_title: 'V-Shape Gesture Recognition',
        gesture_subtitle: 'Down-stroke ➔ Apex pause ➔ Up-stroke. Automatically registered upon completing the cycle.',
        gesture_state_idle: 'Waiting for signal',
        gesture_state_not_capturing: 'Awaiting Capture',
        gesture_guide_not_capturing: 'Please click [Start CSI Capture] on target device to activate gesture recognition',
        gesture_celebrate_title: 'GESTURE REGISTERED SUCCESSFULLY!',
        gesture_celebrate_desc: 'Completed V-shape gesture cycle with high confidence.',
        v_canvas_title: 'GESTURE TRAJECTORY VISUALIZATION',
        v_label_start: '① Start Down-Stroke',
        v_label_pause: '② Pause at Apex',
        v_label_end: '③ Finish Up-Stroke',
        v_guide_title: 'GESTURE EXECUTION PROCEDURE',
        v_step_1_title: 'Down-stroke',
        v_step_1_desc: 'Execute downward motion continuously for ≥ 0.5s. Left trajectory illuminates active.',
        v_step_2_title: 'Apex Pause',
        v_step_2_desc: 'Pause motion steadily at the vertex for ≥ 0.2s. Apex beacon pulses to confirm.',
        v_step_3_title: 'Up-stroke',
        v_step_3_desc: 'Execute upward motion continuously for ≥ 0.5s. Right trajectory illuminates upwards.',
        v_step_4_title: 'Cycle Complete',
        v_step_4_desc: 'Complete all steps within ≤ 5.0s ➔ Gesture registered successfully!',
        v_tele_model: 'GESTURE TYPE',
        v_tele_threshold: 'DECISION THRESHOLD',
        v_tele_score: 'CONFIDENCE SCORE',
        v_tele_label: 'MOTION STATE',
        v_tele_stage: 'ACTIVE PHASE',
        v_tele_timing: 'DOWN / PAUSE / UP',
        m5r_chart_kicker: 'CONTINUOUS MONITORING',
        m5r_chart_title: 'Confidence Score & Trigger Threshold',
        legend_m5r_score: 'Confidence Score',
        legend_m5r_thresh: 'Trigger Threshold (0.68)',
        v_guide_ready: 'Ready for gesture execution...',
        // Views
        views: {
            overview: { title: 'Wi-Fi Sensing', kicker: 'ROUTER TELEMETRY / CSI MONITOR', chart: 'Motion Score Activity' },
            gesture_v: { title: 'V-Gesture Recognition', kicker: 'GESTURE TELEMETRY', chart: 'Confidence Score & Trajectory' }
        }
    }
};

let currentLang = localStorage.getItem('edge_csi_lang') || 'vi';
function t(key) {
    return i18n[currentLang]?.[key] || i18n.vi[key] || key;
}

function applyLanguage(lang) {
    if (!i18n[lang]) return;
    currentLang = lang;
    localStorage.setItem('edge_csi_lang', lang);

    document.querySelectorAll('.lang-btn').forEach(b => {
        b.classList.toggle('active', b.dataset.lang === lang);
    });

    document.querySelectorAll('[data-i18n]').forEach(el => {
        const key = el.dataset.i18n;
        const translation = t(key);
        if (translation) {
            if (translation.includes('<br>')) {
                el.innerHTML = translation;
            } else {
                el.textContent = translation;
            }
        }
    });

    setView(activeView);
    if (latestFrame) {
        updateDomMetrics(latestFrame, false, performance.now());
    }
    renderProberUI(currentProberConfig);
    renderCfrUI();
    updateAvailableBandwidths();
}

const colors = ['#00e5ff', '#ff2a85', '#8b5cf6', '#0072ce', '#38bdf8'];
const canvas = document.getElementById('signal-chart');
const context = canvas.getContext('2d');
const emptyOverlay = document.getElementById('chart-empty');
const chainSelect = document.getElementById('chain-select');
const calibrateButton = document.getElementById('calibrate-button');

const domElements = {
    stabilityScoreVal: document.getElementById('stability-score-val'),
    stabilityStatusChip: document.getElementById('stability-status-chip'),
    stabilityRatingLabel: document.getElementById('stability-rating-label'),
    stabilityNeedle: document.getElementById('stability-needle'),
    gaugeCenterCap: document.getElementById('gauge-center-cap'),
    pillSignal: document.getElementById('pill-signal'),
    pillSignalVal: document.getElementById('pill-signal-val'),
    pillJitter: document.getElementById('pill-jitter'),
    pillJitterVal: document.getElementById('pill-jitter-val'),
    pillEnv: document.getElementById('pill-env'),
    pillEnvVal: document.getElementById('pill-env-val'),
    stabilityTechToggleBtn: document.getElementById('stability-tech-toggle-btn'),
    techToggleText: document.getElementById('tech-toggle-text'),
    stabilityTechDetails: document.getElementById('stability-tech-details'),
    techAgcVal: document.getElementById('tech-agc-val'),
    techFadeVal: document.getElementById('tech-fade-val'),
    techKVal: document.getElementById('tech-k-val'),
    techLosVal: document.getElementById('tech-los-val'),
    techCorrVal: document.getElementById('tech-corr-val'),
    techVarVal: document.getElementById('tech-var-val'),
    stabilityAgcPill: document.getElementById('stability-agc-pill'),
    stabilityAgcText: document.getElementById('stability-agc-text'),
    stabilityFadePill: document.getElementById('stability-fade-pill'),
    stabilityFadeText: document.getElementById('stability-fade-text'),
    stabilityKPill: document.getElementById('stability-k-pill'),
    stabilityKText: document.getElementById('stability-k-text'),
    stabilityCaption: document.getElementById('stability-caption'),
    stabilityLiveDot: document.getElementById('stability-live-dot'),
    motionValue: document.getElementById('motion-value'),
    motionCaption: document.getElementById('motion-caption'),
    distanceVal: document.getElementById('distance-val'),
    distanceZoneChip: document.getElementById('distance-zone-chip'),
    distanceCaption: document.getElementById('distance-caption'),
    fsplBandBadge: document.getElementById('fspl-band-badge'),
    distLiveDot: document.getElementById('dist-live-dot'),
    distDiagInfo: document.getElementById('dist-diag-info'),
    distCalBtn: document.getElementById('dist-cal-btn'),
    distEnvTabs: document.getElementById('dist-env-tabs'),
    envTabLoS: document.getElementById('env-tab-los'),
    envTabStandard: document.getElementById('env-tab-standard'),
    envTabNLoS: document.getElementById('env-tab-nlos'),
    distErrorTag: document.getElementById('dist-error-tag'),
    distErrorVal: document.getElementById('dist-error-val'),
    distNInput: document.getElementById('dist-n-input'),
    distNDec: document.getElementById('dist-n-dec'),
    distNInc: document.getElementById('dist-n-inc'),
    gestureVPanel: document.getElementById('gesture-v-panel'),
    vShape: document.getElementById('vShape'),
    vDown: document.getElementById('vDown'),
    vUp: document.getElementById('vUp'),
    vPoint: document.getElementById('vPoint'),
    v6StateLabel: document.getElementById('v6StateLabel'),
    v6GuideDetail: document.getElementById('v6GuideDetail'),
    v6GuideBox: document.getElementById('v6GuideBox'),
    v6SyncedTarget: document.getElementById('v6-synced-target'),
    v6TeleScore: document.getElementById('v6-tele-score'),
    v6TeleLabel: document.getElementById('v6-tele-label'),
    v6TelePass: document.getElementById('v6-tele-pass'),
    v6TeleFail: document.getElementById('v6-tele-fail'),
    v6Notice: document.getElementById('v6Notice'),
    v6TimingDetails: document.getElementById('v6TimingDetails')
};

let currentProberConfig = {
    enabled: false,
    mode: 'single',
    target_ip: '',
    target_ips: [],
    interval_ms: 50,
    packets_sent: 0,
    cards: [],
    clients: []
};

let activeView = 'overview';
let latestFrame = null;
let motionHistory = [];
let breathWaveHistory = [];
let receiveTimes = [];
let frameRate = 0;
let lastFrameAt = 0;
let renderRequested = false;
let lastDomUpdateAt = 0;

function scheduleRender() {
    if (!renderRequested) {
        renderRequested = true;
        requestAnimationFrame(() => {
            renderRequested = false;
            if (document.hidden || activeView === 'gesture_v') return; // Không tốn CPU render canvas khi tab ẩn hoặc đang xem tab V
            drawCurrent();
        });
    }
}

function getDistanceZoneMeta(distOrZone) {
    let zone = distOrZone;
    if (typeof distOrZone === 'number') {
        if (distOrZone < 0.6) zone = 'immediate';
        else if (distOrZone < 1.8) zone = 'near';
        else if (distOrZone < 4.0) zone = 'mid';
        else zone = 'far';
    }
    switch (zone) {
        case 'immediate':
            return {
                label: t('zone_immediate'),
                cls: 'zone-immediate'
            };
        case 'near':
            return {
                label: t('zone_near'),
                cls: 'zone-near'
            };
        case 'mid':
            return {
                label: t('zone_mid'),
                cls: 'zone-mid'
            };
        case 'far':
        default:
            return {
                label: t('zone_far'),
                cls: 'zone-far'
            };
    }
}

function updateStabilityUI(frame, isNewFrame) {
    if (!domElements.stabilityScoreVal || !frame) return;

    if (domElements.stabilityLiveDot && isNewFrame) {
        if (!domElements.stabilityLiveDot.classList.contains('pulse')) {
            domElements.stabilityLiveDot.classList.add('pulse');
            setTimeout(() => {
                if (domElements.stabilityLiveDot) domElements.stabilityLiveDot.classList.remove('pulse');
            }, 300);
        }
    }

    const score = Number(frame.link_stability_score);
    const hasScore = !isNaN(score) && score > 0;
    const statusText = frame.link_status_text || 'calibrating';
    const isAgcEvent = Boolean(frame.is_agc_event);
    const deepFadeWarn = Boolean(frame.deep_fade_warning);
    const fadeRatio = Number(frame.deep_fade_ratio) || 1.0;
    const spectralCorr = Number(frame.spectral_correlation) || 1.0;
    const kRician = Number(frame.csi_rician_k) || 0.0;
    const losRatio = Number(frame.csi_los_ratio) || 0.0;
    const varTime = Number(frame.temporal_csi_variance) || 0.0;

    if (hasScore) {
        domElements.stabilityScoreVal.textContent = Math.round(score);

        // Update Analog Gauge Needle Rotation (-90deg at 0 to +90deg at 100)
        if (domElements.stabilityNeedle) {
            const clampedScore = Math.min(100, Math.max(0, score));
            const angle = -90 + (clampedScore / 100) * 180;
            domElements.stabilityNeedle.style.transform = `rotate(${angle.toFixed(1)}deg)`;

            const needleColorCls = score >= 85 ? 'needle-high' : score >= 70 ? 'needle-med' : score >= 50 ? 'needle-warn' : 'needle-low';
            domElements.stabilityNeedle.className = `gauge-needle ${needleColorCls}`;
        }

        // 1. Rating Label & Top Chip
        let ratingText = '';
        let ratingClass = 'stability-rating-badge rating-none';
        let chipText = 'KHỞI TẠO';
        let chipCls = 'motion-chip';
        let captionText = '';

        if (score >= 85) {
            ratingText = currentLang === 'vi' ? 'Xuất sắc (Rất mượt mà)' : 'Excellent (Very Smooth)';
            ratingClass = 'stability-rating-badge rating-excellent';
            chipText = currentLang === 'vi' ? 'XUẤT SẮC' : 'EXCELLENT';
            chipCls = 'motion-chip chip-los';
            captionText = currentLang === 'vi'
                ? 'Tác vụ phù hợp: Xem video 4K/8K, Họp trực tuyến HD, Game ping thấp'
                : 'Suitable tasks: 4K/8K streaming, HD video conference, Low-ping gaming';
        } else if (score >= 70) {
            ratingText = currentLang === 'vi' ? 'Tốt (Ổn định)' : 'Good (Stable)';
            ratingClass = 'stability-rating-badge rating-good';
            chipText = currentLang === 'vi' ? 'ỔN ĐỊNH' : 'STABLE';
            chipCls = 'motion-chip chip-multipath';
            captionText = currentLang === 'vi'
                ? 'Tác vụ phù hợp: Xem phim Full HD, Họp video Meet/Teams, Lướt web nhanh'
                : 'Suitable tasks: Full HD streaming, Video meetings, Fast web browsing';
        } else if (score >= 50) {
            ratingText = currentLang === 'vi' ? 'Khá (Bình thường)' : 'Fair (Normal)';
            ratingClass = 'stability-rating-badge rating-fair';
            chipText = currentLang === 'vi' ? 'BÌNH THƯỜNG' : 'NORMAL';
            chipCls = 'motion-chip chip-dynamic';
            captionText = currentLang === 'vi'
                ? 'Tác vụ phù hợp: Lướt mạng xã hội, Nghe nhạc, Đọc tin tức'
                : 'Suitable tasks: Social media, Music streaming, News browsing';
        } else {
            ratingText = currentLang === 'vi' ? 'Yếu (Chập chờn)' : 'Poor (Unstable)';
            ratingClass = 'stability-rating-badge rating-poor';
            chipText = currentLang === 'vi' ? 'SÓNG YẾU' : 'WEAK SIGNAL';
            chipCls = 'motion-chip chip-weak';
            captionText = currentLang === 'vi'
                ? 'Khuyến nghị: Tín hiệu suy giảm, nên di chuyển thiết bị lại gần router hơn'
                : 'Recommendation: Signal degraded, consider moving closer to router';
        }

        if (domElements.stabilityRatingLabel) {
            domElements.stabilityRatingLabel.textContent = ratingText;
            domElements.stabilityRatingLabel.className = ratingClass;
        }

        if (domElements.stabilityStatusChip) {
            domElements.stabilityStatusChip.textContent = chipText;
            domElements.stabilityStatusChip.className = chipCls;
        }

        // 2. Friendly Pill 1: Signal strength
        const rssi = frame.rssi_comb_db !== undefined ? Number(frame.rssi_comb_db) : -60;
        if (domElements.pillSignalVal) {
            if (rssi >= -50) {
                domElements.pillSignalVal.textContent = currentLang === 'vi' ? `Rất mạnh (${rssi} dBm)` : `Very strong (${rssi} dBm)`;
                if (domElements.pillSignal) domElements.pillSignal.className = 'friendly-pill pill-status-excellent';
            } else if (rssi >= -65) {
                domElements.pillSignalVal.textContent = currentLang === 'vi' ? `Tốt (${rssi} dBm)` : `Good (${rssi} dBm)`;
                if (domElements.pillSignal) domElements.pillSignal.className = 'friendly-pill pill-status-good';
            } else if (rssi >= -75) {
                domElements.pillSignalVal.textContent = currentLang === 'vi' ? `Khá (${rssi} dBm)` : `Fair (${rssi} dBm)`;
                if (domElements.pillSignal) domElements.pillSignal.className = 'friendly-pill pill-status-warn';
            } else {
                domElements.pillSignalVal.textContent = currentLang === 'vi' ? `Yếu (${rssi} dBm)` : `Weak (${rssi} dBm)`;
                if (domElements.pillSignal) domElements.pillSignal.className = 'friendly-pill pill-status-bad';
            }
        }

        // 3. Friendly Pill 2: Smoothness / stability
        if (domElements.pillJitterVal) {
            if (varTime <= 0.025) {
                domElements.pillJitterVal.textContent = currentLang === 'vi' ? 'Rất cao (Không trễ)' : 'High (Zero Lag)';
                if (domElements.pillJitter) domElements.pillJitter.className = 'friendly-pill pill-status-excellent';
            } else if (varTime <= 0.070) {
                domElements.pillJitterVal.textContent = currentLang === 'vi' ? 'Tốt (Dao động nhẹ)' : 'Good (Slight Jitter)';
                if (domElements.pillJitter) domElements.pillJitter.className = 'friendly-pill pill-status-good';
            } else {
                domElements.pillJitterVal.textContent = currentLang === 'vi' ? 'Có dao động / Cử động' : 'Dynamic / Movement';
                if (domElements.pillJitter) domElements.pillJitter.className = 'friendly-pill pill-status-warn';
            }
        }

        // 4. Friendly Pill 3: Space / Environment
        if (domElements.pillEnvVal) {
            if (statusText === 'clean_los' || losRatio >= 0.70) {
                domElements.pillEnvVal.textContent = currentLang === 'vi' ? 'Trực diện (LoS)' : 'Direct LoS';
                if (domElements.pillEnv) domElements.pillEnv.className = 'friendly-pill pill-status-excellent';
            } else if (statusText === 'multipath_stable' || losRatio >= 0.35) {
                domElements.pillEnvVal.textContent = currentLang === 'vi' ? 'Trong phòng' : 'Indoor Room';
                if (domElements.pillEnv) domElements.pillEnv.className = 'friendly-pill pill-status-good';
            } else if (statusText === 'deep_fade') {
                domElements.pillEnvVal.textContent = currentLang === 'vi' ? 'Lõm sóng' : 'Deep Fade';
                if (domElements.pillEnv) domElements.pillEnv.className = 'friendly-pill pill-status-bad';
            } else {
                domElements.pillEnvVal.textContent = currentLang === 'vi' ? 'Góc khuất' : 'Blocked / NLoS';
                if (domElements.pillEnv) domElements.pillEnv.className = 'friendly-pill pill-status-warn';
            }
        }

        // 5. Update Caption
        if (domElements.stabilityCaption) {
            domElements.stabilityCaption.textContent = captionText;
        }

        // 6. Update Technical details (Collapsible)
        if (domElements.techAgcVal) {
            domElements.techAgcVal.textContent = isAgcEvent
                ? (currentLang === 'vi' ? 'Bù trừ nấc' : 'Gain hop')
                : (currentLang === 'vi' ? 'Ổn định' : 'Stable');
        }
        if (domElements.techFadeVal) {
            domElements.techFadeVal.textContent = `${fadeRatio.toFixed(1)}x` + (deepFadeWarn ? (currentLang === 'vi' ? ' (Cảnh báo)' : ' (Warn)') : '');
        }
        if (domElements.techKVal) {
            domElements.techKVal.textContent = `${kRician.toFixed(1)}`;
        }
        if (domElements.techLosVal) {
            domElements.techLosVal.textContent = `${Math.round(losRatio * 100)}%`;
        }
        if (domElements.techCorrVal) {
            domElements.techCorrVal.textContent = `${spectralCorr.toFixed(3)}`;
        }
        if (domElements.techVarVal) {
            domElements.techVarVal.textContent = `${varTime.toFixed(4)}`;
        }
    } else {
        domElements.stabilityScoreVal.textContent = '—';
        if (domElements.stabilityNeedle) {
            domElements.stabilityNeedle.style.transform = 'rotate(-90deg)';
            domElements.stabilityNeedle.className = 'gauge-needle';
        }
        if (domElements.stabilityRatingLabel) {
            domElements.stabilityRatingLabel.textContent = currentLang === 'vi' ? 'Chờ dữ liệu' : 'Waiting for data';
            domElements.stabilityRatingLabel.className = 'stability-rating-badge rating-none';
        }
        if (domElements.stabilityStatusChip) {
            domElements.stabilityStatusChip.textContent = currentLang === 'vi' ? 'CHỜ DỮ LIỆU' : 'AWAITING';
            domElements.stabilityStatusChip.className = 'motion-chip';
        }
        if (domElements.pillSignalVal) domElements.pillSignalVal.textContent = '—';
        if (domElements.pillJitterVal) domElements.pillJitterVal.textContent = '—';
        if (domElements.pillEnvVal) domElements.pillEnvVal.textContent = '—';
        if (domElements.pillSignal) domElements.pillSignal.className = 'friendly-pill';
        if (domElements.pillJitter) domElements.pillJitter.className = 'friendly-pill';
        if (domElements.pillEnv) domElements.pillEnv.className = 'friendly-pill';
        if (domElements.stabilityCaption) {
            domElements.stabilityCaption.textContent = currentLang === 'vi' 
                ? 'Đang phân tích chất lượng kết nối...' 
                : 'Analyzing link quality...';
        }
    }
}

function updateDomMetrics(frame, isNewFrame, now) {
    updateStabilityUI(frame, isNewFrame);

    if (domElements.motionValue) {
        domElements.motionValue.textContent = (Number(frame.motion_score) || 0).toFixed(4);
    }

    const thresHigh = Number(frame.motion_threshold_high) || 0;
    const motionChipEl = document.getElementById('motion-chip');

    if (motionChipEl) {
        if (!frame.motion_calibrated) {
            motionChipEl.className = 'motion-chip';
            motionChipEl.textContent = currentLang === 'vi' ? 'CHỜ NỀN' : 'CALIB';
        } else {
            const score = Number(frame.motion_score) || 0;
            if (score >= thresHigh) {
                motionChipEl.className = 'motion-chip motion-active';
                motionChipEl.textContent = currentLang === 'vi' ? 'ĐỘNG' : 'MOTION';
            } else {
                motionChipEl.className = 'motion-chip motion-quiet';
                motionChipEl.textContent = currentLang === 'vi' ? 'TĨNH' : 'QUIET';
            }
        }
    }

    if (domElements.motionCaption) {
        if (!frame.motion_ready) {
            domElements.motionCaption.textContent = currentLang === 'vi'
                ? `Cửa sổ median ${frame.motion_window_samples || 0}/30 frame`
                : `Median window ${frame.motion_window_samples || 0}/30 frames`;
        } else if (!frame.motion_calibrated) {
            domElements.motionCaption.textContent = frame.motion_calibration_stable
                ? (currentLang === 'vi' ? `Hiệu chuẩn nền · ${frame.motion_calibration_frames || 0}/40 mẫu ổn định` : `Calibrating baseline · ${frame.motion_calibration_frames || 0}/40 stable`)
                : (currentLang === 'vi' ? `Tín hiệu dao động · chờ ổn định ${frame.motion_calibration_frames || 0}/40` : `Jittering · awaiting baseline ${frame.motion_calibration_frames || 0}/40`);
        } else {
            domElements.motionCaption.textContent = currentLang === 'vi'
                ? `Nền ổn định · Độ lệch chuẩn ≤ 0.035`
                : `Baseline calibrated · StdDev ≤ 0.035`;
        }
    }

    if (domElements.distanceVal && domElements.distanceCaption) {
        const dist = Number(frame.estimated_distance_m) || 0;
        const zone = frame.distance_zone || 'unknown';
        const trend = frame.distance_trend || 'stationary';
        const rssi = frame.rssi_comb_db !== undefined ? frame.rssi_comb_db : '—';
        const los = Math.round((Number(frame.csi_los_ratio) || 0) * 100);

        if (domElements.fsplBandBadge) {
            const band = frame.distance_band || ((cfrState && cfrState.band_name) ? cfrState.band_name : '5.2 GHz');
            domElements.fsplBandBadge.textContent = band;
            if (band.includes('2.4')) {
                domElements.fsplBandBadge.className = 'fspl-band-badge band-2g';
            } else if (band.includes('6')) {
                domElements.fsplBandBadge.className = 'fspl-band-badge band-6g';
            } else {
                domElements.fsplBandBadge.className = 'fspl-band-badge band-5g';
            }
        }

        if (domElements.distDiagInfo) {
            const a1m = frame.distance_a_1m !== undefined ? Number(frame.distance_a_1m).toFixed(1) : '—';
            const nVal = frame.distance_n !== undefined ? Number(frame.distance_n).toFixed(2) : '1.75';
            const src = frame.distance_cal_source === 'custom' ? (currentLang === 'vi' ? 'Hiệu chuẩn riêng' : 'Calibrated') : (currentLang === 'vi' ? 'Mặc định' : 'Default');
            domElements.distDiagInfo.textContent = `A1m: ${a1m} dBm · n: ${nVal} (${src})`;
        }

        if (domElements.distErrorVal && frame.distance_error_margin_m !== undefined) {
            const err = Number(frame.distance_error_margin_m) || 0.15;
            domElements.distErrorVal.textContent = err.toFixed(2);
        }

        if (frame.distance_env_mode) {
            syncDistanceEnvUI(frame.distance_env_mode, frame.distance_n);
        }

        if (domElements.distLiveDot && isNewFrame) {
            if (!domElements.distLiveDot.classList.contains('pulse')) {
                domElements.distLiveDot.classList.add('pulse');
                setTimeout(() => {
                    if (domElements.distLiveDot) domElements.distLiveDot.classList.remove('pulse');
                }, 300);
            }
        }

        if (dist > 0) {
            domElements.distanceVal.textContent = dist.toFixed(2);

            let zoneText = '—';
            let zoneClass = 'motion-chip';
            if (zone === 'immediate') {
                zoneText = currentLang === 'vi' ? 'Sát router (<0.6m)' : 'Immediate (<0.6m)';
                zoneClass = 'motion-chip motion-chip-active';
            } else if (zone === 'near') {
                zoneText = currentLang === 'vi' ? 'Gần (0.6-1.8m)' : 'Near (0.6-1.8m)';
                zoneClass = 'motion-chip motion-chip-active';
            } else if (zone === 'mid') {
                zoneText = currentLang === 'vi' ? 'Trung bình (1.8-4m)' : 'Mid (1.8-4m)';
                zoneClass = 'motion-chip';
            } else {
                zoneText = currentLang === 'vi' ? 'Xa (>4m)' : 'Far (>4m)';
                zoneClass = 'motion-chip';
            }
            if (domElements.distanceZoneChip) {
                domElements.distanceZoneChip.textContent = zoneText;
                domElements.distanceZoneChip.className = zoneClass;
            }

            let trendIcon = '•';
            let trendText = currentLang === 'vi' ? 'Đứng yên' : 'Stationary';
            if (trend === 'approaching') {
                trendIcon = '↓';
                trendText = currentLang === 'vi' ? 'Đang lại gần' : 'Approaching';
            } else if (trend === 'departing') {
                trendIcon = '↑';
                trendText = currentLang === 'vi' ? 'Đang đi xa' : 'Departing';
            }

            domElements.distanceCaption.textContent = `${trendIcon} ${trendText} · RSSI: ${rssi} dBm · LoS: ${los}%`;
        } else {
            domElements.distanceVal.textContent = '—';
            if (domElements.distanceZoneChip) domElements.distanceZoneChip.textContent = '—';
            domElements.distanceCaption.textContent = currentLang === 'vi' ? 'Chờ tín hiệu RSSI + CSI' : 'Waiting RSSI + CSI';
        }
    }

    if (frame && frame.rssi_comb_db !== undefined) {
        const liveValEl = document.querySelector('.capturing-row .station-rssi-text');
        if (liveValEl) {
            liveValEl.textContent = `${frame.rssi_comb_db} dBm`;
        }
    }

    updateGestureVUI(frame);

    if (isNewFrame) {
        setStatus('live', (currentLang === 'vi' ? 'Nhận frame ' : 'Frame received ') + new Date().toLocaleTimeString());
    }
}

const M5R_STROKE_LEN = 267; // Approximate length in px for each V stroke arm

function updateGestureVUI(frame) {
    return;
}

const v6Reasons = {
    draw_timeout: 'Bạn vẽ quá thời gian (tối đa 5,0 giây).',
    down_short: 'Nét xuống quá ngắn (< 0,15 giây).',
    down_long: 'Nét xuống quá dài (> 1,50 giây).',
    pause_short: 'Khoảng khựng ở đáy quá ngắn (< 0,15 giây).',
    pause_long: 'Khoảng khựng ở đáy quá dài (> 2,00 giây).',
    up_short: 'Nét lên quá ngắn (< 0,15 giây).',
    up_long: 'Nét lên quá dài (> 2,00 giây).',
    finish_moved: 'Điện thoại còn chuyển động trong lúc xác nhận.'
};

const v6Instructions = {
    WAIT_READY: 'Đang ổn định tín hiệu...',
    READY: 'Sẵn sàng',
    DOWN: 'Vẽ nét chéo xuống',
    DOWN_HOLD: 'Vẽ nét chéo xuống',
    BOTTOM: 'Khựng nhẹ ở đáy',
    UP_CUE: 'Khựng nhẹ ở đáy',
    UP: 'Vẽ nét chéo lên',
    UP_HOLD: 'Vẽ nét chéo lên',
    FINISH: 'Giữ yên để xác nhận',
    PASS: 'Đã nhận diện thành công!',
    FAIL: 'Không nhận diện'
};

function renderV(s) {
    return;
}

function setStatus(status, description) {
    const pill = document.getElementById('connection-pill');
    const dot = document.getElementById('status-dot');
    const label = document.getElementById('status');
    const lastUpdate = document.getElementById('last-update');

    if (pill) {
        pill.className = `connection-live-pill ${status === 'live' ? '' : status === 'waiting' ? 'pill-waiting' : 'pill-offline'}`;
    }
    if (dot) {
        dot.className = `live-status-dot ${status}`;
    }
    if (label) {
        label.textContent = status === 'live' 
            ? (currentLang === 'vi' ? 'ĐÃ BẬT · LIVE' : 'ENABLED · LIVE') 
            : status === 'waiting' ? t('status_waiting') : t('status_offline');
    }
    if (lastUpdate) {
        lastUpdate.textContent = description;
        lastUpdate.title = description;
    }
}

// ============================================================================
// Model V6 Native Engine Controller (Cổng 5243 - Trực Tiếp Động Cơ M5R Chuẩn 100%)
// ============================================================================
const v6State = {
    running: false,
    mac: '',
    ip: '',
    pollTimer: null,
    devices: [],
    masterActive: false,
    isPolling: false
};

function showV6Notice(message, isError = false) {
    if (!domElements.v6Notice) return;
    if (!message) {
        domElements.v6Notice.hidden = true;
        domElements.v6Notice.textContent = '';
        return;
    }
    domElements.v6Notice.hidden = false;
    domElements.v6Notice.textContent = message;
    domElements.v6Notice.style.color = isError ? '#ef4444' : '#00e5ff';
}

function updateV6UIControls(running) {
    v6State.running = Boolean(running);
    if (domElements.v6StartBtn) domElements.v6StartBtn.disabled = v6State.running;
    if (domElements.v6StopBtn) domElements.v6StopBtn.disabled = !v6State.running;
    if (domElements.v6DeviceSelect) domElements.v6DeviceSelect.disabled = v6State.running;
    if (domElements.v6IpInput) domElements.v6IpInput.disabled = v6State.running;
    if (domElements.v6RefreshBtn) domElements.v6RefreshBtn.disabled = v6State.running;
}

function syncTargetDeviceToV6(mac, ip, hostname = '') {
    if (!mac) return;
    v6State.mac = mac;
    if (ip) v6State.ip = ip;

    // Tự động tìm hostname từ danh sách client nếu chưa có
    if (!hostname && currentProberConfig && Array.isArray(currentProberConfig.clients)) {
        const found = currentProberConfig.clients.find(c => c.mac && c.mac.toLowerCase() === mac.toLowerCase());
        if (found) {
            hostname = found.hostname || '';
            if (!ip && found.ip) ip = found.ip;
        }
    }
    if (!hostname) {
        try {
            hostname = localStorage.getItem('edge_csi_selected_hostname') || '';
        } catch (_) {}
    }
    v6State.hostname = hostname;

    // Lưu vào localStorage để nhớ thiết bị kể cả khi refresh
    try {
        localStorage.setItem('edge_csi_selected_mac', mac);
        if (ip) localStorage.setItem('edge_csi_selected_ip', ip);
        if (hostname) localStorage.setItem('edge_csi_selected_hostname', hostname);
    } catch (_) {}

    // Iframe 8082 chỉ được nạp khi mở tab Cử chỉ chữ V (xem enterGestureView),
    // tránh 8082 tự thu CSI trong lúc người dùng đang xem tab Tổng quan.
    updateGestureToolbar();
    if (activeView === 'gesture_v') enterGestureView();
}

// ===== Tab Cử chỉ chữ V: nhúng cổng 8082, thiết bị lấy từ tab Tổng quan =====
// Driver ath12k chỉ cho một bộ đọc CFR. Khi vào tab chữ V, phiên thu của tab
// Tổng quan được dừng để 8082 thu; khi rời tab, 8082 dừng rồi phiên cũ được thu lại.
const gestureFrame = {
    key: '',             // mac|ip đang nạp trong iframe
    running: false,      // trạng thái phiên 8082 do iframe báo về (WAKE_STATE)
    paused: true,        // ban đầu ở trạng thái chờ/tạm dừng, người dùng bấm mới bắt đầu
    resumeOverview: null, // phiên thu Tổng quan cần khôi phục khi rời tab
    stopWaiters: []
};

function gestureHost() {
    return window.location.hostname || '192.168.5.1';
}

function gestureTarget() {
    let mac = String(cfrState.targetMac || cfrState.target_mac || v6State.mac || '').toLowerCase();
    let ip = String(cfrState.targetIp || cfrState.target_ip || v6State.ip || '');
    if (mac && !/^\d{1,3}(\.\d{1,3}){3}$/.test(ip) && currentProberConfig && Array.isArray(currentProberConfig.clients)) {
        const found = currentProberConfig.clients.find(c => c.mac && c.mac.toLowerCase() === mac);
        if (found && found.ip) ip = found.ip;
    }
    // Tự động tìm thiết bị 5 GHz khả dụng nếu người dùng chưa bấm chọn ở Tổng quan
    if ((!mac || !ip) && currentProberConfig && Array.isArray(currentProberConfig.clients)) {
        const autoClient = currentProberConfig.clients.find(c => c.is_5ghz && c.ip && /^\d{1,3}(\.\d{1,3}){3}$/.test(c.ip)) ||
                           currentProberConfig.clients.find(c => c.ip && /^\d{1,3}(\.\d{1,3}){3}$/.test(c.ip));
        if (autoClient) {
            mac = autoClient.mac.toLowerCase();
            ip = autoClient.ip;
            syncTargetDeviceToV6(mac, ip, autoClient.hostname || '');
        }
    }
    if (!/^([0-9a-f]{2}:){5}[0-9a-f]{2}$/.test(mac)) return null;
    return { mac, ip: /^\d{1,3}(\.\d{1,3}){3}$/.test(ip) ? ip : '' };
}

function populateGestureDeviceSelect(clients, currentTargetIp) {
    const select = document.getElementById('gesture-device-select');
    if (!select) return;

    const list = Array.isArray(clients) ? clients : (currentProberConfig && Array.isArray(currentProberConfig.clients) ? currentProberConfig.clients : []);
    const target = gestureTarget();
    const currentMac = (target && target.mac ? target.mac.toLowerCase() : '');
    const previousVal = select.value || currentMac;

    select.innerHTML = '';

    // Bỏ thiết bị 2.4 GHz, chỉ lấy các thiết bị 5 GHz
    const list5g = list.filter(c => {
        if (!c.mac) return false;
        if (c.is_5ghz) return true;
        if (c.freq && c.freq > 5000) return true;
        if (c.iface && (c.iface.includes('.0-') || c.iface.includes('5g') || c.iface.includes('5G'))) return true;
        return false;
    });
    const candidates = list5g.length > 0 ? list5g : list.filter(c => c.mac && !(c.freq && c.freq < 3000));

    if (!candidates.length) {
        select.innerHTML = '<option value="">Chưa có thiết bị 5 GHz</option>';
        return;
    }

    let matchedOption = false;
    for (const c of candidates) {
        if (!c.mac) continue;
        const macLower = c.mac.toLowerCase();
        // Chỉ cần tên thiết bị
        const deviceName = (c.hostname && c.hostname !== '*' && c.hostname.trim())
            ? c.hostname.trim()
            : (c.ip || c.mac);

        const opt = document.createElement('option');
        opt.value = macLower;
        opt.textContent = deviceName;
        opt.title = `${deviceName} · ${c.ip || 'chưa có IP'} · ${c.mac}`;
        opt.dataset.ip = c.ip || '';
        opt.dataset.hostname = c.hostname || '';

        if (previousVal && (previousVal === macLower || (target && target.ip && target.ip === c.ip))) {
            opt.selected = true;
            matchedOption = true;
        }
        select.appendChild(opt);
    }

    if (!matchedOption && select.options.length > 0) {
        select.selectedIndex = 0;
    }
}

function updateGestureToolbar() {
    const target = gestureTarget();
    const statusText = document.getElementById('gesture-status-text');
    const statusDot = document.getElementById('gesture-status-dot');

    if (statusText && statusDot) {
        if (gestureFrame.running) {
            statusText.textContent = 'AI đang phân tích sóng Wi-Fi...';
            statusDot.style.background = '#00d2ff';
            statusDot.style.boxShadow = '0 0 10px #00d2ff';
        } else if (target && target.ip) {
            statusText.textContent = `Đã kết nối (${target.ip}) · Sẵn sàng`;
            statusDot.style.background = '#5ce3bd';
            statusDot.style.boxShadow = '0 0 8px #5ce3bd';
        } else if (target && target.mac) {
            statusText.textContent = `Đang chờ cấp IP cho ${target.mac}...`;
            statusDot.style.background = '#f59e0b';
            statusDot.style.boxShadow = '0 0 8px #f59e0b';
        } else {
            statusText.textContent = 'Đang chờ chọn thiết bị 5 GHz...';
            statusDot.style.background = '#95acc0';
            statusDot.style.boxShadow = 'none';
        }
    }

    const openLink = document.getElementById('link-open-8082');
    if (openLink) openLink.href = `http://${gestureHost()}:8082/`;
}

function postToGestureFrame(type) {
    const iframe = document.getElementById('v8082-iframe');
    if (!iframe || !iframe.contentWindow || !gestureFrame.key) return;
    try { iframe.contentWindow.postMessage({ type }, `http://${gestureHost()}:8082`); } catch (_) {}
}

function waitGestureStopped(timeoutMs = 5000) {
    if (!gestureFrame.running) return Promise.resolve();
    return new Promise(resolve => {
        const timer = setTimeout(done, timeoutMs);
        function done() { clearTimeout(timer); resolve(); }
        gestureFrame.stopWaiters.push(done);
    });
}

window.addEventListener('message', async event => {
    let origin;
    try { origin = new URL(event.origin); } catch (_) { return; }
    if (origin.hostname !== gestureHost() || origin.port !== '8082') return;
    const data = event.data || {};

    // Handover thông minh: Chỉ dừng việc thu của tab Tổng quan khi người dùng thực sự bấm bắt đầu thu ở Bước 1
    if (data.type === 'WAKE_USER_START' || (data.type === 'WAKE_STATE' && data.running && !gestureFrame.running)) {
        gestureFrame.running = true;
        if (cfrState.running) {
            gestureFrame.resumeOverview = null;
            await stopCfrCapture(false);
        }
        updateGestureToolbar();
    } else if (data.type === 'WAKE_USER_STOP' || (data.type === 'WAKE_STATE' && !data.running && gestureFrame.running)) {
        gestureFrame.running = false;
        const waiters = gestureFrame.stopWaiters.splice(0);
        waiters.forEach(fn => fn());
        updateGestureToolbar();
    } else if (data.type === 'WAKE_STATE') {
        gestureFrame.running = Boolean(data.running);
        if (!gestureFrame.running) {
            const waiters = gestureFrame.stopWaiters.splice(0);
            waiters.forEach(fn => fn());
        }
        updateGestureToolbar();
    }
});

async function enterGestureView() {
    updateGestureToolbar();
    populateGestureDeviceSelect();
    const iframe = document.getElementById('v8082-iframe');
    const empty = document.getElementById('v8082-empty');
    const target = gestureTarget();
    if (!iframe) return;

    if (!target || !target.ip) {
        if (empty) {
            empty.hidden = false;
            const detail = document.getElementById('v8082-empty-detail');
            if (detail) detail.textContent = target
                ? `Thiết bị ${target.mac} chưa có địa chỉ IPv4. Chọn một thiết bị 5 GHz khác từ danh sách ở trên.`
                : 'Chọn một điện thoại 5 GHz trong danh sách thiết bị ở trên để bắt đầu trải nghiệm.';
        }
        return;
    }
    if (empty) empty.hidden = true;

    // Không dừng tab Tổng quan ngay tại đây! Tab Tổng quan vẫn chạy cho tới khi bấm Bắt đầu trải nghiệm ở Bước 1.
    const key = `${target.mac}|${target.ip}`;
    if (gestureFrame.key && gestureFrame.key !== key) {
        postToGestureFrame('WAKE_STOP');
        await waitGestureStopped();
        if (activeView !== 'gesture_v') return;
    }
    if (gestureFrame.key !== key || !iframe.src || iframe.src === 'about:blank') {
        gestureFrame.key = key;
        gestureFrame.paused = true;
        iframe.src = `http://${gestureHost()}:8082/?embed=1&mac=${encodeURIComponent(target.mac)}&ip=${encodeURIComponent(target.ip)}`;
    }
    updateGestureToolbar();
}

async function leaveGestureView() {
    if (!gestureFrame.key) return;
    // Rời tab Cử chỉ chữ V: dừng thu tín hiệu của model 8082
    postToGestureFrame('WAKE_STOP');
    gestureFrame.resumeOverview = null;
    await waitGestureStopped();

    // Tuyệt đối không tự động bắt đầu thu khi quay lại tab Tổng quan.
    // Nếu có tiến trình thu nào đang chạy dở, dừng hẳn lại để giữ trạng thái nghỉ (idle).
    if (cfrState.running) {
        await stopCfrCapture(false);
    }
}

async function fetchV6Devices(forceRefresh = false) {
    try {
        const res = await fetch('/api/v6/devices');
        if (!res.ok) return;
        const devices = await res.json();
        v6State.devices = Array.isArray(devices) ? devices : [];

        // Hợp nhất với danh sách trạm từ currentProberConfig nếu có
        if (currentProberConfig && Array.isArray(currentProberConfig.clients)) {
            for (const pc of currentProberConfig.clients) {
                if (!pc.mac) continue;
                const exists = v6State.devices.find(d => d.mac && d.mac.toLowerCase() === pc.mac.toLowerCase());
                if (!exists) {
                    v6State.devices.push({ mac: pc.mac, ip: pc.ip || '', is_5ghz: pc.is_5ghz });
                } else if (pc.ip && !exists.ip) {
                    exists.ip = pc.ip;
                }
            }
        }

        if (domElements.v6DeviceSelect) {
            const currentSelected = domElements.v6DeviceSelect.value || v6State.mac || cfrState.target_mac || cfrState.targetMac || '';
            domElements.v6DeviceSelect.innerHTML = '';
            
            if (v6State.devices.length === 0) {
                const emptyOpt = document.createElement('option');
                emptyOpt.value = '';
                emptyOpt.textContent = 'Chưa phát hiện thiết bị (Bấm Tìm thiết bị)';
                domElements.v6DeviceSelect.appendChild(emptyOpt);
            } else {
                for (const d of v6State.devices) {
                    const opt = document.createElement('option');
                    opt.value = d.mac;
                    opt.dataset.ip = d.ip || '';
                    const label = `${d.ip ? d.ip + ' · ' : ''}${d.mac}${d.is_5ghz ? ' (5 GHz)' : ''}`;
                    opt.textContent = label;
                    domElements.v6DeviceSelect.appendChild(opt);
                }
            }

            // Đồng bộ thiết bị đã chọn trước đó
            if (currentSelected) {
                syncTargetDeviceToV6(currentSelected, v6State.ip || cfrState.target_ip || cfrState.targetIp);
            } else if (v6State.devices.length > 0) {
                const first5g = v6State.devices.find(d => d.is_5ghz) || v6State.devices[0];
                syncTargetDeviceToV6(first5g.mac, first5g.ip);
            }
        }
    } catch (e) {
        console.warn('Lỗi tải danh sách thiết bị V6:', e);
    }
}

async function startV6Capture() {
    if (v6State.running || cfrState.running) {
        v6State.running = true;
        updateV6UIControls(true);
        return;
    }

    let mac = (domElements.v6DeviceSelect ? domElements.v6DeviceSelect.value : '') || v6State.mac || cfrState.target_mac || cfrState.targetMac || '';
    let ip = (domElements.v6IpInput ? domElements.v6IpInput.value.trim() : '') || v6State.ip || cfrState.target_ip || cfrState.targetIp || '';

    if (!mac) {
        showV6Notice('Vui lòng chọn thiết bị trước khi bắt đầu', true);
        return;
    }

    await startCfrCapture({ mac, ip });
}

async function stopV6Capture(clearMaster = true) {
    if (clearMaster) {
        v6State.masterActive = false;
    }
    stopV6Polling();
    v6State.running = false;
    updateV6UIControls(false);
    if (cfrState.running) {
        await stopCfrCapture(clearMaster);
    }
    renderV({ state: 'READY', stroke_progress: 0, stroke_fraction: 0, score: 0, label: 0 });
}

function startV6Polling() {
    stopV6Polling();
    // Đã dùng luồng SSE /events duy nhất đẩy v6_state thời gian thực,
    // không khởi động polling HTTP 60ms để tránh xung đột lock mutex và nghẽn tài nguyên router.
}

function stopV6Polling() {
    if (v6State.pollTimer) {
        clearInterval(v6State.pollTimer);
        v6State.pollTimer = null;
    }
}

async function pollV6Score() {
    // Không thực hiện request HTTP - toàn bộ dữ liệu V6 được nhận qua luồng SSE thời gian thực
    return;
}

function initV6Controls() {
    const btnResetGesture = document.getElementById('btnResetGesture');
    if (btnResetGesture) {
        btnResetGesture.addEventListener('click', async () => {
            btnResetGesture.classList.add('loading');
            try {
                await fetch('/api/v6/reset', { method: 'POST' });
            } catch (err) {
                console.warn('Reset gesture error:', err);
            }
            const strokeDown = document.getElementById('vStrokeDown');
            const strokeUp = document.getElementById('vStrokeUp');
            const pointSvg = document.getElementById('vPointSvg');
            const stateLabel = document.getElementById('v6StateLabel');
            if (strokeDown) strokeDown.style.strokeDashoffset = '147px';
            if (strokeUp) strokeUp.style.strokeDashoffset = '147px';
            if (pointSvg) pointSvg.classList.remove('active');
            if (stateLabel) {
                stateLabel.textContent = currentLang === 'vi' ? 'Sẵn sàng' : 'Ready';
                stateLabel.className = 'v6-status-text ready';
            }
            if (domElements.m5rScoreDisplay) domElements.m5rScoreDisplay.textContent = '0.0000';
            if (domElements.m5rLabelChip) {
                domElements.m5rLabelChip.className = 'motion-chip chip-still';
                domElements.m5rLabelChip.textContent = currentLang === 'vi' ? '0 · TĨNH' : '0 · STILL';
            }
            if (domElements.m5rStageDisplay) {
                domElements.m5rStageDisplay.textContent = currentLang === 'vi' ? 'SẴN SÀNG' : 'READY';
            }
            setTimeout(() => btnResetGesture.classList.remove('loading'), 300);
        });
    }

    if (domElements.v6StartBtn) {
        domElements.v6StartBtn.addEventListener('click', startV6Capture);
    }
    if (domElements.v6StopBtn) {
        domElements.v6StopBtn.addEventListener('click', () => stopV6Capture(true));
    }
    if (domElements.v6RefreshBtn) {
        domElements.v6RefreshBtn.addEventListener('click', () => {
            fetchV6Devices(true);
        });
    }
    if (domElements.v6DeviceSelect) {
        domElements.v6DeviceSelect.addEventListener('change', () => {
            const sel = domElements.v6DeviceSelect;
            const opt = sel.options[sel.selectedIndex];
            if (opt) {
                const mac = opt.value;
                const ip = opt.dataset.ip || '';
                syncTargetDeviceToV6(mac, ip);
            }
        });
    }
    if (domElements.v6IpInput) {
        domElements.v6IpInput.addEventListener('input', () => {
            v6State.ip = domElements.v6IpInput.value.trim();
        });
    }

    // Thiết lập và khởi tạo bộ nút chuyển chế độ chống nhiễu người ở gần
    const sensButtons = document.querySelectorAll('.v6-sens-btn');
    if (sensButtons.length > 0) {
        sensButtons.forEach(btn => {
            btn.addEventListener('click', async () => {
                sensButtons.forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                const high = parseFloat(btn.dataset.high) || 0.80;
                const low = parseFloat(btn.dataset.low) || 0.65;
                const mode = btn.dataset.mode || 'anti_noise';
                try {
                    localStorage.setItem('v6_sens_mode', mode);
                    await fetch('/api/v6/sensitivity', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ threshold_high: high, threshold_low: low })
                    });
                } catch (_) {}
                const teleThres = document.getElementById('v6-tele-threshold');
                if (teleThres) {
                    teleThres.textContent = `${high.toFixed(2)} / ${low.toFixed(2)}`;
                }
            });
        });

        // Khôi phục lựa chọn đã lưu hoặc mặc định anti_noise (0.80 / 0.65)
        const savedMode = localStorage.getItem('v6_sens_mode') || 'anti_noise';
        const targetBtn = Array.from(sensButtons).find(b => b.dataset.mode === savedMode) || sensButtons[1];
        if (targetBtn) {
            sensButtons.forEach(b => b.classList.remove('active'));
            targetBtn.classList.add('active');
            const high = parseFloat(targetBtn.dataset.high) || 0.80;
            const low = parseFloat(targetBtn.dataset.low) || 0.65;
            fetch('/api/v6/sensitivity', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ threshold_high: high, threshold_low: low })
            }).catch(() => {});
            const teleThres = document.getElementById('v6-tele-threshold');
            if (teleThres) {
                teleThres.textContent = `${high.toFixed(2)} / ${low.toFixed(2)}`;
            }
        }
    }
    // Bộ nút nạp lại giao diện 8082
    const btnReload8082 = document.getElementById('btn-reload-8082');
    if (btnReload8082) {
        btnReload8082.addEventListener('click', () => {
            gestureFrame.key = '';
            if (activeView === 'gesture_v') enterGestureView();
        });
    }
    const btnPause8082 = document.getElementById('btn-pause-8082');
    if (btnPause8082) {
        btnPause8082.addEventListener('click', () => {
            gestureFrame.paused = !gestureFrame.paused;
            postToGestureFrame(gestureFrame.paused ? 'WAKE_STOP' : 'WAKE_START');
            updateGestureToolbar();
        });
    }

    // Khôi phục thiết bị đã chọn từ localStorage khi khởi động
    try {
        const savedMac = localStorage.getItem('edge_csi_selected_mac');
        const savedIp = localStorage.getItem('edge_csi_selected_ip');
        if (savedMac) {
            v6State.mac = savedMac;
            v6State.ip = savedIp || '';
            cfrState.targetMac = savedMac;
            cfrState.targetIp = savedIp || '';
            syncTargetDeviceToV6(savedMac, savedIp);
        }
    } catch (_) {}
}

async function setView(view) {
    const viewDict = i18n[currentLang]?.views || i18n.vi.views;
    if (!viewDict[view]) return;
    const previousView = activeView;
    activeView = view;
    document.querySelectorAll('.tab-btn').forEach(button => {
        button.classList.toggle('active', button.dataset.view === view);
    });
    document.getElementById('page-title').textContent = viewDict[view].title;

    const cfrPanel = document.getElementById('cfr-panel');
    const proberPanel = document.getElementById('prober-panel');
    const metricGrid = document.querySelector('.metric-grid');
    const chartPanel = document.getElementById('chart-panel');
    const gestureVPanel = document.getElementById('gesture-v-panel');

    const overviewTechPanel = document.getElementById('overview-tech-panel');
    if (view === 'gesture_v') {
        if (cfrState.running) {
            await stopCfrCapture(false);
        }
        if (cfrPanel) { cfrPanel.hidden = true; cfrPanel.style.display = 'none'; }
        if (proberPanel) { proberPanel.hidden = true; proberPanel.style.display = 'none'; }
        if (metricGrid) { metricGrid.hidden = true; metricGrid.style.display = 'none'; }
        if (chartPanel) { chartPanel.hidden = true; chartPanel.style.display = 'none'; }
        if (overviewTechPanel) { overviewTechPanel.hidden = true; overviewTechPanel.style.display = 'none'; }
        if (gestureVPanel) { gestureVPanel.hidden = false; gestureVPanel.style.display = 'flex'; }

        // Nhúng cổng 8082 với thiết bị đang chọn ở tab Tổng quan (không cần chọn lại trong 8082)
        enterGestureView();
    } else {
        if (previousView === 'gesture_v') await leaveGestureView();
        if (cfrPanel) { cfrPanel.hidden = false; cfrPanel.style.display = ''; }
        if (proberPanel) { proberPanel.hidden = false; proberPanel.style.display = ''; }
        if (metricGrid) { metricGrid.hidden = false; metricGrid.style.display = ''; }
        if (chartPanel) { chartPanel.hidden = false; chartPanel.style.display = ''; }
        if (overviewTechPanel) { overviewTechPanel.hidden = false; overviewTechPanel.style.display = ''; }
        if (gestureVPanel) { gestureVPanel.hidden = true; gestureVPanel.style.display = 'none'; }
        document.getElementById('chart-kicker').textContent = viewDict[view].kicker;
        document.getElementById('chart-title').textContent = viewDict[view].chart;
    }

    const chainCtrl = document.getElementById('chain-control');
    if (chainCtrl) chainCtrl.hidden = true;
    scheduleRender();
}

document.querySelectorAll('.tab-btn').forEach(button => {
    button.addEventListener('click', () => setView(button.dataset.view));
});

// Lắng nghe thay đổi trên bộ chọn thiết bị của Tab Demo Wi-Fi Sensing
const gestureDeviceSelect = document.getElementById('gesture-device-select');
if (gestureDeviceSelect) {
    gestureDeviceSelect.addEventListener('change', () => {
        const mac = gestureDeviceSelect.value;
        if (!mac || !currentProberConfig || !Array.isArray(currentProberConfig.clients)) return;
        const c = currentProberConfig.clients.find(item => item.mac && item.mac.toLowerCase() === mac.toLowerCase());
        if (c) {
            currentProberConfig.target_ip = c.ip || c.mac;
            cfrState.targetMac = c.mac;
            cfrState.targetIp = c.ip || '';
            v6State.mac = c.mac;
            v6State.ip = c.ip || '';
            v6State.hostname = c.hostname || '';
            syncTargetDeviceToV6(c.mac, c.ip || '', c.hostname || '');
            renderProberUI(currentProberConfig, true);
            if (activeView === 'gesture_v') {
                enterGestureView();
            }
        }
    });
}

const gestureRefreshBtn = document.getElementById('gesture-refresh-btn');
if (gestureRefreshBtn) {
    gestureRefreshBtn.addEventListener('click', () => {
        fetchProberData(true);
    });
}

chainSelect.addEventListener('change', scheduleRender);
calibrateButton.addEventListener('click', requestCalibration);

function getTargetMacForDistance() {
    let targetMac = cfrState.targetMac || cfrState.target_mac || '';
    if (!targetMac && currentProberConfig && currentProberConfig.clients && currentProberConfig.clients.length > 0) {
        const c5 = currentProberConfig.clients.find(c => c.is_5ghz) || currentProberConfig.clients[0];
        if (c5) targetMac = c5.mac;
    }
    return targetMac || 'auto';
}

function syncDistanceEnvUI(envMode, nVal) {
    if (domElements.envTabLoS && domElements.envTabStandard && domElements.envTabNLoS) {
        domElements.envTabLoS.classList.toggle('active', envMode === 'los');
        domElements.envTabStandard.classList.toggle('active', envMode === 'standard');
        domElements.envTabNLoS.classList.toggle('active', envMode === 'nlos');
    }
    if (domElements.distNInput && document.activeElement !== domElements.distNInput && nVal !== undefined) {
        domElements.distNInput.value = Number(nVal).toFixed(2);
    }
}

function initDistanceControls() {
    const tabs = [
        { el: domElements.envTabLoS, env: 'los', defN: 1.75 },
        { el: domElements.envTabStandard, env: 'standard', defN: 2.15 },
        { el: domElements.envTabNLoS, env: 'nlos', defN: 3.00 }
    ];

    tabs.forEach(item => {
        if (!item.el) return;
        item.el.addEventListener('click', async () => {
            tabs.forEach(t => { if (t.el) t.el.classList.remove('active'); });
            item.el.classList.add('active');
            if (domElements.distNInput) {
                domElements.distNInput.value = item.defN.toFixed(2);
            }
            const targetMac = getTargetMacForDistance();
            try {
                await fetch('/api/distance/set_env', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ env: item.env, n: item.defN, mac: targetMac })
                });
            } catch (err) {
                console.error('Failed to set distance env:', err);
            }
        });
    });

    const sendNUpdate = async (nVal) => {
        const targetMac = getTargetMacForDistance();
        try {
            await fetch('/api/distance/set_n', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ n: nVal, mac: targetMac })
            });
        } catch (err) {
            console.error('Failed to set path loss exponent n:', err);
        }
    };

    if (domElements.distNDec) {
        domElements.distNDec.addEventListener('click', () => {
            let n = parseFloat(domElements.distNInput ? domElements.distNInput.value : '1.75') || 1.75;
            n = Math.max(1.0, Math.round((n - 0.05) * 100) / 100);
            if (domElements.distNInput) domElements.distNInput.value = n.toFixed(2);
            sendNUpdate(n);
        });
    }

    if (domElements.distNInc) {
        domElements.distNInc.addEventListener('click', () => {
            let n = parseFloat(domElements.distNInput ? domElements.distNInput.value : '1.75') || 1.75;
            n = Math.min(5.0, Math.round((n + 0.05) * 100) / 100);
            if (domElements.distNInput) domElements.distNInput.value = n.toFixed(2);
            sendNUpdate(n);
        });
    }

    if (domElements.distNInput) {
        domElements.distNInput.addEventListener('change', () => {
            let n = parseFloat(domElements.distNInput.value);
            if (isNaN(n) || n < 1.0) n = 1.0;
            if (n > 5.0) n = 5.0;
            domElements.distNInput.value = n.toFixed(2);
            sendNUpdate(n);
        });
    }
}
initDistanceControls();

if (domElements.distCalBtn) {
    domElements.distCalBtn.addEventListener('click', async () => {
        const promptMsg = currentLang === 'vi'
            ? 'Nhập khoảng cách thực tế từ router tới thiết bị (mét):\n(Nhập 0 hoặc "reset" để đặt lại về mặc định)'
            : 'Enter actual distance from router to device (meters):\n(Enter 0 or "reset" to reset to defaults)';
        const input = window.prompt(promptMsg, '1.5');
        if (!input) return;

        let targetMac = getTargetMacForDistance();

        const trimmed = input.trim().toLowerCase();
        if (trimmed === '0' || trimmed === 'reset') {
            try {
                const res = await fetch('/api/distance/reset_cal', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ mac: targetMac })
                });
                if (res.ok) {
                    alert(currentLang === 'vi' ? 'Đã đặt lại hiệu chuẩn khoảng cách về mặc định!' : 'Distance calibration reset to default!');
                }
            } catch (e) {
                console.error(e);
            }
            return;
        }

        const distM = parseFloat(trimmed);
        if (isNaN(distM) || distM < 0.2 || distM > 25.0) {
            alert(currentLang === 'vi' ? 'Khoảng cách không hợp lệ! Vui lòng nhập từ 0.2 đến 25 mét.' : 'Invalid distance! Please enter between 0.2 and 25 meters.');
            return;
        }

        const customN = parseFloat(domElements.distNInput ? domElements.distNInput.value : '1.75') || 1.75;
        try {
            const res = await fetch('/api/distance/calibrate', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ distance_m: distM, mac: targetMac, n: customN })
            });
            if (res.ok) {
                const data = await res.json();
                alert(currentLang === 'vi'
                    ? `Hiệu chuẩn thành công tại ${distM} m!\nThiết bị: ${data.mac || targetMac}\nA1m = ${data.applied_a_1m} dBm · n = ${data.applied_n || customN} (${data.band})`
                    : `Calibrated successfully at ${distM} m!\nDevice: ${data.mac || targetMac}\nA1m = ${data.applied_a_1m} dBm · n = ${data.applied_n || customN} (${data.band})`);
            } else {
                const err = await res.json();
                alert(`Lỗi: ${err.error || 'Calibration failed'}`);
            }
        } catch (e) {
            console.error(e);
            alert(`Lỗi kết nối API: ${e.message}`);
        }
    });
}

if (domElements.stabilityTechToggleBtn && domElements.stabilityTechDetails) {
    domElements.stabilityTechToggleBtn.addEventListener('click', () => {
        const isHidden = domElements.stabilityTechDetails.hidden;
        domElements.stabilityTechDetails.hidden = !isHidden;
        if (domElements.techToggleText) {
            domElements.techToggleText.textContent = isHidden
                ? (currentLang === 'vi' ? '[-] Thu gọn' : '[-] Collapse')
                : (currentLang === 'vi' ? '[+] Chi tiết kỹ thuật' : '[+] Tech details');
        }
    });
}

if (domElements.gestureModelSelect) {
    domElements.gestureModelSelect.addEventListener('change', async (e) => {
        const chosen = e.target.value;
        try {
            const res = await fetch('/api/model/select', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ model: chosen })
            });
            const data = await res.json();
            if (res.ok && data.status === 'ok') {
                if (domElements.vTeleActiveModel) {
                    domElements.vTeleActiveModel.textContent = chosen;
                }
                const msg = currentLang === 'vi'
                    ? `Đã chuyển sang mô hình: ${chosen}`
                    : `Switched to model: ${chosen}`;
                if (typeof showToast === 'function') {
                    showToast(msg, 'success');
                } else {
                    console.log(msg);
                }
            } else {
                alert(currentLang === 'vi' ? 'Không thể chuyển đổi mô hình!' : 'Failed to switch model!');
            }
        } catch (err) {
            console.error('Model switch request failed:', err);
        }
    });
}

async function requestCalibration() {
    calibrateButton.disabled = true;
    const previousLabel = calibrateButton.textContent;
    calibrateButton.textContent = currentLang === 'vi' ? 'Đang yêu cầu…' : 'Requesting…';
    try {
        const response = await fetch('/api/calibrate', { method: 'POST' });
        if (!response.ok) {
            const details = await response.text();
            throw new Error(`Calibration request failed: ${details}`);
        }
        motionHistory = [];
        const pv = document.getElementById('presence-value');
        if (pv) pv.textContent = t('presence_calibrating');
        const pc = document.getElementById('presence-caption');
        if (pc) pc.textContent = t('caption_calibrating');
        const mv = document.getElementById('motion-value');
        if (mv) mv.textContent = '—';
        const mc = document.getElementById('motion-caption');
        if (mc) mc.textContent = currentLang === 'vi' ? 'Đang khởi tạo cửa sổ 30 frame' : 'Initializing 30-frame window';
        setStatus('waiting', t('status_calibrating'));
        scheduleRender();
    } catch (error) {
        console.error('Unable to restart motion calibration', error);
        setStatus('offline', 'Error requesting calibration');
    } finally {
        calibrateButton.textContent = previousLabel;
        calibrateButton.disabled = false;
    }
}

function updateChainOptions(frame) {
    if (!chainSelect || !frame || !Array.isArray(frame.chains) || frame.chains.length === 0) return;
    const selected = Number(chainSelect.value) || 0;
    if (chainSelect.options.length === frame.chains.length) return;
    if (typeof chainSelect.replaceChildren === 'function') {
        chainSelect.replaceChildren();
    } else {
        chainSelect.innerHTML = '';
    }
    frame.chains.forEach((_, index) => {
        const option = document.createElement('option');
        option.value = String(index);
        option.textContent = `Chain ${index}`;
        chainSelect.append(option);
    });
    chainSelect.value = String(Math.min(selected, Math.max(frame.chains.length - 1, 0)));
}

function acceptFrame(frame) {
    if (!frame || !Array.isArray(frame.chains) || frame.chains.length === 0) {
        if (!latestFrame) {
            setStatus('waiting', t('caption_waiting_csi'));
        }
        return;
    }
    const isNewFrame = !latestFrame ||
        frame.timestamp_ns !== latestFrame.timestamp_ns ||
        frame.phy_ppdu_id !== latestFrame.phy_ppdu_id;
    latestFrame = frame;
    captureStartedAt = 0;
    autoWakeAttempts = 0;
    updateChainOptions(frame);

    const now = performance.now();
    if (isNewFrame) {
        lastFrameAt = now;
        receiveTimes.push(now);
        if (receiveTimes.length > 20) receiveTimes.shift();
        if (receiveTimes.length > 1) {
            const intervals = receiveTimes.slice(1).map((time, index) => time - receiveTimes[index]);
            const average = intervals.reduce((sum, value) => sum + value, 0) / intervals.length;
            frameRate = average > 0 ? 1000 / average : 0;
        }

        const score = Number(frame.motion_score) || 0;
        motionHistory.push({ time: now, score: score });
        if (motionHistory.length > 180) motionHistory.shift();
        breathWaveHistory.push(Number(frame.breath_wave) || 0);
        if (breathWaveHistory.length > 240) breathWaveHistory.shift();
    }

    if (activeView === 'gesture_v' && isNewFrame) {
        renderV(frame);
        updateGestureVUI(frame);
    }

    if (now - lastDomUpdateAt >= 100 || isNewFrame && !lastDomUpdateAt) {
        lastDomUpdateAt = now;
        updateDomMetrics(frame, isNewFrame, now);
    }

    scheduleRender();
}

function connectStream() {
    if (!('EventSource' in window)) {
        setStatus('offline', 'Browser does not support EventSource');
        return;
    }
    const source = new EventSource('/events');
    let consecutiveJsonErrors = 0;
    source.addEventListener('csi', event => {
        let frame = null;
        try {
            frame = JSON.parse(event.data);
            consecutiveJsonErrors = 0;
        } catch (error) {
            consecutiveJsonErrors++;
            console.warn(`[CSI] Frame JSON parse error (#${consecutiveJsonErrors}):`, error, event.data ? event.data.slice(0, 150) : '');
            if (consecutiveJsonErrors > 10) {
                setStatus('offline', 'Invalid payload format');
            }
            return;
        }
        try {
            acceptFrame(frame);
        } catch (err) {
            console.error('Error rendering CSI frame in UI:', err);
        }
    });
    source.onopen = () => {
        if (!latestFrame) setStatus('waiting', t('status_connected'));
    };
    source.onerror = () => {
        if (!latestFrame) setStatus('offline', 'Connection error');
        else setStatus('waiting', t('status_disrupted'));
    };
}

function resizeCanvas() {
    const bounds = canvas.getBoundingClientRect();
    const ratio = Math.max(1, window.devicePixelRatio || 1);
    const width = Math.max(300, Math.floor(bounds.width * ratio));
    const height = Math.max(180, Math.floor(bounds.height * ratio));
    if (canvas.width !== width || canvas.height !== height) {
        canvas.width = width;
        canvas.height = height;
    }
    scheduleRender();
}

function drawCurrent() {
    const width = canvas.width;
    const height = canvas.height;
    context.clearRect(0, 0, width, height);

    if (!latestFrame || !Array.isArray(latestFrame.chains) || latestFrame.chains.length === 0) {
        emptyOverlay.hidden = false;
        setLegend([]);
        return;
    }
    emptyOverlay.hidden = true;

    const chainIndex = Math.min(Number(chainSelect ? chainSelect.value : 0) || 0, latestFrame.chains.length - 1);
    const chain = latestFrame.chains[chainIndex] || latestFrame.chains[0];

    if (activeView === 'overview') {
        const values = motionHistory.map(entry => entry.score);
        const isCalibrated = Boolean(latestFrame.motion_calibrated);

        if (values.length === 0) {
            const ratio = Math.max(1, window.devicePixelRatio || 1);
            const left = 62 * ratio;
            const right = 16 * ratio;
            const top = 20 * ratio;
            const bottom = 34 * ratio;
            const plotHeight = height - top - bottom;
            context.strokeStyle = 'rgba(129, 150, 181, 0.16)';
            context.lineWidth = 1 * ratio;
            for (let i = 0; i <= 4; ++i) {
                const y = top + (plotHeight * i) / 4;
                context.beginPath();
                context.moveTo(left, y);
                context.lineTo(width - right, y);
                context.stroke();
            }
            context.save();
            context.fillStyle = 'rgba(143, 160, 183, 0.7)';
            context.font = `${13 * ratio}px "DM Mono", monospace`;
            context.textAlign = 'center';
            context.textBaseline = 'middle';
            context.fillText(
                currentLang === 'vi'
                    ? `Đang tích lũy tương quan (${latestFrame.motion_window_samples || 0}/30 frame)...`
                    : `Buffering correlation (${latestFrame.motion_window_samples || 0}/30 frames)...`,
                (left + width - right) / 2,
                top + plotHeight / 2
            );
            context.restore();
        } else {
            drawLineChart(values, '#00e5ff', width, height, 'Motion score', false, []);
        }

        const legend = [
            { name: `Motion score: ${(Number(latestFrame.motion_score) || 0).toFixed(4)}`, color: '#00e5ff' }
        ];
        if (!isCalibrated) {
            legend.push({
                name: currentLang === 'vi' ? `Đang hiệu chuẩn (${latestFrame.motion_calibration_frames || 0}/40)` : `Calibrating (${latestFrame.motion_calibration_frames || 0}/40)`,
                color: '#8fa0b7'
            });
        }
        setLegend(legend);
    }
}

function drawLineChart(values, color, width, height, label, overlay = false, references = []) {
    if (!values || values.length === 0) return;
    const ratio = Math.max(1, window.devicePixelRatio || 1);
    const left = 62 * ratio;
    const right = 16 * ratio;
    const top = 20 * ratio;
    const bottom = 34 * ratio;
    const plotWidth = width - left - right;
    const plotHeight = height - top - bottom;

    let min = Math.min(...values);
    let max = Math.max(...values);
    if (!overlay && references.length) {
        references.forEach(ref => {
            if (Number.isFinite(ref.value)) {
                min = Math.min(min, ref.value);
                max = Math.max(max, ref.value);
            }
        });
    }
    if (min === max) {
        min -= 1;
        max += 1;
    }
    const range = (max - min) * 0.1 || 0.1;
    min -= range;
    max += range;

    if (!overlay) {
        context.strokeStyle = 'rgba(129, 150, 181, 0.16)';
        context.lineWidth = 1 * ratio;
        for (let i = 0; i <= 4; ++i) {
            const y = top + (plotHeight * i) / 4;
            context.beginPath();
            context.moveTo(left, y);
            context.lineTo(width - right, y);
            context.stroke();
        }

        // Vẽ các đường ngưỡng tham chiếu (Reference Threshold Lines)
        if (references && references.length) {
            references.forEach(ref => {
                if (Number.isFinite(ref.value) && ref.value >= min && ref.value <= max) {
                    const y = top + ((max - ref.value) / (max - min)) * plotHeight;
                    context.save();
                    context.strokeStyle = ref.color || 'rgba(255, 255, 255, 0.4)';
                    context.lineWidth = (ref.lineWidth || 1.8) * ratio;
                    if (ref.dashed !== false) {
                        context.setLineDash([8 * ratio, 6 * ratio]);
                    }
                    context.beginPath();
                    context.moveTo(left, y);
                    context.lineTo(width - right, y);
                    context.stroke();

                    // Nhãn ngưỡng hiển thị trực quan ngay trên đường vẽ
                    if (ref.label) {
                        context.fillStyle = ref.color || '#fff';
                        context.font = `bold ${12 * ratio}px "DM Mono", monospace`;
                        context.textAlign = 'right';
                        context.textBaseline = 'bottom';
                        context.fillText(`${ref.label}: ${ref.value.toFixed(4)}`, width - right, y - 3 * ratio);
                    }
                    context.restore();
                }
            });
        }
    }

    context.save();
    context.strokeStyle = color;
    context.lineWidth = 2.4 * ratio;
    context.shadowColor = color;
    context.shadowBlur = 6 * ratio;
    context.beginPath();
    values.forEach((value, index) => {
        const x = left + (values.length === 1 ? plotWidth / 2 : (index / (values.length - 1)) * plotWidth);
        const y = top + ((max - value) / (max - min)) * plotHeight;
        if (index === 0) context.moveTo(x, y);
        else context.lineTo(x, y);
    });
    context.stroke();
    context.restore();

    if (!overlay) {
        context.fillStyle = '#9cb0c9';
        context.font = `${12 * ratio}px "DM Mono", monospace`;
        context.textAlign = 'right';
        context.textBaseline = 'bottom';
        context.fillText(max.toPrecision(4), left - 8 * ratio, top + 10 * ratio);
        context.fillText(min.toPrecision(4), left - 8 * ratio, height - bottom);
    }
}

function setLegend(items) {
    const legend = document.getElementById('chart-legend');
    legend.replaceChildren();
    items.forEach(item => {
        const entry = document.createElement('span');
        entry.className = 'legend-item';
        const swatch = document.createElement('span');
        swatch.className = 'legend-swatch';
        swatch.style.background = item.color;
        const text = document.createElement('span');
        text.textContent = item.name;
        entry.append(swatch, text);
        legend.append(entry);
    });
}

// ============================================================================
// CFR Capture Controller (ATH12K Hardware Capture & Background Daemon)
// ============================================================================
let cfrState = {
    running: false,
    target_mac: '',
    target_ip: '',
    iface: '',
    phy: 'phy00',
    hw: '',
    mac_dev: '',
    pid: -1,
    uptime_sec: 0,
    last_error: '',
    bandwidth: 0,
    latest_file: '',
    latest_file_size: 0,
    available_bandwidths: [0, 1, 2]
};

const cfrElements = {
    panel: document.getElementById('cfr-panel'),
    badge: document.getElementById('cfr-status-badge'),
    statusText: document.getElementById('cfr-status-text'),
    startBtn: document.getElementById('cfr-start-btn'),
    stopBtn: document.getElementById('cfr-stop-btn'),
    downloadBtn: document.getElementById('cfr-download-btn'),
    bandwidthSelect: document.getElementById('cfr-bandwidth-select'),
    bandwidthDisplay: document.getElementById('cfr-bandwidth-display'),
    bwHint: document.getElementById('cfr-bw-hint'),
    fileDisplay: document.getElementById('cfr-file-display'),
    targetDisplay: document.getElementById('cfr-target-display'),
    ifaceDisplay: document.getElementById('cfr-iface-display'),
    chipsetDisplay: document.getElementById('cfr-chipset-display'),
    pidDisplay: document.getElementById('cfr-pid-display'),
    uptimeDisplay: document.getElementById('cfr-uptime-display'),
    errorBanner: document.getElementById('cfr-error-banner')
};

function formatUptime(seconds) {
    const s = Number(seconds) || 0;
    const mins = Math.floor(s / 60);
    const secs = s % 60;
    if (mins >= 60) {
        const hrs = Math.floor(mins / 60);
        const remMins = mins % 60;
        return `${String(hrs).padStart(2, '0')}:${String(remMins).padStart(2, '0')}:${String(secs).padStart(2, '0')}`;
    }
    return `${String(mins).padStart(2, '0')}:${String(secs).padStart(2, '0')}`;
}

const bwMap = { 0: '20 MHz', 1: '40 MHz', 2: '80 MHz', 3: '160 MHz', 4: '320 MHz' };

function updateAvailableBandwidths(targetClient, targetCard) {
    if (!cfrElements.bandwidthSelect) return;

    if (!targetClient && !targetCard && currentProberConfig) {
        if (Array.isArray(currentProberConfig.clients) && currentProberConfig.clients.length > 0) {
            if (currentProberConfig.target_ip && currentProberConfig.target_ip !== 'auto') {
                targetClient = currentProberConfig.clients.find(c => c.ip === currentProberConfig.target_ip || c.mac === currentProberConfig.target_ip);
            }
            if (!targetClient) targetClient = currentProberConfig.clients[0];
        }
        if (targetClient && Array.isArray(currentProberConfig.cards)) {
            targetCard = currentProberConfig.cards.find(c => c.iface === targetClient.iface);
        }
        if (!targetCard && Array.isArray(currentProberConfig.cards) && currentProberConfig.cards.length > 0) {
            targetCard = currentProberConfig.cards[0];
        }
    }

    let supported = [0];
    if (targetClient && Array.isArray(targetClient.supported_bandwidths) && targetClient.supported_bandwidths.length > 0) {
        supported = targetClient.supported_bandwidths;
    } else if (targetCard && Array.isArray(targetCard.supported_bandwidths) && targetCard.supported_bandwidths.length > 0) {
        supported = targetCard.supported_bandwidths;
    } else if (Array.isArray(cfrState.available_bandwidths) && cfrState.available_bandwidths.length > 0) {
        supported = cfrState.available_bandwidths;
    } else {
        const band = targetClient?.band || targetCard?.band || '';
        supported = (band === '2.4GHz') ? [0] : [0, 1, 2];
    }

    let currentVal = parseInt(cfrElements.bandwidthSelect.value, 10);
    if (isNaN(currentVal) || !supported.includes(currentVal)) {
        currentVal = supported[supported.length - 1] ?? 0;
        cfrElements.bandwidthSelect.value = String(currentVal);
        cfrState.bandwidth = currentVal;
    }

    // 1. Native select options: strictly clean text (e.g. "20 MHz", "40 MHz") without clutter
    Array.from(cfrElements.bandwidthSelect.options).forEach(opt => {
        const val = parseInt(opt.value, 10);
        const isSupported = supported.includes(val);
        opt.disabled = !isSupported;
        opt.textContent = bwMap[val] || (val + ' code');
    });

    // 2. Interactive Bandwidth Chips
    const chips = document.querySelectorAll('.cfr-bw-chip');
    chips.forEach(chip => {
        const val = parseInt(chip.dataset.bw, 10);
        const isSupported = supported.includes(val);
        const isSelected = (val === currentVal);

        chip.classList.toggle('unsupported', !isSupported);
        chip.classList.toggle('active', isSelected);
        chip.classList.toggle('capturing', isSelected && cfrState.running);
        chip.disabled = !isSupported || (cfrState.running && !isSelected);

        if (!isSupported) {
            chip.title = (currentLang === 'vi') ? 'Băng thông không được hỗ trợ bởi card hoặc thiết bị này' : 'Bandwidth not supported by card or client device';
        } else {
            chip.title = bwMap[val] || '';
        }
    });

    // 3. Available Bandwidths Hint Badge
    if (cfrElements.bwHint) {
        const availNames = supported.map(b => bwMap[b]).join(' · ');
        const textSpan = cfrElements.bwHint.querySelector('.bw-hint-text');
        if (textSpan) {
            textSpan.textContent = `${t('bw_available_prefix')}: ${availNames}`;
        } else {
            cfrElements.bwHint.textContent = `${t('bw_available_prefix')}: ${availNames}`;
        }
        cfrElements.bwHint.className = 'cfr-bw-hint' + (supported.length === 1 ? ' warning' : '');
    }

    if (cfrElements.bandwidthDisplay) {
        cfrElements.bandwidthDisplay.textContent = bwMap[currentVal] || '20 MHz';
    }
}

function renderCfrUI() {
    if (!cfrElements.badge) return;

    if (cfrState.running) {
        cfrElements.badge.className = 'badge-active';
        cfrElements.statusText.textContent = t('cfr_status_running');
        if (cfrElements.startBtn) cfrElements.startBtn.disabled = true;
        if (cfrElements.stopBtn) cfrElements.stopBtn.disabled = false;
    } else if (cfrState.last_error) {
        cfrElements.badge.className = 'badge-error';
        cfrElements.statusText.textContent = t('cfr_status_error');
        if (cfrElements.startBtn) cfrElements.startBtn.disabled = false;
        if (cfrElements.stopBtn) cfrElements.stopBtn.disabled = true;
        updateGestureVUI({ gesture_stage: 'IDLE', gesture_reason: 'not_capturing' });
    } else {
        cfrElements.badge.className = 'badge-idle';
        cfrElements.statusText.textContent = t('cfr_status_idle');
        if (cfrElements.startBtn) cfrElements.startBtn.disabled = false;
        if (cfrElements.stopBtn) cfrElements.stopBtn.disabled = true;
        updateGestureVUI({ gesture_stage: 'IDLE', gesture_reason: 'not_capturing' });
    }

    // Bandwidth display & select state
    const bwMap = { 0: '20 MHz', 1: '40 MHz', 2: '80 MHz', 3: '160 MHz', 4: '320 MHz' };
    if (cfrElements.bandwidthDisplay) {
        cfrElements.bandwidthDisplay.textContent = bwMap[cfrState.bandwidth] || (cfrState.bandwidth + ' code');
    }
    if (cfrElements.bandwidthSelect) {
        cfrElements.bandwidthSelect.disabled = cfrState.running;
        if (cfrState.running) {
            cfrElements.bandwidthSelect.value = String(cfrState.bandwidth);
        }
    }
    document.querySelectorAll('.cfr-bw-chip').forEach(chip => {
        const chipVal = parseInt(chip.dataset.bw, 10);
        const isSelected = (chipVal === cfrState.bandwidth);
        chip.classList.toggle('active', isSelected);
        chip.classList.toggle('capturing', isSelected && cfrState.running);
        if (cfrState.running) {
            chip.disabled = !isSelected;
        } else {
            chip.disabled = chip.classList.contains('unsupported');
        }
    });

    // Download button & file display
    if (cfrElements.fileDisplay) {
        if (cfrState.latest_file) {
            const sizeMb = cfrState.latest_file_size ? (cfrState.latest_file_size / (1024 * 1024)).toFixed(1) + ' MB' : '';
            cfrElements.fileDisplay.textContent = cfrState.latest_file + (sizeMb ? ` (${sizeMb})` : '');
            cfrElements.fileDisplay.title = `${cfrState.latest_file} (${cfrState.latest_file_size} bytes)`;
        } else {
            cfrElements.fileDisplay.textContent = '—';
            cfrElements.fileDisplay.title = t('cfr_no_file');
        }
    }

    if (cfrElements.downloadBtn) {
        const hasFile = Boolean(cfrState.latest_file);
        cfrElements.downloadBtn.disabled = !hasFile;
        cfrElements.downloadBtn.title = hasFile ? `${t('download_btn_title')}: ${cfrState.latest_file}` : t('cfr_no_file');
    }

    // Target display
    if (cfrElements.targetDisplay) {
        if (cfrState.running && cfrState.target_mac) {
            const ipPart = cfrState.target_ip ? ` (${cfrState.target_ip})` : '';
            cfrElements.targetDisplay.textContent = `${cfrState.target_mac}${ipPart}`;
        } else {
            if (currentProberConfig.target_ip && currentProberConfig.target_ip !== 'auto') {
                const matched = Array.isArray(currentProberConfig.clients) ? currentProberConfig.clients.find(c => c.ip === currentProberConfig.target_ip) : null;
                if (matched) {
                    cfrElements.targetDisplay.textContent = `${matched.hostname || matched.ip} [${matched.mac}]`;
                } else {
                    cfrElements.targetDisplay.textContent = currentProberConfig.target_ip;
                }
            } else {
                cfrElements.targetDisplay.textContent = t('unselected_device');
            }
        }
    }

    if (cfrElements.ifaceDisplay) {
        cfrElements.ifaceDisplay.textContent = cfrState.iface || (cfrState.running ? 'phy00.1-0' : '—');
    }

    if (cfrElements.chipsetDisplay) {
        cfrElements.chipsetDisplay.textContent = cfrState.hw ? `${cfrState.hw} (${cfrState.mac_dev || 'mac'})` : 'VNPT Gateway (Wi-Fi 7)';
    }

    if (cfrElements.pidDisplay) {
        cfrElements.pidDisplay.textContent = cfrState.pid > 0 ? String(cfrState.pid) : '—';
    }

    if (cfrElements.uptimeDisplay) {
        cfrElements.uptimeDisplay.textContent = formatUptime(cfrState.uptime_sec);
    }

    if (cfrElements.errorBanner) {
        if (cfrState.last_error && !cfrState.running) {
            cfrElements.errorBanner.textContent = `[Lỗi] ${cfrState.last_error}`;
            cfrElements.errorBanner.hidden = false;
        } else {
            cfrElements.errorBanner.hidden = true;
        }
    }

    // Đồng bộ khối Cấu hình thu CSI của Model V6
    updateV6UIControls(v6State.running);
    if (domElements.v6SyncedTarget) {
        const displayMac = cfrState.target_mac || cfrState.targetMac;
        const displayIp = cfrState.target_ip || cfrState.targetIp;
        if (displayMac) {
            domElements.v6SyncedTarget.textContent = `${displayIp ? displayIp + ' · ' : ''}${displayMac}`;
        }
    }
}

async function fetchCfrStatus() {
    try {
        const res = await fetch('/api/cfr');
        if (!res.ok) return;
        const data = await res.json();
        const prevRunning = cfrState.running;
        cfrState.running = Boolean(data.running);
        cfrState.target_mac = data.target_mac || '';
        cfrState.target_ip = data.target_ip || '';
        cfrState.iface = data.iface || '';
        cfrState.phy = data.phy || 'phy00';
        cfrState.hw = data.hw || '';
        cfrState.mac_dev = data.mac_dev || '';
        cfrState.pid = Number(data.pid) || -1;
        cfrState.uptime_sec = Number(data.uptime_sec) || 0;
        cfrState.last_error = data.last_error || '';
        if (cfrState.running) {
            cfrState.bandwidth = Number(data.bandwidth) || 0;
        } else if (cfrElements.bandwidthSelect) {
            const userBw = parseInt(cfrElements.bandwidthSelect.value, 10);
            if (!isNaN(userBw)) {
                cfrState.bandwidth = userBw;
            }
        }
        cfrState.latest_file = data.latest_file || '';
        cfrState.latest_file_size = Number(data.latest_file_size) || 0;
        if (Array.isArray(data.available_bandwidths)) {
            cfrState.available_bandwidths = data.available_bandwidths;
            updateAvailableBandwidths();
        }

        renderCfrUI();

        if (prevRunning !== cfrState.running) {
            if (!cfrState.running) {
                latestFrame = null;
                frameRate = 0;
                receiveTimes = [];
                motionHistory = [];
                breathWaveHistory = [];
                setStatus('waiting', cfrState.last_error || t('caption_waiting_csi'));
                scheduleRender();
            }
            if (currentProberConfig.cards) {
                renderCardsTables(currentProberConfig.cards, currentProberConfig.clients, currentProberConfig.target_ip);
            }
        }
    } catch (e) {
        console.warn('Failed to fetch CFR status:', e);
    }
}

let captureStartedAt = 0;
let autoWakeAttempts = 0;

async function startCfrCapture(target = null) {
    if (cfrElements.startBtn) cfrElements.startBtn.disabled = true;

    // Đảm bảo thông tin mạng & danh sách trạm đã được nạp ít nhất 1 lần
    if (!currentProberConfig || !Array.isArray(currentProberConfig.clients) || currentProberConfig.clients.length === 0) {
        try {
            await loadProberStatus(false);
        } catch (_) {}
    }

    let mac = '';
    let ip = '';
    let iface = '';

    if (target && target.mac) {
        mac = target.mac;
        ip = target.ip || '';
        iface = target.iface || '';
    } else if (currentProberConfig.target_ip && currentProberConfig.target_ip !== 'auto') {
        const matched = Array.isArray(currentProberConfig.clients) ? currentProberConfig.clients.find(c => c.ip === currentProberConfig.target_ip || (c.mac && c.mac.toUpperCase() === currentProberConfig.target_ip.toUpperCase())) : null;
        if (matched) {
            mac = matched.mac;
            ip = matched.ip;
            iface = matched.iface;
        } else {
            ip = currentProberConfig.target_ip;
        }
    } else if (v6State.mac) {
        mac = v6State.mac;
        ip = v6State.ip;
    }

    // Bắt buộc người dùng phải chọn một thiết bị cụ thể trước khi thu (Không tự ý chọn thiết bị ma hoặc tự động thu)
    if (!mac) {
        if (cfrElements.errorBanner) {
            cfrElements.errorBanner.textContent = t('prompt_select_device');
            cfrElements.errorBanner.hidden = false;
        }
        if (proberElements.selectionBadge) {
            proberElements.selectionBadge.classList.add('pulse-warning');
            setTimeout(() => proberElements.selectionBadge && proberElements.selectionBadge.classList.remove('pulse-warning'), 2500);
        }
        const panel = document.getElementById('prober-panel');
        if (panel) {
            panel.scrollIntoView({ behavior: 'smooth', block: 'center' });
        }
        if (cfrElements.startBtn) cfrElements.startBtn.disabled = false;
        return;
    }

    // Kích hoạt chế độ master capture và đồng bộ sang Model V6
    v6State.masterActive = true;
    v6State.mac = mac;
    v6State.ip = ip;
    syncTargetDeviceToV6(mac, ip);

    let bandwidth = 0;
    if (target && target.bandwidth !== undefined) {
        bandwidth = target.bandwidth;
    } else if (cfrElements.bandwidthSelect) {
        bandwidth = parseInt(cfrElements.bandwidthSelect.value, 10) || 0;
    }
    const period = 10;
    const autoProbe = true;

    try {
        const res = await fetch('/api/cfr', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                action: 'start',
                mac: mac,
                ip: ip,
                iface: iface,
                bandwidth: bandwidth,
                period: period,
                auto_probe: autoProbe
            })
        });
        const data = await res.json();
        if (res.ok && data.status === 'ok') {
            cfrState.running = true;
            v6State.running = true;
            updateV6UIControls(true);
            captureStartedAt = performance.now();
            autoWakeAttempts = 0;
            cfrState.target_mac = data.target_mac || mac;
            cfrState.target_ip = data.target_ip || ip;
            cfrState.targetMac = cfrState.target_mac;
            cfrState.targetIp = cfrState.target_ip;
            cfrState.iface = data.iface || iface;
            cfrState.hw = data.hw || '';
            cfrState.pid = data.pid || -1;
            cfrState.uptime_sec = 0;
            cfrState.last_error = '';
            cfrState.bandwidth = Number(data.bandwidth) || bandwidth;
            if (data.latest_file) cfrState.latest_file = data.latest_file;
            if (data.latest_file_size) cfrState.latest_file_size = Number(data.latest_file_size);
            renderCfrUI();
            if (autoProbe) loadProberStatus();
        } else {
            cfrState.last_error = data.error || data.message || 'Lỗi khởi chạy capture';
            renderCfrUI();
        }
    } catch (err) {
        cfrState.last_error = String(err.message || err);
        renderCfrUI();
    } finally {
        if (cfrElements.startBtn) cfrElements.startBtn.disabled = cfrState.running;
        if (currentProberConfig.cards) {
            renderCardsTables(currentProberConfig.cards, currentProberConfig.clients, currentProberConfig.target_ip);
        }
    }
}

async function stopCfrCapture(clearMaster = true) {
    if (clearMaster) {
        v6State.masterActive = false;
    }
    if (cfrElements.stopBtn) cfrElements.stopBtn.disabled = true;
    try {
        const res = await fetch('/api/cfr', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'stop' })
        });
        const data = await res.json();
        if (res.ok) {
            cfrState.running = false;
            v6State.running = false;
            updateV6UIControls(false);
            renderV({ state: 'READY', stroke_progress: 0, stroke_fraction: 0, score: 0, label: 0 });
            cfrState.uptime_sec = 0;
            latestFrame = null;
            frameRate = 0;
            receiveTimes = [];
            motionHistory = [];
            breathWaveHistory = [];
            setStatus('waiting', t('caption_waiting_csi'));
            scheduleRender();
            await fetchCfrStatus();
            renderCfrUI();
            currentProberConfig.enabled = false;
            renderProberUI(currentProberConfig);
        }
    } catch (err) {
        console.warn('Failed to stop CFR capture:', err);
    } finally {
        if (cfrElements.stopBtn) cfrElements.stopBtn.disabled = !cfrState.running;
        if (cfrElements.startBtn) cfrElements.startBtn.disabled = cfrState.running;
        if (currentProberConfig.cards) {
            renderCardsTables(currentProberConfig.cards, currentProberConfig.clients, currentProberConfig.target_ip);
        }
    }
}

// ============================================================================
// Active Prober Controller (Grouped by Wi-Fi Card)
// ============================================================================
const proberElements = {
    selectionBadge: document.getElementById('prober-selection-badge'),
    quickAutoBtn: document.getElementById('quick-auto-btn'),
    refreshBtn: document.getElementById('prober-refresh-btn'),
    cardsContainer: document.getElementById('prober-cards-container')
};

// currentProberConfig is initialized at the top of file

function getSignalClass(snrStr, signalStr) {
    let snr = -1;
    if (snrStr && snrStr !== '—') {
        snr = parseInt(snrStr, 10);
    }
    if (snr >= 30) return '';
    if (snr >= 15) return 'fair';
    if (snr >= 0) return 'poor';
    if (signalStr && signalStr.includes('dBm')) {
        const dbm = parseInt(signalStr, 10);
        if (dbm >= -65) return '';
        if (dbm >= -80) return 'fair';
        return 'poor';
    }
    return '';
}

function renderProberUI(data, isFullRender = true) {
    if (!data) return;

    // Update selection summary badge
    const targetIp = (data.target_ip && data.target_ip !== 'auto') ? data.target_ip : '';
    if (!targetIp) {
        if (proberElements.selectionBadge) {
            proberElements.selectionBadge.textContent = t('unselected_device');
            proberElements.selectionBadge.classList.remove('selected');
        }
    } else {
        const matchedClient = Array.isArray(data.clients) ? data.clients.find(c => c.ip === targetIp || (c.mac && c.mac.toUpperCase() === targetIp.toUpperCase())) : null;
        if (proberElements.selectionBadge) {
            proberElements.selectionBadge.classList.add('selected');
            if (matchedClient) {
                const rssiPart = (matchedClient.signal_dbm && matchedClient.signal_dbm !== '—') ? ` · RSSI: ${matchedClient.signal_dbm}` : '';
                proberElements.selectionBadge.textContent = `${t('selecting_device')}${matchedClient.hostname || matchedClient.ip} (${matchedClient.ip || matchedClient.mac})${rssiPart}`;
            } else {
                proberElements.selectionBadge.textContent = `${t('selecting_device')}${targetIp}`;
            }
        }
    }

    populateGestureDeviceSelect(data.clients, targetIp);

    if (isFullRender) {
        renderCardsTables(data.cards, data.clients, targetIp);
    }

    let activeClient = null;
    let activeCard = null;
    if (Array.isArray(data.clients)) {
        if (targetIp) {
            activeClient = data.clients.find(c => c.ip === targetIp || (c.mac && c.mac.toUpperCase() === targetIp.toUpperCase()));
        }
    }
    if (activeClient && Array.isArray(data.cards)) {
        activeCard = data.cards.find(c => c.iface === activeClient.iface);
    }
    if (!activeCard && Array.isArray(data.cards) && data.cards.length > 0) {
        activeCard = data.cards[0];
    }
    updateAvailableBandwidths(activeClient, activeCard);
}

function renderCardsTables(cards, clients, currentTargetIp) {
    if (!proberElements.cardsContainer) return;
    if (!currentTargetIp && currentProberConfig) {
        currentTargetIp = currentProberConfig.target_ip;
    }

    // Group clients by interface name
    const clientsByCard = new Map();
    if (Array.isArray(clients)) {
        clients.forEach(c => {
            const iface = c.iface || 'br-lan';
            if (!clientsByCard.has(iface)) clientsByCard.set(iface, []);
            clientsByCard.get(iface).push(c);
        });
    }

    // Determine list of cards to render
    let cardList = Array.isArray(cards) ? [...cards] : [];
    if (cardList.length === 0) {
        // Fallback: build card list from clients
        clientsByCard.forEach((clientList, iface) => {
            cardList.push({
                iface,
                ssid: clientList[0]?.ssid || 'Wi-Fi Network',
                band: clientList[0]?.band || (iface.includes('.0-') ? '5GHz' : (iface.includes('.1-') ? '2.4GHz' : 'Wi-Fi')),
                station_count: clientList.length
            });
        });
    }

    // Also include any client interfaces not explicitly in cards
    clientsByCard.forEach((clientList, iface) => {
        if (!cardList.some(c => c.iface === iface)) {
            cardList.push({
                iface,
                ssid: clientList[0]?.ssid || 'Other Network',
                band: clientList[0]?.band || 'LAN',
                station_count: clientList.length
            });
        }
    });

    if (cardList.length === 0) {
        proberElements.cardsContainer.innerHTML = `<div class="empty-client-notice">${t('no_wifi_clients')}</div>`;
        return;
    }

    proberElements.cardsContainer.replaceChildren();

    cardList.forEach(card => {
        const cardStations = clientsByCard.get(card.iface) || [];
        const panel = document.createElement('div');
        panel.className = 'wifi-card-panel';

        // Card Header
        const header = document.createElement('div');
        header.className = 'wifi-card-header';

        const left = document.createElement('div');
        left.className = 'wifi-card-info-left';

        const radioSvg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        radioSvg.setAttribute('class', 'card-radio-svg');
        radioSvg.setAttribute('viewBox', '0 0 24 24');
        radioSvg.setAttribute('width', '13');
        radioSvg.setAttribute('height', '13');
        radioSvg.setAttribute('fill', 'none');
        radioSvg.setAttribute('stroke', 'currentColor');
        radioSvg.setAttribute('stroke-width', '2');
        radioSvg.setAttribute('stroke-linecap', 'round');
        radioSvg.setAttribute('stroke-linejoin', 'round');
        radioSvg.innerHTML = '<path d="M5 12.55a11 11 0 0 1 14.08 0"></path><path d="M1.42 9a16 16 0 0 1 21.16 0"></path><path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path><line x1="12" y1="20" x2="12.01" y2="20"></line>';

        const devBadge = document.createElement('span');
        devBadge.className = 'card-dev-badge';
        devBadge.textContent = card.iface;

        const bandTag = document.createElement('span');
        if (card.band === '5GHz') {
            bandTag.className = 'band-tag-5g';
            bandTag.textContent = '5GHz';
        } else if (card.band === '2.4GHz') {
            bandTag.className = 'band-tag-2g';
            bandTag.textContent = '2.4GHz';
        } else {
            bandTag.className = 'band-tag-generic';
            bandTag.textContent = card.band || 'Wi-Fi';
        }

        const ssidBadge = document.createElement('span');
        ssidBadge.className = 'card-ssid-badge';
        ssidBadge.textContent = card.ssid ? `SSID: ${card.ssid}` : 'SSID: —';

        const widthBadge = document.createElement('span');
        widthBadge.className = 'card-bw-badge';
        widthBadge.textContent = card.width_str || (card.band === '2.4GHz' ? '20 MHz' : '80 MHz');

        left.append(radioSvg, devBadge, bandTag, ssidBadge, widthBadge);

        const right = document.createElement('div');
        right.className = 'wifi-card-info-right';

        const countBadge = document.createElement('span');
        countBadge.className = cardStations.length > 0 ? 'card-count-badge has-stations' : 'card-count-badge';
        countBadge.textContent = `${cardStations.length} ${t('devices_count')}`;
        right.appendChild(countBadge);

        let bestCardRssi = -999;
        cardStations.forEach(c => {
            if (c.signal_dbm && c.signal_dbm !== '—') {
                const m = c.signal_dbm.match(/-?\d+/);
                if (m) {
                    const v = parseInt(m[0], 10);
                    if (v > bestCardRssi) bestCardRssi = v;
                }
            }
        });
        if (bestCardRssi > -900) {
            const cardRssiBadge = document.createElement('span');
            cardRssiBadge.className = 'card-rssi-summary-badge';
            cardRssiBadge.title = currentLang === 'vi' ? 'Cường độ RSSI mạnh nhất trên card này' : 'Strongest RSSI on this card';
            cardRssiBadge.innerHTML = `<span class="card-rssi-dot"></span>RSSI max: <strong>${bestCardRssi} dBm</strong>`;
            right.appendChild(cardRssiBadge);
        }

        header.append(left, right);
        panel.appendChild(header);

        // Card Table
        const tableWrap = document.createElement('div');
        tableWrap.className = 'wifi-station-table-wrap';

        const table = document.createElement('table');
        table.className = 'wifi-station-table';

        const thead = document.createElement('thead');
        thead.innerHTML = `
            <tr>
                <th style="width: 44px; text-align: center;">${t('col_select')}</th>
                <th>${t('col_device')}</th>
                <th>${t('col_ip')}</th>
                <th>${t('col_mac')}</th>
                <th>${t('col_signal')}</th>
                <th>${t('col_bitrate')}</th>
                <th style="text-align: center;">${t('col_activity')}</th>
            </tr>
        `;
        table.appendChild(thead);

        const tbody = document.createElement('tbody');

        if (cardStations.length === 0) {
            const emptyTr = document.createElement('tr');
            emptyTr.className = 'card-empty-row';
            emptyTr.innerHTML = `<td colspan="7" class="card-empty-cell">${t('no_card_stations')}</td>`;
            tbody.appendChild(emptyTr);
        } else {
            cardStations.forEach(c => {
                const tr = document.createElement('tr');
                tr.className = 'wifi-station-row';
                const clientTarget = c.ip || c.mac;

                const isCapturingThis = cfrState.running && (
                    (cfrState.target_mac && c.mac && cfrState.target_mac.toUpperCase() === c.mac.toUpperCase()) ||
                    (cfrState.target_ip && c.ip && cfrState.target_ip === c.ip)
                );

                const isSelected = Boolean(currentTargetIp && currentTargetIp !== 'auto' && (currentTargetIp === c.ip || currentTargetIp === c.mac));

                if (isSelected) tr.classList.add('selected-row');

                // 1. Selector input cell (hỗ trợ click để chọn hoặc bỏ chọn về auto)
                const tdSelect = document.createElement('td');
                tdSelect.style.textAlign = 'center';
                const input = document.createElement('input');
                input.type = 'radio';
                input.name = 'target-device-radio';
                input.checked = isSelected;

                const toggleSelection = () => {
                    if (!clientTarget) return;
                    if (currentProberConfig.target_ip === clientTarget) {
                        // Đang chọn -> BỎ CHỌN (về trạng thái chưa chọn thiết bị nào)
                        currentProberConfig.target_ip = '';
                        input.checked = false;
                        document.querySelectorAll('.wifi-station-row').forEach(r => r.classList.remove('selected-row'));
                        updateAvailableBandwidths(null, card);
                    } else {
                        // Chưa chọn -> CHỌN thiết bị này và thiết lập OPTION DEFAULTS
                        currentProberConfig.target_ip = clientTarget;
                        cfrState.targetMac = c.mac;
                        cfrState.targetIp = c.ip;
                        v6State.mac = c.mac;
                        v6State.ip = c.ip;
                        v6State.hostname = c.hostname || '';
                        syncTargetDeviceToV6(c.mac, c.ip, c.hostname || '');
                        input.checked = true;
                        document.querySelectorAll('.wifi-station-row').forEach(r => r.classList.remove('selected-row'));
                        tr.classList.add('selected-row');

                        // 1. OPTION DEFAULT: Tự động gán Băng thông cao nhất thiết bị hỗ trợ
                        let maxBw = 0;
                        if (c.supported_bandwidths && c.supported_bandwidths.length > 0) {
                            maxBw = c.supported_bandwidths[c.supported_bandwidths.length - 1];
                        } else if (card && card.supported_bandwidths && card.supported_bandwidths.length > 0) {
                            maxBw = card.supported_bandwidths[card.supported_bandwidths.length - 1];
                        }
                        cfrState.bandwidth = maxBw;
                        if (cfrElements.bandwidthSelect) {
                            cfrElements.bandwidthSelect.value = String(maxBw);
                        }


                        updateAvailableBandwidths(c, card);

                        // Tắt banner lỗi nếu trước đó đang cảnh báo nhắc chọn thiết bị
                        if (cfrElements.errorBanner && cfrElements.errorBanner.textContent.includes(t('prompt_select_device'))) {
                            cfrElements.errorBanner.hidden = true;
                        }

                        // Nếu CFR đang chạy, tự động chuyển đổi luồng thu phần cứng sang thiết bị này ngay lập tức
                        if (cfrState.running) {
                            startCfrCapture(c);
                        }
                    }
                    renderCfrUI();
                    saveProberConfig();
                };

                input.addEventListener('click', e => {
                    e.stopPropagation();
                    toggleSelection();
                });

                tr.addEventListener('click', e => {
                    if (e.target.tagName.toLowerCase() === 'input' || e.target.tagName.toLowerCase() === 'button') return;
                    toggleSelection();
                });

                tdSelect.appendChild(input);

                // 2. Device Name cell (hiển thị tag CSI Live nếu đang thu)
                const tdDevice = document.createElement('td');
                let capturingBadge = '';
                if (isCapturingThis) {
                    tr.classList.add('capturing-row');
                    capturingBadge = `<span class="capturing-badge-inline"><span class="pulse-dot"></span>${t('badge_capturing')}</span>`;
                }
                tdDevice.innerHTML = `<span class="device-name-cell"><span class="device-name-text">${escapeHtml(c.hostname || 'Device')}</span>${capturingBadge}</span>`;

                // 3. IP Address cell
                const tdIp = document.createElement('td');
                if (c.ip && c.ip.length > 0) {
                    tdIp.innerHTML = `<span class="ip-badge">${c.ip}</span>`;
                } else {
                    tdIp.innerHTML = `<span class="ip-badge unassigned">${t('no_ip_assigned')}</span>`;
                }

                // 4. MAC Address cell
                const tdMac = document.createElement('td');
                tdMac.innerHTML = `<span class="mac-badge">${c.mac}</span>`;

                // 5. RSSI cell (trích xuất mức công suất thu thực tế dBm cho mọi thiết bị)
                const tdSignal = document.createElement('td');
                let rssiText = '—';
                let rssiVal = null;
                if (isCapturingThis && latestFrame && latestFrame.rssi_comb_db !== undefined && latestFrame.rssi_comb_db !== 0) {
                    rssiVal = latestFrame.rssi_comb_db;
                    rssiText = `${rssiVal} dBm`;
                } else if (c.signal_dbm && c.signal_dbm !== '—' && c.signal_dbm !== 'Online') {
                    const m = c.signal_dbm.match(/-?\d+/);
                    if (m) {
                        const parsed = parseInt(m[0], 10);
                        if (parsed < 0) {
                            rssiVal = parsed;
                            rssiText = `${parsed} dBm`;
                        } else if (parsed > 0) {
                            rssiVal = parsed - 95;
                            rssiText = `${rssiVal} dBm`;
                        }
                    }
                }

                if (rssiVal === null && c.snr && c.snr !== '—' && c.snr !== 'Online') {
                    const m = c.snr.match(/\d+/);
                    if (m) {
                        const snr = parseInt(m[0], 10);
                        if (snr > 0) {
                            rssiVal = snr - 95;
                            rssiText = `${rssiVal} dBm`;
                        }
                    }
                }

                const sigClass = getSignalClass(null, rssiText);
                tdSignal.innerHTML = `<span class="signal-cell"><span class="signal-indicator-dot ${sigClass}"></span><span class="station-rssi-text">${rssiText}</span></span>`;

                // 6. Bitrate cell
                const tdRate = document.createElement('td');
                const bwBadge = c.width_str ? `<span class="station-bw-badge">${c.width_str}</span>` : '';
                const rateText = (c.bitrate && c.bitrate !== '—') ? c.bitrate : '—';
                tdRate.innerHTML = `<span>${rateText}</span>${bwBadge}`;

                // 7. Activity / Last Seen cell
                const tdActive = document.createElement('td');
                tdActive.style.textAlign = 'center';
                const rawAct = (c.inactive_ms && c.inactive_ms !== '—') ? c.inactive_ms : 'Active';
                if (rawAct === '0 ms' || rawAct === '0ms' || rawAct === 'Active') {
                    tdActive.innerHTML = `<span style="color:#60a5fa" title="Vừa truyền nhận dữ liệu (${rawAct})">${currentLang === 'vi' ? 'Vừa xong' : 'Just now'}</span>`;
                } else {
                    tdActive.textContent = rawAct;
                }

                tr.append(tdSelect, tdDevice, tdIp, tdMac, tdSignal, tdRate, tdActive);
                tbody.appendChild(tr);
            });
        }

        table.appendChild(tbody);
        tableWrap.appendChild(table);
        panel.appendChild(tableWrap);
        proberElements.cardsContainer.appendChild(panel);
    });
}

function escapeHtml(str) {
    if (!str) return '';
    return str.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
}

let isScanningNetwork = false;
let proberTableRenderedOnce = false;
let lastRenderedClientsSig = '';

function computeClientsSignature(cards, clients) {
    if (!Array.isArray(clients) && !Array.isArray(cards)) return '';
    const cSig = (clients || []).map(c => `${c.mac}|${c.ip}|${c.iface}|${c.signal_dbm}|${c.inactive_ms}`).join(';');
    const cardSig = (cards || []).map(card => `${card.iface}|${card.station_count}|${card.max_bandwidth}`).join(';');
    return `${cardSig}##${cSig}`;
}

async function loadProberStatus(forceScan = false) {
    if (forceScan && isScanningNetwork) return;
    if (forceScan) {
        isScanningNetwork = true;
        if (proberElements.refreshBtn) {
            proberElements.refreshBtn.classList.add('loading');
            proberElements.refreshBtn.disabled = true;
        }
    }
    try {
        const url = forceScan ? '/api/prober?scan=1' : '/api/prober';
        const res = await fetch(url);
        if (!res.ok) return;
        const data = await res.json();
        currentProberConfig = data;
        const newSig = computeClientsSignature(data.cards, data.clients);
        const shouldFullRender = forceScan || !proberTableRenderedOnce || (newSig !== lastRenderedClientsSig);
        if (shouldFullRender) {
            lastRenderedClientsSig = newSig;
            proberTableRenderedOnce = true;
        }
        renderProberUI(data, shouldFullRender);
    } catch (e) {
        console.warn('Cannot load prober status', e);
    } finally {
        if (forceScan) {
            isScanningNetwork = false;
            if (proberElements.refreshBtn) {
                proberElements.refreshBtn.classList.remove('loading');
                proberElements.refreshBtn.disabled = false;
            }
        }
    }
}

async function saveProberConfig() {
    try {
        const target_ip = currentProberConfig.target_ip || 'auto';
        const target_ips = (target_ip && target_ip !== 'auto') ? [target_ip] : [];
        const interval_ms = 10;
        const enabled = cfrState.running;

        const res = await fetch('/api/prober', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ enabled, mode: 'single', target_ip, target_ips, interval_ms })
        });
        if (res.ok) {
            const data = await res.json();
            currentProberConfig = data;
            renderProberUI(data, false);
        }
    } catch (e) {
        console.error('Cannot save prober config', e);
    }
}

// Quick Auto Strongest button listener
if (proberElements.quickAutoBtn) {
    proberElements.quickAutoBtn.addEventListener('click', () => {
        currentProberConfig.target_ip = 'auto';
        document.querySelectorAll('.wifi-station-row').forEach(r => r.classList.remove('selected-row'));
        document.querySelectorAll('input[name="target-device-radio"]').forEach(r => r.checked = false);
        updateAvailableBandwidths();
        renderCfrUI();
        saveProberConfig();
    });
}

// Language switch listeners
document.querySelectorAll('.lang-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        applyLanguage(btn.dataset.lang);
    });
});

window.addEventListener('resize', resizeCanvas);
if ('ResizeObserver' in window) new ResizeObserver(resizeCanvas).observe(canvas);

if (cfrElements.startBtn) {
    cfrElements.startBtn.addEventListener('click', () => startCfrCapture());
}
if (cfrElements.stopBtn) {
    cfrElements.stopBtn.addEventListener('click', () => stopCfrCapture());
}
if (cfrElements.downloadBtn) {
    cfrElements.downloadBtn.addEventListener('click', () => {
        window.location.href = '/api/cfr/download';
    });
}
if (cfrElements.bandwidthSelect) {
    cfrElements.bandwidthSelect.addEventListener('change', () => {
        const val = parseInt(cfrElements.bandwidthSelect.value, 10) || 0;
        cfrState.bandwidth = val;
        const bwMap = { 0: '20 MHz', 1: '40 MHz', 2: '80 MHz', 3: '160 MHz', 4: '320 MHz' };
        if (cfrElements.bandwidthDisplay) {
            cfrElements.bandwidthDisplay.textContent = bwMap[val] || '20 MHz';
        }
        document.querySelectorAll('.cfr-bw-chip').forEach(chip => {
            const chipVal = parseInt(chip.dataset.bw, 10);
            chip.classList.toggle('active', chipVal === val);
        });
    });
}

document.querySelectorAll('.cfr-bw-chip').forEach(chip => {
    chip.addEventListener('click', () => {
        if (cfrState.running || chip.disabled || chip.classList.contains('unsupported')) return;
        const val = parseInt(chip.dataset.bw, 10);
        if (cfrElements.bandwidthSelect) {
            cfrElements.bandwidthSelect.value = String(val);
            cfrElements.bandwidthSelect.dispatchEvent(new Event('change'));
        }
    });
});

if (proberElements.refreshBtn) {
    proberElements.refreshBtn.addEventListener('click', () => {
        loadProberStatus(true);
    });
}

applyLanguage(currentLang);
resizeCanvas();
connectStream();
loadProberStatus(false);
fetchCfrStatus();
initV6Controls();

// Định kỳ tự động cập nhật danh sách thiết bị kết nối & trạng thái an toàn (chu kỳ 6s nhẹ CPU, an toàn tuyệt đối với hostapd IPC)
window.setInterval(() => {
    loadProberStatus(false);
}, 6000);

window.setInterval(() => {
    if (cfrState.running) {
        fetchCfrStatus();
    }
}, 1500);

window.setInterval(() => {
    if (lastFrameAt && performance.now() - lastFrameAt > 6000) {
        setStatus('waiting', t('status_packet_wait'));
    }
    // Auto-wake watchdog: Nếu đã ấn thu nhưng sau 2s chưa có frame đầu tiên (do trạm ngủ sâu),
    // tự động kích hoạt thêm 1 đợt probe burst để đánh thức trạm, người dùng không cần bấm huỷ rồi thu lại
    if (cfrState.running && !latestFrame && captureStartedAt && (performance.now() - captureStartedAt > 2000) && autoWakeAttempts < 2) {
        autoWakeAttempts++;
        console.log('[CSI] Auto-wake triggering extra probe burst for sleeping station...');
        fetch('/api/cfr', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                action: 'start',
                mac: cfrState.target_mac || 'auto',
                ip: cfrState.target_ip || '',
                iface: cfrState.iface || '',
                bandwidth: cfrState.bandwidth || 0,
                auto_probe: true
            })
        }).catch(() => {});
    }
}, 1000);

// Khởi tạo mã QR ở Sidebar bên trái
function initSidebarQr() {
    const container = document.getElementById('sidebarQrBox');
    if (!container || typeof QRCode === 'undefined') return;
    container.innerHTML = '';
    const host = location.hostname || '192.168.5.1';
    const port = location.port ? (':' + location.port) : '';
    const url = `${location.protocol}//${host}${port}/gioithieu`;
    try {
        new QRCode(container, {
            text: url,
            width: 104,
            height: 104,
            colorDark: '#07121e',
            colorLight: '#ffffff',
            correctLevel: QRCode.CorrectLevel.M
        });
    } catch (e) {
        console.error('[QR] Sidebar render error:', e);
    }
}

// Bộ chuyển tab mã QR ở chân Sidebar (Phương án B: QR Wi-Fi & Tài liệu mô tả)
function initSidebarQrTabs() {
    const tabWifi = document.getElementById('tab-qr-wifi');
    const tabDoc = document.getElementById('tab-qr-doc');
    const panelWifi = document.getElementById('panel-qr-wifi');
    const panelDoc = document.getElementById('panel-qr-doc');
    if (!tabWifi || !tabDoc || !panelWifi || !panelDoc) return;

    function switchTab(toWifi) {
        if (toWifi) {
            tabWifi.classList.add('active');
            tabWifi.setAttribute('aria-selected', 'true');
            tabDoc.classList.remove('active');
            tabDoc.setAttribute('aria-selected', 'false');
            panelWifi.hidden = false;
            panelWifi.classList.add('active');
            panelDoc.hidden = true;
            panelDoc.classList.remove('active');
        } else {
            tabDoc.classList.add('active');
            tabDoc.setAttribute('aria-selected', 'true');
            tabWifi.classList.remove('active');
            tabWifi.setAttribute('aria-selected', 'false');
            panelDoc.hidden = false;
            panelDoc.classList.add('active');
            panelWifi.hidden = true;
            panelWifi.classList.remove('active');
            initSidebarQr();
        }
    }

    tabWifi.onclick = () => switchTab(true);
    tabDoc.onclick = () => switchTab(false);
}

window.addEventListener('DOMContentLoaded', () => {
    initSidebarQr();
    initSidebarQrTabs();
});
setTimeout(() => {
    initSidebarQr();
    initSidebarQrTabs();
}, 600);
