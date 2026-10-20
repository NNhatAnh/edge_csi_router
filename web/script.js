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
        prober_badge: 'HƯỚNG 1 · ACTIVE PROBING',
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
        col_activity: 'Hoạt động',
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
        stat_sent: 'Đã gửi',
        stat_status: 'Trạng thái',
        prober_active: 'Đang ping',
        prober_idle: 'Tạm dừng',
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
        stat_sent: 'Packets sent',
        stat_status: 'Status',
        prober_active: 'Probing',
        prober_idle: 'Idle',
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

    const customInput = document.getElementById('prober-ip-custom');
    if (customInput) customInput.placeholder = t('custom_ip_placeholder');

    setView(activeView);
    if (latestFrame) {
        updateDomMetrics(latestFrame, false, performance.now());
    }
    renderProberUI(currentProberConfig);
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
    if (domElements.motionCaption) {
        if (!frame.motion_ready) {
            domElements.motionCaption.textContent = currentLang === 'vi'
                ? `Cửa sổ median ${frame.motion_window_samples || 0}/30 frame`
                : `Median window ${frame.motion_window_samples || 0}/30 frames`;
        } else if (!frame.motion_calibrated) {
            domElements.motionCaption.textContent = frame.motion_calibration_stable
                ? (currentLang === 'vi' ? `Tín hiệu ổn định · ${frame.motion_calibration_frames || 0}/40 mẫu` : `Stable signal · ${frame.motion_calibration_frames || 0}/40 samples`)
                : (currentLang === 'vi' ? `Tín hiệu dao động · chờ ổn định ${frame.motion_calibration_frames || 0}/40` : `Jittering · awaiting baseline ${frame.motion_calibration_frames || 0}/40`);
        } else {
            domElements.motionCaption.textContent = currentLang === 'vi'
                ? `Ngưỡng cao ${Number(frame.motion_threshold_high).toFixed(4)} · thấp ${Number(frame.motion_threshold_low).toFixed(4)}`
                : `High threshold ${Number(frame.motion_threshold_high).toFixed(4)} · low ${Number(frame.motion_threshold_low).toFixed(4)}`;
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
    dot.className = `status-dot ${status}`;
    label.textContent = status === 'live' ? t('status_live') : status === 'waiting' ? t('status_waiting') : t('status_offline');
    document.getElementById('last-update').textContent = description;
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
        drawLineChart(values, '#ffb86b', width, height, 'Motion score', false);
        const legend = [{ name: 'Motion score', color: '#ffb86b' }];
        if (latestFrame.motion_calibrated) {
            drawLineChart([latestFrame.motion_threshold_high], '#ff7f9d', width, height, 'Ngưỡng cao', true);
            drawLineChart([latestFrame.motion_threshold_low], '#4be2d0', width, height, 'Ngưỡng thấp', true);
            legend.push(
                { name: `${currentLang === 'vi' ? 'Ngưỡng cao' : 'High thres'} (${latestFrame.motion_threshold_high.toFixed(3)})`, color: '#ff7f9d' },
                { name: `${currentLang === 'vi' ? 'Ngưỡng thấp' : 'Low thres'} (${latestFrame.motion_threshold_low.toFixed(3)})`, color: '#4be2d0' }
            );
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
    quickManualToggle: document.getElementById('quick-manual-toggle'),
    manualIpWrap: document.getElementById('manual-ip-wrap'),
    ipCustom: document.getElementById('prober-ip-custom'),
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
let isManualIpMode = false;
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
    if (proberElements.quickManualToggle) proberElements.quickManualToggle.hidden = (mode !== 'single');
    if (mode === 'multi') {
        if (proberElements.manualIpWrap) proberElements.manualIpWrap.hidden = true;
    }
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

function renderProberUI(data) {
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
            if (proberElements.quickManualToggle) proberElements.quickManualToggle.classList.remove('active');
            if (proberElements.manualIpWrap && !isManualIpMode) proberElements.manualIpWrap.hidden = true;
            if (proberElements.selectionBadge) proberElements.selectionBadge.textContent = t('selecting_auto');
        } else {
            // Check if matches an existing client
            const matchedClient = Array.isArray(data.clients) ? data.clients.find(c => c.ip === targetIp) : null;
            if (matchedClient) {
                if (proberElements.quickAutoBtn) proberElements.quickAutoBtn.classList.remove('active');
                if (proberElements.quickManualToggle) proberElements.quickManualToggle.classList.remove('active');
                if (proberElements.manualIpWrap && !isManualIpMode) proberElements.manualIpWrap.hidden = true;
                if (proberElements.selectionBadge) {
                    proberElements.selectionBadge.textContent = `${t('selecting_device')}${matchedClient.hostname || matchedClient.ip} (${matchedClient.ip})`;
                }
            } else {
                // Manual IP
                if (proberElements.quickAutoBtn) proberElements.quickAutoBtn.classList.remove('active');
                if (proberElements.quickManualToggle) proberElements.quickManualToggle.classList.add('active');
                if (proberElements.manualIpWrap) proberElements.manualIpWrap.hidden = false;
                if (proberElements.ipCustom && document.activeElement !== proberElements.ipCustom) {
                    proberElements.ipCustom.value = targetIp;
                }
                if (proberElements.selectionBadge) {
                    proberElements.selectionBadge.textContent = `${t('selecting_manual')}${targetIp}`;
                }
            }
        }
    } else {
        updateMultiCount();
    }

    renderCardsTables(data.cards, data.clients, mode, data.target_ip);
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

        left.append(radioSvg, devBadge, bandTag, ssidBadge);

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
                        } else {
                            // Chưa chọn -> CHỌN thiết bị này
                            currentProberConfig.target_ip = c.ip;
                            input.checked = true;
                            document.querySelectorAll('.wifi-station-row').forEach(r => r.classList.remove('selected-row'));
                            tr.classList.add('selected-row');
                        }
                        if (proberElements.ipCustom) proberElements.ipCustom.value = '';
                        isManualIpMode = false;
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
                    if (e.target.tagName.toLowerCase() === 'input') return;
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
                tdRate.textContent = (c.bitrate && c.bitrate !== '—') ? c.bitrate : '—';

                // 7. Activity cell
                const tdActive = document.createElement('td');
                tdActive.textContent = (c.inactive_ms && c.inactive_ms !== '—') ? c.inactive_ms : 'Active';

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
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

async function loadProberStatus() {
    try {
        const res = await fetch('/api/prober');
        if (!res.ok) return;
        const data = await res.json();
        currentProberConfig = data;
        renderProberUI(data);
    } catch (e) {
        console.warn('Cannot load prober status', e);
    }
}

async function saveProberConfig() {
    if (!proberElements.applyBtn) return;
    const prevText = proberElements.applyBtn.textContent;
    proberElements.applyBtn.textContent = t('applying');
    proberElements.applyBtn.disabled = true;

    try {
        const enabled = proberElements.toggle ? proberElements.toggle.checked : false;
        const mode = currentProberConfig.mode;
        let target_ip = currentProberConfig.target_ip || 'auto';
        let target_ips = [];

        if (mode === 'single') {
            if (proberElements.ipCustom && isManualIpMode && proberElements.ipCustom.value.trim() !== '') {
                target_ip = proberElements.ipCustom.value.trim();
            }
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
        }
    } catch (e) {
        console.error('Cannot save prober config', e);
    } finally {
        proberElements.applyBtn.textContent = prevText;
        proberElements.applyBtn.disabled = false;
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
        isManualIpMode = false;
        if (proberElements.manualIpWrap) proberElements.manualIpWrap.hidden = true;
        if (proberElements.ipCustom) proberElements.ipCustom.value = '';
        saveProberConfig();
    });
}

// Quick Manual IP toggle listener
if (proberElements.quickManualToggle) {
    proberElements.quickManualToggle.addEventListener('click', () => {
        isManualIpMode = !isManualIpMode;
        if (proberElements.manualIpWrap) {
            proberElements.manualIpWrap.hidden = !isManualIpMode;
            if (isManualIpMode && proberElements.ipCustom) {
                proberElements.ipCustom.focus();
            }
        }
        proberElements.quickManualToggle.classList.toggle('active', isManualIpMode);
    });
}

if (proberElements.ipCustom) {
    proberElements.ipCustom.addEventListener('keydown', e => {
        if (e.key === 'Enter') {
            const val = proberElements.ipCustom.value.trim();
            if (val) {
                currentProberConfig.target_ip = val;
                saveProberConfig();
            }
        }
    });
    proberElements.ipCustom.addEventListener('blur', () => {
        const val = proberElements.ipCustom.value.trim();
        if (val && val !== currentProberConfig.target_ip) {
            currentProberConfig.target_ip = val;
            saveProberConfig();
        }
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

if (proberElements.refreshBtn) {
    proberElements.refreshBtn.addEventListener('click', () => {
        proberElements.refreshBtn.style.transform = 'rotate(360deg)';
        setTimeout(() => proberElements.refreshBtn.style.transform = '', 500);
        loadProberStatus();
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

applyLanguage(currentLang);
resizeCanvas();
connectStream();
loadProberStatus();
window.setInterval(loadProberStatus, 3000);
window.setInterval(() => {
    if (lastFrameAt && performance.now() - lastFrameAt > 6000) {
        setStatus('waiting', t('status_packet_wait'));
    }
}, 1000);
