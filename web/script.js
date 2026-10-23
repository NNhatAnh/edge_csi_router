// ============================================================================
// Edge CSI Dashboard · Bilingual & Multi-Device Active Prober
// ============================================================================

const i18n = {
    vi: {
        brand_sub: 'WIRELESS SENSING',
        nav_workspace: 'WORKSPACE',
        tab_overview: 'Tổng quan',
        tab_breath: 'Dạng sóng thở',
        tab_amplitude: 'Biên độ CSI',
        tab_phase: 'Pha CSI',
        tab_iq: 'Chòm sao I/Q',
        tab_chains: 'So sánh antenna',
        tab_packet: 'Thông tin gói',
        sidebar_footer: 'Edge processing<br>Local network only',
        topbar_eyebrow: 'ĐO ĐẠC ROUTER / GIÁM SÁT CSI',
        btn_calibrate: 'Hiệu chuẩn lại',
        status_live: 'TRỰC TIẾP',
        status_waiting: 'ĐANG CHỜ DỮ LIỆU',
        status_offline: 'MẤT KẾT NỐI',
        status_connecting: 'ĐANG KẾT NỐI',
        status_connected: 'Đã nối backend · chờ CSI',
        status_disrupted: 'Stream gián đoạn · đang nối lại',
        status_calibrating: 'Đang chờ hiệu chuẩn nền mới',
        status_packet_wait: 'Đang chờ gói (Client ngủ hoặc chu kỳ beacon thưa)',
        metric_sc: 'SUBCARRIERS',
        metric_chains: 'ANTENNA CHAINS',
        metric_motion: 'MOTION SCORE',
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
        prober_mode_label: 'Chế độ thu:',
        mode_single: 'Đơn thiết bị (Single Device)',
        mode_multi: 'Đa thiết bị (Multi-Device)',
        target_client_label: 'Thiết bị Wi-Fi mục tiêu:',
        auto_select_wifi: '-- Tự động chọn client Wi-Fi mạnh nhất --',
        custom_ip_placeholder: 'Nhập IP thủ công (vd: 192.168.10.238)',
        cards_section_title: 'Danh Sách Thiết Bị Kết Nối Theo Card Wi-Fi',
        auto_strongest_btn: 'Tự động chọn sóng mạnh nhất',
        manual_ip_btn: 'Nhập IP thủ công',
        refresh_btn: 'Quét lại',
        select_all: 'Chọn tất cả',
        deselect_all: 'Bỏ chọn tất cả',
        selected_count: 'Đã chọn: ',
        col_select: 'Chọn',
        col_device: 'Tên thiết bị',
        col_ip: 'Địa chỉ IP',
        col_mac: 'Địa chỉ MAC',
        col_signal: 'Tín hiệu / SNR',
        col_bitrate: 'Tốc độ TX',
        col_activity: 'Lần cuối thấy',
        no_card_stations: 'Không có thiết bị nào đang kết nối trên card này',
        no_ip_assigned: 'Chưa cấp IP',
        selecting_auto: 'Đang chọn: Tự động (Sóng mạnh nhất)',
        selecting_manual: 'Đang chọn thủ công: ',
        selecting_device: 'Đang chọn: ',
        devices_count: 'thiết bị',
        select_card: 'Chọn card này',
        deselect_card: 'Bỏ chọn card',
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
        col_action: 'Thao tác',
        // CFR Panel
        cfr_badge: 'ATH12K · HARDWARE CFR PIPELINE',
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
        thres_high_short: 'Cao',
        thres_low_short: 'Thấp',
        // Views
        views: {
            overview: { title: 'Tổng quan tín hiệu', kicker: 'LIVE ACTIVITY', chart: 'Motion score & ngưỡng phát hiện' },
            breath: { title: 'Dạng sóng nhịp thở & Phân biệt người', kicker: 'HUMAN DISCRIMINATION', chart: 'Biên độ pha vi chuyển động (rad)' },
            amplitude: { title: 'Biên độ CSI', kicker: 'CHANNEL RESPONSE', chart: 'Biên độ theo tone · raw I/Q' },
            phase: { title: 'Pha CSI', kicker: 'PHASE RESPONSE', chart: 'Pha theo tone · radian' },
            iq: { title: 'Chòm sao I/Q', kicker: 'COMPLEX CHANNEL', chart: 'Phân bố mẫu I/Q' },
            chains: { title: 'So sánh antenna', kicker: 'RF CHAINS', chart: 'Biên độ giữa các antenna chains' },
            packet: { title: 'Thông tin gói', kicker: 'FRAME INSPECTOR', chart: 'CSI amplitude của frame hiện tại' }
        }
    },
    en: {
        brand_sub: 'WIRELESS SENSING',
        nav_workspace: 'WORKSPACE',
        tab_overview: 'Overview',
        tab_breath: 'Respiration Wave',
        tab_amplitude: 'CSI Amplitude',
        tab_phase: 'CSI Phase',
        tab_iq: 'I/Q Constellation',
        tab_chains: 'Antenna Chains',
        tab_packet: 'Frame Inspector',
        sidebar_footer: 'Edge processing<br>Local network only',
        topbar_eyebrow: 'ROUTER TELEMETRY / CSI MONITOR',
        btn_calibrate: 'Recalibrate',
        status_live: 'LIVE',
        status_waiting: 'WAITING FOR DATA',
        status_offline: 'DISCONNECTED',
        status_connecting: 'CONNECTING',
        status_connected: 'Backend connected · waiting for CSI',
        status_disrupted: 'Stream disrupted · reconnecting',
        status_calibrating: 'Waiting for new baseline calibration',
        status_packet_wait: 'Waiting for packets (Client asleep or slow beacons)',
        metric_sc: 'SUBCARRIERS',
        metric_chains: 'ANTENNA CHAINS',
        metric_motion: 'MOTION SCORE',
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
        prober_mode_label: 'Capture Mode:',
        mode_single: 'Single Device',
        mode_multi: 'Multi-Device',
        target_client_label: 'Target Wi-Fi Device(s):',
        auto_select_wifi: '-- Auto-select strongest Wi-Fi client --',
        custom_ip_placeholder: 'Custom IP (e.g. 192.168.10.238)',
        cards_section_title: 'Wi-Fi Interfaces & Connected Stations',
        auto_strongest_btn: 'Auto Strongest Signal',
        manual_ip_btn: 'Manual IP',
        refresh_btn: 'Refresh',
        select_all: 'Select All',
        deselect_all: 'Deselect All',
        selected_count: 'Selected: ',
        col_select: 'Select',
        col_device: 'Device Name',
        col_ip: 'IP Address',
        col_mac: 'MAC Address',
        col_signal: 'Signal / SNR',
        col_bitrate: 'TX Rate',
        col_activity: 'Last Seen',
        no_card_stations: 'No stations connected on this interface',
        no_ip_assigned: 'No IP',
        selecting_auto: 'Target: Auto (Strongest Signal)',
        selecting_manual: 'Target Manual IP: ',
        selecting_device: 'Target: ',
        devices_count: 'device(s)',
        select_card: 'Select card',
        deselect_card: 'Deselect card',
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
        col_action: 'Action',
        // CFR Panel
        cfr_badge: 'ATH12K · HARDWARE CFR PIPELINE',
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
        thres_high_short: 'High',
        thres_low_short: 'Low',
        // Views
        views: {
            overview: { title: 'Signal Overview', kicker: 'LIVE ACTIVITY', chart: 'Motion score & detection thresholds' },
            breath: { title: 'Respiration Wave & Discrimination', kicker: 'HUMAN DISCRIMINATION', chart: 'Micro-motion phase residual (rad)' },
            amplitude: { title: 'CSI Amplitude', kicker: 'CHANNEL RESPONSE', chart: 'Tone amplitude · raw I/Q' },
            phase: { title: 'CSI Phase', kicker: 'PHASE RESPONSE', chart: 'Tone phase · radians' },
            iq: { title: 'I/Q Constellation', kicker: 'COMPLEX CHANNEL', chart: 'I/Q sample constellation' },
            chains: { title: 'Antenna Comparison', kicker: 'RF CHAINS', chart: 'Amplitude across RF chains' },
            packet: { title: 'Frame Inspector', kicker: 'FRAME INSPECTOR', chart: 'CSI amplitude of current frame' }
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
    if (proberElements.applyBtn && !isSavingProber) {
        proberElements.applyBtn.textContent = t('apply_btn');
    }
}

const colors = ['#4be2d0', '#a98cff', '#ffb86b', '#71a7ff', '#ff7f9d'];
const canvas = document.getElementById('signal-chart');
const context = canvas.getContext('2d');
const emptyOverlay = document.getElementById('chart-empty');
const chainSelect = document.getElementById('chain-select');
const calibrateButton = document.getElementById('calibrate-button');

const domElements = {
    scCount: document.getElementById('sc-count'),
    scCaption: document.getElementById('sc-caption'),
    chainCount: document.getElementById('chain-count'),
    nssCaption: document.getElementById('nss-caption'),
    motionValue: document.getElementById('motion-value'),
    motionCaption: document.getElementById('motion-caption'),
    presenceValue: document.getElementById('presence-value'),
    presenceCaption: document.getElementById('presence-caption'),
    waveValue: document.getElementById('wave-value'),
    waveCaption: document.getElementById('wave-caption'),
    waveChip: document.getElementById('wave-confidence-chip'),
    waveCard: document.getElementById('metric-card-wave'),
    frameRate: document.getElementById('frame-rate'),
    chipTsf: document.getElementById('chip-tsf'),
    ppduId: document.getElementById('ppdu-id'),
    bandwidth: document.getElementById('bandwidth'),
    apMac: document.getElementById('ap-mac'),
    frameId: document.getElementById('frame-id')
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
            drawCurrent();
        });
    }
}

function updateDomMetrics(frame, isNewFrame, now) {
    if (domElements.scCount) domElements.scCount.textContent = Number(frame.subcarriers_count || 0).toLocaleString();
    if (domElements.scCaption) domElements.scCaption.textContent = `${frame.tones_per_chain || 0} tone / chain`;
    if (domElements.chainCount) domElements.chainCount.textContent = String(frame.num_chains || frame.chains?.length || 0);
    if (domElements.nssCaption) domElements.nssCaption.textContent = `Spatial streams ${frame.nss ?? '—'}`;
    if (domElements.motionValue) {
        domElements.motionValue.textContent = frame.motion_ready
            ? (Number(frame.motion_score) || 0).toFixed(4)
            : '—';
    }

    const thresHigh = Number(frame.motion_threshold_high) || 0;
    const thresLow = Number(frame.motion_threshold_low) || 0;
    const thresHighEl = document.getElementById('thres-high-val');
    const thresLowEl = document.getElementById('thres-low-val');
    const motionChipEl = document.getElementById('motion-chip');

    if (thresHighEl) {
        thresHighEl.textContent = thresHigh > 0 ? thresHigh.toFixed(4) : '—';
    }
    if (thresLowEl) {
        thresLowEl.textContent = thresLow > 0 ? thresLow.toFixed(4) : '—';
    }
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

    const status = frame.presence_status || (frame.presence_detected ? (frame.is_human_presence ? 'human_present' : 'human_moving') : 'empty_room');

    if (domElements.presenceValue && domElements.presenceCaption) {
        if (status === 'human_moving' || status === 'gross_motion') {
            domElements.presenceValue.textContent = t('presence_moving');
            domElements.presenceValue.style.color = '#ffb86b';
            domElements.presenceCaption.textContent = t('caption_moving');
        } else if (status === 'human_present') {
            domElements.presenceValue.textContent = t('presence_present');
            domElements.presenceValue.style.color = '#4be2d0';
            const bpm = Number(frame.breath_bpm || 0).toFixed(1);
            const span = Number(frame.breath_phase_span || 0).toFixed(2);
            const reg = Math.round(Number(frame.breath_regularity || 0) * 100);
            const sleepStr = frame.is_sleeping ? (currentLang === 'vi' ? ' · Đang ngủ' : ' · Sleeping') : '';
            domElements.presenceCaption.textContent = currentLang === 'vi'
                ? `Nhịp thở: ${bpm} BPM · Tự tương quan: ${reg}% · Span: ${span} rad${sleepStr}`
                : `Respiration: ${bpm} BPM · Autocorr: ${reg}% · Span: ${span} rad${sleepStr}`;
        } else if (status === 'calibrating') {
            domElements.presenceValue.textContent = t('presence_calibrating');
            domElements.presenceValue.style.color = '#71a7ff';
            domElements.presenceCaption.textContent = t('caption_calibrating');
        } else {
            domElements.presenceValue.textContent = t('presence_empty');
            domElements.presenceValue.style.color = '#77869d';
            domElements.presenceCaption.textContent = t('caption_empty');
        }
    }

    if (domElements.waveValue && domElements.waveCaption) {
        const pct = Math.round((Number(frame.wave_confidence) || 0) * 100);
        const rawPct = Math.round((Number(frame.wave_raw_prob) || 0) * 100);
        const samples = Number(frame.wave_samples_in_window) || 0;
        const reason = frame.wave_reason || 'idle';
        const tones = frame.tones_per_chain || 0;
        const chains = frame.num_chains || 0;

        if (domElements.waveChip) {
            domElements.waveChip.textContent = `${pct}%`;
        }

        if (frame.wave_status === 'unavailable') {
            domElements.waveValue.textContent = t('wave_waiting');
            domElements.waveValue.style.color = '#77869d';
            let detail = t('caption_wave_waiting');
            if (reason === 'waiting_80mhz') {
                detail = currentLang === 'vi' ? `Chờ kênh 80 MHz (${tones}/208 tones)` : `Waiting 80 MHz (${tones}/208 tones)`;
            } else if (reason === 'waiting_3chains') {
                detail = currentLang === 'vi' ? `Chờ 3 antenna (${chains}/3 chains)` : `Waiting 3 antennas (${chains}/3 chains)`;
            } else if (reason === 'too_few_samples') {
                detail = currentLang === 'vi' ? `Chờ dữ liệu (${samples}/16 mẫu · bật Prober)` : `Waiting samples (${samples}/16 · enable Prober)`;
            }
            domElements.waveCaption.textContent = detail;
            if (domElements.waveCard) domElements.waveCard.classList.remove('detected');
            if (domElements.waveChip) domElements.waveChip.className = 'wave-confidence-chip';
        } else if (frame.wave_detected) {
            domElements.waveValue.textContent = t('wave_detected');
            domElements.waveValue.style.color = '#4be2d0';
            domElements.waveCaption.textContent = currentLang === 'vi'
                ? `Cử chỉ vẫy tay đang diễn ra (P: ${pct}%)`
                : `Waving gesture in progress (P: ${pct}%)`;
            if (domElements.waveCard) domElements.waveCard.classList.add('detected');
            if (domElements.waveChip) domElements.waveChip.className = 'wave-confidence-chip wave-active';
        } else {
            domElements.waveValue.textContent = t('wave_idle');
            domElements.waveValue.style.color = '#8fa3bf';
            domElements.waveCaption.textContent = currentLang === 'vi'
                ? `Chưa phát hiện · ${samples} mẫu/3.2s · P: ${rawPct}%`
                : `Idle · ${samples} samples/3.2s · P: ${rawPct}%`;
            if (domElements.waveCard) domElements.waveCard.classList.remove('detected');
            if (domElements.waveChip) domElements.waveChip.className = 'wave-confidence-chip';
        }
    }

    if (domElements.frameRate) domElements.frameRate.textContent = frameRate.toFixed(1);
    if (domElements.chipTsf) domElements.chipTsf.textContent = frame.chip_tsf_us ? `${Number(frame.chip_tsf_us).toLocaleString()} µs` : '—';
    if (domElements.ppduId) domElements.ppduId.textContent = frame.phy_ppdu_id ?? '—';
    if (domElements.bandwidth) domElements.bandwidth.textContent = frame.channel_bw_code ?? '—';
    if (domElements.apMac) domElements.apMac.textContent = frame.ap_mac || '—';
    if (domElements.frameId) domElements.frameId.textContent = `PPDU ${frame.phy_ppdu_id ?? '—'}`;
    if (isNewFrame) {
        setStatus('live', (currentLang === 'vi' ? 'Nhận frame ' : 'Frame received ') + new Date().toLocaleTimeString());
    }
}

function setStatus(status, description) {
    const dot = document.getElementById('status-dot');
    const label = document.getElementById('status');
    const lastUpdate = document.getElementById('last-update');
    if (dot) dot.className = `status-dot ${status}`;
    if (label) label.textContent = status === 'live' ? t('status_live') : status === 'waiting' ? t('status_waiting') : t('status_offline');
    if (lastUpdate) {
        lastUpdate.textContent = description;
        lastUpdate.title = description;
    }
}

function setView(view) {
    const viewDict = i18n[currentLang]?.views || i18n.vi.views;
    if (!viewDict[view]) return;
    activeView = view;
    document.querySelectorAll('.tab-btn').forEach(button => {
        button.classList.toggle('active', button.dataset.view === view);
    });
    document.getElementById('page-title').textContent = viewDict[view].title;
    document.getElementById('chart-kicker').textContent = viewDict[view].kicker;
    document.getElementById('chart-title').textContent = viewDict[view].chart;
    document.getElementById('chain-control').hidden = view === 'overview' || view === 'chains' || view === 'breath';
    scheduleRender();
}

document.querySelectorAll('.tab-btn').forEach(button => {
    button.addEventListener('click', () => setView(button.dataset.view));
});

chainSelect.addEventListener('change', scheduleRender);
calibrateButton.addEventListener('click', requestCalibration);

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
        document.getElementById('presence-value').textContent = t('presence_calibrating');
        document.getElementById('presence-caption').textContent = t('caption_calibrating');
        document.getElementById('motion-value').textContent = '—';
        document.getElementById('motion-caption').textContent = currentLang === 'vi' ? 'Đang khởi tạo cửa sổ 30 frame' : 'Initializing 30-frame window';
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
    const selected = Number(chainSelect.value) || 0;
    if (chainSelect.options.length === frame.chains.length) return;
    chainSelect.replaceChildren();
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
        setStatus('waiting', t('caption_waiting_csi'));
        return;
    }
    const isNewFrame = !latestFrame ||
        frame.timestamp_ns !== latestFrame.timestamp_ns ||
        frame.phy_ppdu_id !== latestFrame.phy_ppdu_id;
    latestFrame = frame;
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

        if (frame.motion_ready) {
            motionHistory.push({ time: now, score: Number(frame.motion_score) || 0 });
            if (motionHistory.length > 180) motionHistory.shift();
        }
        breathWaveHistory.push(Number(frame.breath_wave) || 0);
        if (breathWaveHistory.length > 240) breathWaveHistory.shift();
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
    source.addEventListener('csi', event => {
        try {
            acceptFrame(JSON.parse(event.data));
        } catch (error) {
            console.error('Invalid CSI event payload', error);
            setStatus('offline', 'Invalid payload');
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

    const chainIndex = Math.min(Number(chainSelect.value) || 0, latestFrame.chains.length - 1);
    const chain = latestFrame.chains[chainIndex] || latestFrame.chains[0];

    if (activeView === 'overview') {
        const values = motionHistory.map(entry => entry.score);
        const thresHigh = Number(latestFrame.motion_threshold_high) || 0;
        const thresLow = Number(latestFrame.motion_threshold_low) || 0;
        const isCalibrated = Boolean(latestFrame.motion_calibrated);

        const refs = [];
        if (thresHigh > 0) {
            refs.push({
                value: thresHigh,
                color: '#ff7f9d',
                label: currentLang === 'vi' ? 'Ngưỡng cao' : 'High thres',
                dashed: true
            });
        }
        if (thresLow > 0) {
            refs.push({
                value: thresLow,
                color: '#4be2d0',
                label: currentLang === 'vi' ? 'Ngưỡng thấp' : 'Low thres',
                dashed: true
            });
        }

        drawLineChart(values, '#ffb86b', width, height, 'Motion score', false, refs);

        const legend = [
            { name: `Motion score: ${(Number(latestFrame.motion_score) || 0).toFixed(4)}`, color: '#ffb86b' }
        ];
        if (thresHigh > 0) {
            legend.push({
                name: `${currentLang === 'vi' ? 'Ngưỡng cao' : 'High Thres'}: ${thresHigh.toFixed(4)}`,
                color: '#ff7f9d'
            });
        }
        if (thresLow > 0) {
            legend.push({
                name: `${currentLang === 'vi' ? 'Ngưỡng thấp' : 'Low Thres'}: ${thresLow.toFixed(4)}`,
                color: '#4be2d0'
            });
        }
        if (!isCalibrated) {
            legend.push({
                name: currentLang === 'vi' ? `Đang hiệu chuẩn (${latestFrame.motion_calibration_frames || 0}/40)` : `Calibrating (${latestFrame.motion_calibration_frames || 0}/40)`,
                color: '#8fa0b7'
            });
        }
        setLegend(legend);
    } else if (activeView === 'breath') {
        drawLineChart(
            breathWaveHistory,
            '#4be2d0',
            width,
            height,
            'Breath phase residual (rad)',
            false,
            [{ value: 0, color: 'rgba(255, 255, 255, 0.15)' }]
        );
        const status = latestFrame.presence_status || 'idle';
        const bpm = Number(latestFrame.breath_bpm || 0).toFixed(1);
        const span = Number(latestFrame.breath_phase_span || 0).toFixed(2);
        const reg = (Number(latestFrame.breath_regularity || 0) * 100).toFixed(0);
        const sleepTag = latestFrame.is_sleeping ? (currentLang === 'vi' ? ' · Đang ngủ' : ' · Sleeping') : '';
        setLegend([
            { name: `${currentLang === 'vi' ? 'Sóng vi chuyển động pha' : 'Micro-motion phase wave'} [${status}${sleepTag}]`, color: '#4be2d0' },
            { name: `${currentLang === 'vi' ? 'Nhịp thở' : 'Respiration'}: ${bpm} BPM`, color: '#ffb86b' },
            { name: `Phase Span: ${span} rad · ${currentLang === 'vi' ? 'Độ đều' : 'Regularity'}: ${reg}%`, color: '#a98cff' }
        ]);
    } else if (activeView === 'amplitude' || activeView === 'packet') {
        drawLineChart(chain?.amplitudes || [], colors[chainIndex % colors.length], width, height, 'Amplitude');
        setLegend([{ name: `Chain ${chainIndex} · amplitude`, color: colors[chainIndex % colors.length] }]);
    } else if (activeView === 'phase') {
        drawLineChart(chain?.phases || [], colors[chainIndex % colors.length], width, height, 'Phase');
        setLegend([{ name: `Chain ${chainIndex} · phase`, color: colors[chainIndex % colors.length] }]);
    } else if (activeView === 'iq') {
        drawConstellation(chain, width, height);
        setLegend([{ name: `Chain ${chainIndex} · I/Q samples`, color: colors[chainIndex % colors.length] }]);
    } else if (activeView === 'chains') {
        drawChainsComparison(width, height);
    }
}

function drawChainsComparison(width, height) {
    if (!latestFrame?.chains) return;
    const legend = [];
    latestFrame.chains.forEach((ch, index) => {
        const color = colors[index % colors.length];
        drawLineChart(ch.amplitudes, color, width, height, 'Amplitude', index > 0);
        legend.push({ name: `Chain ${index}`, color });
    });
    setLegend(legend);
}

function drawLineChart(values, color, width, height, label, overlay = false, references = []) {
    if (!values || values.length === 0) return;
    const ratio = Math.max(1, window.devicePixelRatio || 1);
    const left = 48 * ratio;
    const right = 14 * ratio;
    const top = 17 * ratio;
    const bottom = 31 * ratio;
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
        context.strokeStyle = 'rgba(129, 150, 181, 0.14)';
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
                    context.lineWidth = (ref.lineWidth || 1.3) * ratio;
                    if (ref.dashed !== false) {
                        context.setLineDash([7 * ratio, 5 * ratio]);
                    }
                    context.beginPath();
                    context.moveTo(left, y);
                    context.lineTo(width - right, y);
                    context.stroke();

                    // Nhãn ngưỡng hiển thị trực quan ngay trên đường vẽ
                    if (ref.label) {
                        context.fillStyle = ref.color || '#fff';
                        context.font = `bold ${8.5 * ratio}px "DM Mono", monospace`;
                        context.textAlign = 'right';
                        context.textBaseline = 'bottom';
                        context.fillText(`${ref.label}: ${ref.value.toFixed(4)}`, width - right, y - 2 * ratio);
                    }
                    context.restore();
                }
            });
        }
    }

    context.strokeStyle = color;
    context.lineWidth = 1.8 * ratio;
    context.beginPath();
    values.forEach((value, index) => {
        const x = left + (values.length === 1 ? plotWidth / 2 : (index / (values.length - 1)) * plotWidth);
        const y = top + ((max - value) / (max - min)) * plotHeight;
        if (index === 0) context.moveTo(x, y);
        else context.lineTo(x, y);
    });
    context.stroke();

    if (!overlay) {
        context.fillStyle = '#8a98ad';
        context.font = `${9 * ratio}px "DM Mono", monospace`;
        context.textAlign = 'right';
        context.textBaseline = 'bottom';
        context.fillText(max.toPrecision(4), left - 6 * ratio, top + 8 * ratio);
        context.fillText(min.toPrecision(4), left - 6 * ratio, height - bottom);
    }
}

function drawConstellation(chain, width, height) {
    if (!chain || !chain.i?.length || !chain.q?.length) return;
    const ratio = Math.max(1, window.devicePixelRatio || 1);
    const left = 48 * ratio;
    const right = 14 * ratio;
    const top = 17 * ratio;
    const bottom = 31 * ratio;
    const plotWidth = width - left - right;
    const plotHeight = height - top - bottom;
    const limit = Math.max(...chain.i.map(Math.abs), ...chain.q.map(Math.abs), 1) * 1.08;
    const centerX = left + plotWidth / 2;
    const centerY = top + plotHeight / 2;
    const scale = Math.min(plotWidth, plotHeight) / (2 * limit);

    context.strokeStyle = 'rgba(129, 150, 181, 0.22)';
    context.beginPath();
    context.moveTo(centerX, top);
    context.lineTo(centerX, height - bottom);
    context.moveTo(left, centerY);
    context.lineTo(width - right, centerY);
    context.stroke();

    context.fillStyle = colors[Math.max(0, Number(chainSelect.value) || 0) % colors.length];
    const stride = Math.max(1, Math.ceil(chain.i.length / 180));
    for (let index = 0; index < chain.i.length; index += stride) {
        const x = centerX + chain.i[index] * scale;
        const y = centerY - chain.q[index] * scale;
        context.globalAlpha = 0.75;
        context.beginPath();
        context.arc(x, y, 2.3 * ratio, 0, Math.PI * 2);
        context.fill();
    }
    context.globalAlpha = 1;
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
    if (!supported.includes(currentVal)) {
        currentVal = supported[supported.length - 1];
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
    } else {
        cfrElements.badge.className = 'badge-idle';
        cfrElements.statusText.textContent = t('cfr_status_idle');
        if (cfrElements.startBtn) cfrElements.startBtn.disabled = false;
        if (cfrElements.stopBtn) cfrElements.stopBtn.disabled = true;
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
                cfrElements.targetDisplay.textContent = t('auto_select_wifi');
            }
        }
    }

    if (cfrElements.ifaceDisplay) {
        cfrElements.ifaceDisplay.textContent = cfrState.iface || (cfrState.running ? 'phy00.1-0' : '—');
    }

    if (cfrElements.chipsetDisplay) {
        cfrElements.chipsetDisplay.textContent = cfrState.hw ? `${cfrState.hw} (${cfrState.mac_dev || 'mac'})` : 'ath12k';
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
        cfrState.bandwidth = Number(data.bandwidth) || 0;
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
                renderCardsTables(currentProberConfig.cards, currentProberConfig.clients, currentProberConfig.mode, currentProberConfig.target_ip);
            }
        }
    } catch (e) {
        console.warn('Failed to fetch CFR status:', e);
    }
}

async function startCfrCapture(target = null) {
    if (cfrElements.startBtn) cfrElements.startBtn.disabled = true;

    let mac = '';
    let ip = '';
    let iface = '';

    if (target && target.mac) {
        mac = target.mac;
        ip = target.ip || '';
        iface = target.iface || '';
    } else if (currentProberConfig.target_ip && currentProberConfig.target_ip !== 'auto') {
        const matched = Array.isArray(currentProberConfig.clients) ? currentProberConfig.clients.find(c => c.ip === currentProberConfig.target_ip) : null;
        if (matched) {
            mac = matched.mac;
            ip = matched.ip;
            iface = matched.iface;
        } else {
            ip = currentProberConfig.target_ip;
        }
    }

    const autoProbe = true;
    let bandwidth = 0;
    if (cfrElements.bandwidthSelect) {
        bandwidth = parseInt(cfrElements.bandwidthSelect.value, 10) || 0;
    }

    try {
        const res = await fetch('/api/cfr', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                action: 'start',
                mac: mac || 'auto',
                ip: ip,
                iface: iface,
                bandwidth: bandwidth,
                auto_probe: autoProbe
            })
        });
        const data = await res.json();
        if (res.ok && data.status === 'ok') {
            cfrState.running = true;
            cfrState.target_mac = data.target_mac || mac;
            cfrState.target_ip = data.target_ip || ip;
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
            renderCardsTables(currentProberConfig.cards, currentProberConfig.clients, currentProberConfig.mode, currentProberConfig.target_ip);
        }
    }
}

async function stopCfrCapture() {
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
        }
    } catch (err) {
        console.warn('Failed to stop CFR capture:', err);
    } finally {
        if (cfrElements.stopBtn) cfrElements.stopBtn.disabled = !cfrState.running;
        if (cfrElements.startBtn) cfrElements.startBtn.disabled = cfrState.running;
        if (currentProberConfig.cards) {
            renderCardsTables(currentProberConfig.cards, currentProberConfig.clients, currentProberConfig.mode, currentProberConfig.target_ip);
        }
    }
}

// ============================================================================
// Active Prober Controller (Grouped by Wi-Fi Card, Single & Multi Mode)
// ============================================================================
const proberElements = {
    toggle: document.getElementById('prober-toggle'),
    toggleLabel: document.getElementById('prober-toggle-label'),
    singleBtn: document.getElementById('mode-single-btn'),
    multiBtn: document.getElementById('mode-multi-btn'),
    interval: document.getElementById('prober-interval'),
    rateBadge: document.getElementById('prober-rate-badge'),
    applyBtn: document.getElementById('prober-apply-btn'),
    packetsCount: document.getElementById('prober-packets-count'),
    statusBadge: document.getElementById('prober-status-badge'),
    selectionBadge: document.getElementById('prober-selection-badge'),
    quickAutoBtn: document.getElementById('quick-auto-btn'),
    refreshBtn: document.getElementById('prober-refresh-btn'),
    multiActionsBar: document.getElementById('multi-actions-bar'),
    multiCount: document.getElementById('multi-count'),
    selectAllBtn: document.getElementById('multi-select-all'),
    deselectAllBtn: document.getElementById('multi-deselect-all'),
    cardsContainer: document.getElementById('prober-cards-container')
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

let selectedMultiIps = new Set();
let multiSelectionUserModified = false;

function updateRateBadge(ms) {
    const val = Number(ms) || 50;
    const hz = val > 0 ? (1000 / val).toFixed(1) : '0';
    if (proberElements.rateBadge) {
        proberElements.rateBadge.textContent = `${hz} Hz`;
    }
}

function setProberMode(mode) {
    currentProberConfig.mode = mode;
    if (proberElements.singleBtn) proberElements.singleBtn.classList.toggle('active', mode === 'single');
    if (proberElements.multiBtn) proberElements.multiBtn.classList.toggle('active', mode === 'multi');
    if (proberElements.multiActionsBar) proberElements.multiActionsBar.hidden = (mode !== 'multi');
    if (proberElements.quickAutoBtn) proberElements.quickAutoBtn.hidden = (mode !== 'single');
}

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
    if (!proberElements.toggle) return;

    proberElements.toggle.checked = Boolean(data.enabled);
    proberElements.toggleLabel.textContent = data.enabled ? t('prober_on') : t('prober_off');
    proberElements.toggleLabel.classList.toggle('active', Boolean(data.enabled));

    const mode = data.mode === 'multi' ? 'multi' : 'single';
    setProberMode(mode);

    if (data.interval_ms && proberElements.interval && document.activeElement !== proberElements.interval) {
        proberElements.interval.value = data.interval_ms;
        updateRateBadge(data.interval_ms);
        document.querySelectorAll('.preset-pill').forEach(btn => {
            btn.classList.toggle('active', Number(btn.dataset.ms) === Number(data.interval_ms));
        });
    }

    if (proberElements.packetsCount) {
        proberElements.packetsCount.textContent = `${Number(data.packets_sent || 0).toLocaleString()} ${currentLang === 'vi' ? 'gói' : 'pkts'}`;
    }
    if (proberElements.statusBadge) {
        if (data.enabled) {
            proberElements.statusBadge.textContent = `${t('prober_active')} (${data.interval_ms}ms)`;
            proberElements.statusBadge.className = 'badge-active';
        } else {
            proberElements.statusBadge.textContent = t('prober_idle');
            proberElements.statusBadge.className = 'badge-idle';
        }
    }

    // Chỉ nạp multi-selection từ data.target_ips một lần duy nhất lúc khởi tạo
    if (mode === 'multi' && !multiSelectionUserModified && data.target_ips && Array.isArray(data.target_ips) && data.target_ips.length > 0) {
        data.target_ips.forEach(ip => {
            if (ip && ip !== 'auto') selectedMultiIps.add(ip);
        });
        multiSelectionUserModified = true;
    }

    // Update selection summary badge & quick action pills in single mode
    if (mode === 'single') {
        const targetIp = data.target_ip || 'auto';
        if (targetIp === 'auto' || !targetIp) {
            if (proberElements.quickAutoBtn) proberElements.quickAutoBtn.classList.add('active');
            if (proberElements.selectionBadge) proberElements.selectionBadge.textContent = t('selecting_auto');
        } else {
            if (proberElements.quickAutoBtn) proberElements.quickAutoBtn.classList.remove('active');
            const matchedClient = Array.isArray(data.clients) ? data.clients.find(c => c.ip === targetIp) : null;
            if (proberElements.selectionBadge) {
                if (matchedClient) {
                    proberElements.selectionBadge.textContent = `${t('selecting_device')}${matchedClient.hostname || matchedClient.ip} (${matchedClient.ip})`;
                } else {
                    proberElements.selectionBadge.textContent = `${t('selecting_device')}${targetIp}`;
                }
            }
        }
    } else {
        updateMultiCount();
    }

    if (isFullRender) {
        renderCardsTables(data.cards, data.clients, mode, data.target_ip);
    }

    let activeClient = null;
    let activeCard = null;
    if (Array.isArray(data.clients)) {
        if (data.target_ip && data.target_ip !== 'auto') {
            activeClient = data.clients.find(c => c.ip === data.target_ip || c.mac === data.target_ip);
        }
        if (!activeClient && data.clients.length > 0) {
            activeClient = data.clients[0];
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

function updateMultiCount() {
    if (proberElements.multiCount) {
        proberElements.multiCount.textContent = `${t('selected_count')}${selectedMultiIps.size}`;
    }
    if (proberElements.selectionBadge && currentProberConfig.mode === 'multi') {
        proberElements.selectionBadge.textContent = `${t('selected_count')}${selectedMultiIps.size} ${t('devices_count')}`;
    }
}

function renderCardsTables(cards, clients, mode, currentTargetIp) {
    if (!proberElements.cardsContainer) return;

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

        if (mode === 'multi' && cardStations.length > 0) {
            const allSelectedInCard = cardStations.every(c => selectedMultiIps.has(c.ip || c.mac));
            const cardBulkBtn = document.createElement('button');
            cardBulkBtn.type = 'button';
            cardBulkBtn.className = 'card-bulk-btn';
            cardBulkBtn.textContent = allSelectedInCard ? t('deselect_card') : t('select_card');
            cardBulkBtn.addEventListener('click', e => {
                e.stopPropagation();
                multiSelectionUserModified = true;
                if (allSelectedInCard) {
                    cardStations.forEach(c => selectedMultiIps.delete(c.ip || c.mac));
                } else {
                    cardStations.forEach(c => {
                        const k = c.ip || c.mac;
                        if (k) selectedMultiIps.add(k);
                    });
                }
                updateMultiCount();
                saveProberConfig();
            });
            right.appendChild(cardBulkBtn);
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
                <th style="width: 48px; text-align: center;">${t('col_select')}</th>
                <th>${t('col_device')}</th>
                <th>${t('col_ip')}</th>
                <th>${t('col_mac')}</th>
                <th>${t('col_signal')}</th>
                <th>${t('col_bitrate')}</th>
                <th>${t('col_activity')}</th>
                <th style="width: 80px; text-align: center;">${t('col_action')}</th>
            </tr>
        `;
        table.appendChild(thead);

        const tbody = document.createElement('tbody');

        if (cardStations.length === 0) {
            const emptyTr = document.createElement('tr');
            emptyTr.className = 'card-empty-row';
            emptyTr.innerHTML = `<td colspan="8" class="card-empty-cell">${t('no_card_stations')}</td>`;
            tbody.appendChild(emptyTr);
        } else {
            cardStations.forEach(c => {
                const tr = document.createElement('tr');
                tr.className = 'wifi-station-row';
                const clientKey = c.ip || c.mac;

                let isSelected = false;
                if (mode === 'single') {
                    isSelected = (currentTargetIp && currentTargetIp !== 'auto' && currentTargetIp === c.ip);
                } else {
                    isSelected = selectedMultiIps.has(clientKey);
                }

                if (isSelected) tr.classList.add('selected-row');

                // 1. Selector input cell (hỗ trợ tích để chọn và tích lại để bỏ chọn)
                const tdSelect = document.createElement('td');
                tdSelect.style.textAlign = 'center';
                const input = document.createElement('input');
                input.type = (mode === 'single') ? 'radio' : 'checkbox';
                if (mode === 'single') {
                    input.name = 'target-device-radio';
                }
                input.checked = isSelected;

                const toggleSelection = () => {
                    if (mode === 'single') {
                        if (!c.ip) return;
                        if (currentProberConfig.target_ip === c.ip) {
                            // Đang chọn -> BỎ CHỌN (về tự động chọn sóng mạnh nhất)
                            currentProberConfig.target_ip = 'auto';
                            input.checked = false;
                            document.querySelectorAll('.wifi-station-row').forEach(r => r.classList.remove('selected-row'));
                            updateAvailableBandwidths(null, card);
                        } else {
                            // Chưa chọn -> CHỌN thiết bị này
                            currentProberConfig.target_ip = c.ip;
                            input.checked = true;
                            document.querySelectorAll('.wifi-station-row').forEach(r => r.classList.remove('selected-row'));
                            tr.classList.add('selected-row');
                            updateAvailableBandwidths(c, card);
                        }
                        renderCfrUI();
                        saveProberConfig();
                    } else {
                        // Multi mode: tích hoặc bỏ tích
                        multiSelectionUserModified = true;
                        if (selectedMultiIps.has(clientKey)) {
                            // Đang chọn -> BỎ CHỌN
                            selectedMultiIps.delete(clientKey);
                            input.checked = false;
                            tr.classList.remove('selected-row');
                        } else {
                            // Chưa chọn -> CHỌN
                            selectedMultiIps.add(clientKey);
                            input.checked = true;
                            tr.classList.add('selected-row');
                        }
                        updateMultiCount();
                        saveProberConfig();
                    }
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

                // 2. Device Name cell (không dùng icon emoji trẻ con)
                const tdDevice = document.createElement('td');
                tdDevice.innerHTML = `<span class="device-name-cell"><span class="device-name-text">${escapeHtml(c.hostname || 'Device')}</span></span>`;

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

                // 5. Signal / SNR cell
                const tdSignal = document.createElement('td');
                const sigClass = getSignalClass(c.snr, c.signal_dbm);
                const snrPart = (c.snr && c.snr !== '—') ? ` (SNR ${c.snr})` : '';
                const sigText = (c.signal_dbm && c.signal_dbm !== '—') ? `${c.signal_dbm}${snrPart}` : (c.snr ? `SNR ${c.snr}` : '—');
                tdSignal.innerHTML = `<span class="signal-cell"><span class="signal-indicator-dot ${sigClass}"></span><span>${sigText}</span></span>`;

                // 6. Bitrate cell
                const tdRate = document.createElement('td');
                const bwBadge = c.width_str ? `<span class="station-bw-badge">${c.width_str}</span>` : '';
                const rateText = (c.bitrate && c.bitrate !== '—') ? c.bitrate : '—';
                tdRate.innerHTML = `<span>${rateText}</span>${bwBadge}`;

                // 7. Activity / Last Seen cell
                const tdActive = document.createElement('td');
                const rawAct = (c.inactive_ms && c.inactive_ms !== '—') ? c.inactive_ms : 'Active';
                if (rawAct === '0 ms' || rawAct === '0ms' || rawAct === 'Active') {
                    tdActive.innerHTML = `<span style="color:#4be2d0" title="Vừa truyền nhận dữ liệu (${rawAct})">${currentLang === 'vi' ? 'Vừa xong (0ms)' : 'Just now (0ms)'}</span>`;
                } else {
                    tdActive.textContent = rawAct;
                }

                // 8. Quick Action (CFR Capture) cell
                const tdAction = document.createElement('td');
                tdAction.style.textAlign = 'center';
                const isCapturingThis = cfrState.running && (
                    (cfrState.target_mac && c.mac && cfrState.target_mac.toUpperCase() === c.mac.toUpperCase()) ||
                    (cfrState.target_ip && c.ip && cfrState.target_ip === c.ip)
                );
                if (isCapturingThis) {
                    tr.classList.add('capturing-row');
                    const pill = document.createElement('span');
                    pill.className = 'capturing-pill';
                    pill.textContent = t('pill_capturing');
                    tdAction.appendChild(pill);
                } else {
                    const btnCapture = document.createElement('button');
                    btnCapture.type = 'button';
                    btnCapture.className = 'quick-capture-btn';
                    btnCapture.textContent = t('btn_quick_capture');
                    btnCapture.title = t('cfr_btn_start');
                    btnCapture.addEventListener('click', e => {
                        e.stopPropagation();
                        updateAvailableBandwidths(c, card);
                        startCfrCapture({
                            mac: c.mac,
                            ip: c.ip,
                            iface: c.iface || card.iface,
                            name: c.hostname
                        });
                    });
                    tdAction.appendChild(btnCapture);
                }

                tr.append(tdSelect, tdDevice, tdIp, tdMac, tdSignal, tdRate, tdActive, tdAction);
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
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
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

let isSavingProber = false;
let saveSuccessTimeout = null;

async function saveProberConfig() {
    if (isSavingProber) return;
    if (!proberElements.applyBtn) return;
    isSavingProber = true;

    clearTimeout(saveSuccessTimeout);
    proberElements.applyBtn.textContent = t('applying');
    proberElements.applyBtn.disabled = true;

    try {
        const enabled = proberElements.toggle ? proberElements.toggle.checked : false;
        const mode = currentProberConfig.mode;
        let target_ip = currentProberConfig.target_ip || 'auto';
        let target_ips = [];

        if (mode === 'single') {
            target_ips = (target_ip && target_ip !== 'auto') ? [target_ip] : [];
        } else {
            target_ips = Array.from(selectedMultiIps);
            target_ip = target_ips.length > 0 ? target_ips[0] : '';
        }

        const interval_ms = Math.max(10, Math.min(Number(proberElements.interval?.value) || 50, 2000));

        const res = await fetch('/api/prober', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ enabled, mode, target_ip, target_ips, interval_ms })
        });
        if (res.ok) {
            const data = await res.json();
            currentProberConfig = data;
            renderProberUI(data);

            proberElements.applyBtn.textContent = t('saved_btn');
            saveSuccessTimeout = setTimeout(() => {
                if (proberElements.applyBtn) {
                    proberElements.applyBtn.textContent = t('apply_btn');
                    proberElements.applyBtn.disabled = false;
                }
            }, 800);
        } else {
            proberElements.applyBtn.textContent = t('apply_btn');
            proberElements.applyBtn.disabled = false;
        }
    } catch (e) {
        console.error('Cannot save prober config', e);
        if (proberElements.applyBtn) {
            proberElements.applyBtn.textContent = t('apply_btn');
            proberElements.applyBtn.disabled = false;
        }
    } finally {
        isSavingProber = false;
    }
}

// Mode chip listeners
if (proberElements.singleBtn) {
    proberElements.singleBtn.addEventListener('click', () => {
        setProberMode('single');
        saveProberConfig();
    });
}
if (proberElements.multiBtn) {
    proberElements.multiBtn.addEventListener('click', () => {
        setProberMode('multi');
        saveProberConfig();
    });
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

// Multi select actions
if (proberElements.selectAllBtn) {
    proberElements.selectAllBtn.addEventListener('click', () => {
        multiSelectionUserModified = true;
        if (Array.isArray(currentProberConfig.clients)) {
            currentProberConfig.clients.forEach(c => {
                const key = c.ip || c.mac;
                if (key) selectedMultiIps.add(key);
            });
            updateMultiCount();
            renderCardsTables(currentProberConfig.cards, currentProberConfig.clients, 'multi', '');
            saveProberConfig();
        }
    });
}
if (proberElements.deselectAllBtn) {
    proberElements.deselectAllBtn.addEventListener('click', () => {
        multiSelectionUserModified = true;
        selectedMultiIps.clear();
        updateMultiCount();
        renderCardsTables(currentProberConfig.cards, currentProberConfig.clients, 'multi', '');
        saveProberConfig();
    });
}

if (proberElements.interval) {
    proberElements.interval.addEventListener('input', e => {
        updateRateBadge(e.target.value);
        document.querySelectorAll('.preset-pill').forEach(btn => {
            btn.classList.toggle('active', Number(btn.dataset.ms) === Number(e.target.value));
        });
    });
}

document.querySelectorAll('.preset-pill').forEach(btn => {
    btn.addEventListener('click', () => {
        const ms = Number(btn.dataset.ms);
        if (proberElements.interval) {
            proberElements.interval.value = ms;
            updateRateBadge(ms);
        }
        document.querySelectorAll('.preset-pill').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        saveProberConfig();
    });
});

if (proberElements.toggle) {
    proberElements.toggle.addEventListener('change', saveProberConfig);
}
if (proberElements.applyBtn) {
    proberElements.applyBtn.addEventListener('click', saveProberConfig);
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

// Định kỳ tự động cập nhật danh sách thiết bị kết nối & trạng thái
window.setInterval(() => {
    // Khi đang thu CFR: thăm dò giãn cách 8s để tối ưu CPU router; khi rảnh: thăm dò mỗi 4s
    if (!cfrState.running) {
        loadProberStatus(false);
    }
}, 4000);

window.setInterval(() => {
    if (cfrState.running) {
        loadProberStatus(false);
    }
}, 8000);

window.setInterval(() => {
    if (cfrState.running) {
        fetchCfrStatus();
    }
}, 1500);

window.setInterval(() => {
    if (lastFrameAt && performance.now() - lastFrameAt > 6000) {
        setStatus('waiting', t('status_packet_wait'));
    }
}, 1000);
