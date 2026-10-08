const views = {
    overview: { title: 'Tổng quan tín hiệu', kicker: 'LIVE ACTIVITY', chart: 'Motion score & ngưỡng phát hiện' },
    amplitude: { title: 'Biên độ CSI', kicker: 'CHANNEL RESPONSE', chart: 'Biên độ theo tone · raw I/Q' },
    phase: { title: 'Pha CSI', kicker: 'PHASE RESPONSE', chart: 'Pha theo tone · radian' },
    iq: { title: 'Chòm sao I/Q', kicker: 'COMPLEX CHANNEL', chart: 'Phân bố mẫu I/Q' },
    chains: { title: 'So sánh antenna', kicker: 'RF CHAINS', chart: 'Biên độ giữa các antenna chains' },
    packet: { title: 'Thông tin gói', kicker: 'FRAME INSPECTOR', chart: 'CSI amplitude của frame hiện tại' }
};

const colors = ['#4be2d0', '#a98cff', '#ffb86b', '#71a7ff', '#ff7f9d'];
const canvas = document.getElementById('signal-chart');
const context = canvas.getContext('2d');
const emptyOverlay = document.getElementById('chart-empty');
const chainSelect = document.getElementById('chain-select');
const calibrateButton = document.getElementById('calibrate-button');
let activeView = 'overview';
let latestFrame = null;
let motionHistory = [];
let receiveTimes = [];
let frameRate = 0;
let lastFrameAt = 0;

function setStatus(status, description) {
    const dot = document.getElementById('status-dot');
    const label = document.getElementById('status');
    dot.className = `status-dot ${status}`;
    label.textContent = status === 'live' ? 'LIVE' : status === 'waiting' ? 'ĐANG CHỜ DỮ LIỆU' : 'MẤT KẾT NỐI';
    document.getElementById('last-update').textContent = description;
}

function setView(view) {
    if (!views[view]) return;
    activeView = view;
    document.querySelectorAll('.tab-btn').forEach(button => {
        button.classList.toggle('active', button.dataset.view === view);
    });
    document.getElementById('page-title').textContent = views[view].title;
    document.getElementById('chart-kicker').textContent = views[view].kicker;
    document.getElementById('chart-title').textContent = views[view].chart;
    document.getElementById('chain-control').hidden = view === 'overview' || view === 'chains';
    drawCurrent();
}

document.querySelectorAll('.tab-btn').forEach(button => {
    button.addEventListener('click', () => setView(button.dataset.view));
});

chainSelect.addEventListener('change', drawCurrent);
calibrateButton.addEventListener('click', requestCalibration);

async function requestCalibration() {
    calibrateButton.disabled = true;
    const previousLabel = calibrateButton.textContent;
    calibrateButton.textContent = 'Đang yêu cầu…';
    try {
        const response = await fetch('/api/calibrate', { method: 'POST' });
        if (!response.ok) {
            const details = await response.text();
            throw new Error(`Calibration request failed (${response.status}): ${details}`);
        }
        motionHistory = [];
        document.getElementById('presence-value').textContent = 'HIỆU CHUẨN';
        document.getElementById('presence-caption').textContent = 'Đợi tín hiệu ổn định để lấy baseline';
        document.getElementById('motion-value').textContent = '—';
        document.getElementById('motion-caption').textContent = 'Đang khởi tạo cửa sổ 30 frame';
        setStatus('waiting', 'Đang chờ hiệu chuẩn nền mới');
        drawCurrent();
    } catch (error) {
        console.error('Unable to restart motion calibration', error);
        document.getElementById('presence-caption').textContent = 'Lỗi yêu cầu hiệu chuẩn · kiểm tra kết nối';
        setStatus('offline', 'Không thể yêu cầu hiệu chuẩn lại');
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
        setStatus('waiting', 'Chưa có frame CSI');
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
    }
    if (isNewFrame && frame.motion_ready) {
        motionHistory.push({ time: now, score: Number(frame.motion_score) || 0 });
        if (motionHistory.length > 180) motionHistory.shift();
    }

    document.getElementById('sc-count').textContent = Number(frame.subcarriers_count || 0).toLocaleString();
    document.getElementById('sc-caption').textContent = `${frame.tones_per_chain || 0} tone / chain`;
    document.getElementById('chain-count').textContent = String(frame.num_chains || frame.chains.length);
    document.getElementById('nss-caption').textContent = `Spatial streams ${frame.nss ?? '—'}`;
    document.getElementById('motion-value').textContent = frame.motion_ready
        ? (Number(frame.motion_score) || 0).toFixed(4)
        : '—';
    const motionCaption = document.getElementById('motion-caption');
    if (!frame.motion_ready) {
        motionCaption.textContent = `Cửa sổ median ${frame.motion_window_samples || 0}/30 frame`;
    } else if (!frame.motion_calibrated) {
        motionCaption.textContent = frame.motion_calibration_stable
            ? `Tín hiệu ổn định · ${frame.motion_calibration_frames || 0}/50 mẫu`
            : `Tín hiệu dao động · chờ ổn định ${frame.motion_calibration_frames || 0}/50`;
    } else {
        motionCaption.textContent =
            `Ngưỡng cao ${Number(frame.motion_threshold_high).toFixed(4)} · thấp ${Number(frame.motion_threshold_low).toFixed(4)}`;
    }
    const presenceValue = document.getElementById('presence-value');
    const presenceCaption = document.getElementById('presence-caption');
    if (!frame.motion_ready || !frame.motion_calibrated) {
        presenceValue.textContent = 'HIỆU CHUẨN';
        presenceCaption.textContent = !frame.motion_ready
            ? 'Đang tích lũy cửa sổ tín hiệu'
            : frame.motion_calibration_stable
                ? `Đang lấy nền ổn định ${frame.motion_calibration_frames || 0}/50`
                : `Tín hiệu chưa ổn định · ${frame.motion_calibration_frames || 0}/50`;
    } else {
        presenceValue.textContent = frame.presence_detected ? 'CÓ CHUYỂN ĐỘNG' : 'YÊN TĨNH';
        presenceCaption.textContent = 'Bật khi vượt ngưỡng cao · tắt sau 15 frame thấp';
    }
    document.getElementById('frame-rate').textContent = frameRate.toFixed(1);
    document.getElementById('chip-tsf').textContent = frame.chip_tsf_us ? `${Number(frame.chip_tsf_us).toLocaleString()} µs` : '—';
    document.getElementById('ppdu-id').textContent = frame.phy_ppdu_id ?? '—';
    document.getElementById('bandwidth').textContent = frame.channel_bw_code ?? '—';
    document.getElementById('ap-mac').textContent = frame.ap_mac || '—';
    document.getElementById('frame-id').textContent = `PPDU ${frame.phy_ppdu_id ?? '—'}`;
    if (isNewFrame) setStatus('live', `Nhận frame ${new Date().toLocaleTimeString()}`);
    drawCurrent();
}

function connectStream() {
    if (!('EventSource' in window)) {
        setStatus('offline', 'Trình duyệt không hỗ trợ EventSource');
        return;
    }
    const source = new EventSource('/events');
    source.addEventListener('csi', event => {
        try {
            acceptFrame(JSON.parse(event.data));
        } catch (error) {
            console.error('Invalid CSI event payload', error);
            setStatus('offline', 'Dữ liệu stream không hợp lệ');
        }
    });
    source.onopen = () => {
        if (!latestFrame) setStatus('waiting', 'Đã nối backend · chờ CSI');
    };
    source.onerror = () => {
        if (!latestFrame) setStatus('offline', 'Đang thử kết nối lại…');
        else setStatus('waiting', 'Stream gián đoạn · đang nối lại');
    };
}

function resizeCanvas() {
    const bounds = canvas.getBoundingClientRect();
    const ratio = Math.max(1, window.devicePixelRatio || 1);
    const width = Math.round(bounds.width * ratio);
    const height = Math.round(bounds.height * ratio);
    if (canvas.width !== width || canvas.height !== height) {
        canvas.width = width;
        canvas.height = height;
    }
    drawCurrent();
}

function drawCurrent() {
    const width = canvas.width;
    const height = canvas.height;
    if (!width || !height) return;
    context.clearRect(0, 0, width, height);
    drawGrid(width, height);

    if (!latestFrame) {
        emptyOverlay.hidden = false;
        document.getElementById('chart-legend').replaceChildren();
        return;
    }
    emptyOverlay.hidden = true;
    const chainIndex = Math.max(0, Number(chainSelect.value) || 0);
    const chain = latestFrame.chains[chainIndex];

    if (activeView === 'overview') {
        const references = latestFrame.motion_calibrated
            ? [
                { value: Number(latestFrame.motion_threshold_high), color: '#ffb86b' },
                { value: Number(latestFrame.motion_threshold_low), color: '#a98cff' }
            ]
            : [];
        drawLineChart(
            motionHistory.map(point => point.score),
            colors[0],
            width,
            height,
            'Motion score',
            false,
            references
        );
        const legend = [{ name: '1 − Pearson r · median 30', color: colors[0] }];
        if (references.length) {
            legend.push(
                { name: 'Ngưỡng cao', color: references[0].color },
                { name: 'Ngưỡng thấp', color: references[1].color }
            );
        }
        setLegend(legend);
    } else if (activeView === 'amplitude' || activeView === 'packet') {
        drawLineChart(chain?.amplitudes || [], colors[chainIndex % colors.length], width, height, 'Amplitude');
        setLegend([{ name: `Chain ${chainIndex} · amplitude`, color: colors[chainIndex % colors.length] }]);
    } else if (activeView === 'phase') {
        drawLineChart(chain?.phases || [], colors[chainIndex % colors.length], width, height, 'Phase (rad)');
        setLegend([{ name: `Chain ${chainIndex} · phase (rad)`, color: colors[chainIndex % colors.length] }]);
    } else if (activeView === 'iq') {
        drawConstellation(chain, width, height);
        setLegend([{ name: `Chain ${chainIndex} · I/Q samples`, color: colors[chainIndex % colors.length] }]);
    } else if (activeView === 'chains') {
        const legend = latestFrame.chains.map((item, index) => ({
            name: `Chain ${index}`,
            color: colors[index % colors.length]
        }));
        latestFrame.chains.forEach((item, index) => drawLineChart(
            item.amplitudes || [],
            colors[index % colors.length],
            width,
            height,
            `Chain ${index}`,
            true
        ));
        setLegend(legend);
    }
}

function drawGrid(width, height) {
    const ratio = Math.max(1, window.devicePixelRatio || 1);
    const left = 48 * ratio;
    const right = 14 * ratio;
    const top = 17 * ratio;
    const bottom = 31 * ratio;
    context.lineWidth = 1;
    context.strokeStyle = 'rgba(129, 150, 181, 0.12)';
    context.fillStyle = '#77869d';
    context.font = `${9 * ratio}px "DM Mono", monospace`;
    context.textAlign = 'right';
    context.textBaseline = 'middle';

    for (let row = 0; row <= 4; row++) {
        const y = top + (height - top - bottom) * row / 4;
        context.beginPath();
        context.moveTo(left, y);
        context.lineTo(width - right, y);
        context.stroke();
    }
    context.textAlign = 'center';
    context.textBaseline = 'top';
    for (let column = 0; column <= 4; column++) {
        const x = left + (width - left - right) * column / 4;
        context.beginPath();
        context.moveTo(x, top);
        context.lineTo(x, height - bottom);
        context.stroke();
    }
}

function drawLineChart(values, color, width, height, label, overlay = false, references = []) {
    if (!Array.isArray(values) || values.length === 0) return;
    const ratio = Math.max(1, window.devicePixelRatio || 1);
    const left = 48 * ratio;
    const right = 14 * ratio;
    const top = 17 * ratio;
    const bottom = 31 * ratio;
    const plotWidth = width - left - right;
    const plotHeight = height - top - bottom;
    const referenceValues = references
        .map(reference => reference.value)
        .filter(value => Number.isFinite(value));
    let min = Math.min(...values, ...referenceValues);
    let max = Math.max(...values, ...referenceValues);
    if (overlay) {
        latestFrame.chains.forEach(item => {
            min = Math.min(min, ...item.amplitudes);
            max = Math.max(max, ...item.amplitudes);
        });
    }
    if (max === min) {
        const padding = Math.max(Math.abs(max) * 0.08, 1);
        min -= padding;
        max += padding;
    } else {
        const padding = (max - min) * 0.08;
        min -= padding;
        max += padding;
    }

    context.strokeStyle = color;
    context.lineWidth = 1.8 * ratio;
    context.lineJoin = 'round';
    context.lineCap = 'round';
    context.beginPath();
    values.forEach((value, index) => {
        const x = left + (values.length === 1 ? plotWidth / 2 : index / (values.length - 1) * plotWidth);
        const y = top + (max - value) / (max - min) * plotHeight;
        if (index === 0) context.moveTo(x, y);
        else context.lineTo(x, y);
    });
    context.stroke();

    references.forEach(reference => {
        if (!Number.isFinite(reference.value)) return;
        const y = top + (max - reference.value) / (max - min) * plotHeight;
        context.save();
        context.strokeStyle = reference.color;
        context.lineWidth = 1.2 * ratio;
        context.setLineDash([5 * ratio, 4 * ratio]);
        context.beginPath();
        context.moveTo(left, y);
        context.lineTo(width - right, y);
        context.stroke();
        context.restore();
    });

    if (!overlay) {
        context.fillStyle = '#8998ad';
        context.font = `${9 * ratio}px "DM Mono", monospace`;
        context.textAlign = 'left';
        context.textBaseline = 'bottom';
        context.fillText(max.toPrecision(4), 5 * ratio, top + 4 * ratio);
        context.textBaseline = 'top';
        context.fillText(min.toPrecision(4), 5 * ratio, height - bottom - 4 * ratio);
        context.textAlign = 'right';
        context.fillStyle = '#718099';
        context.fillText(label, width - right, height - 7 * ratio);
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
    context.fillStyle = '#8a98ad';
    context.font = `${9 * ratio}px "DM Mono", monospace`;
    context.textAlign = 'right';
    context.textBaseline = 'bottom';
    context.fillText('I', width - right, height - 7 * ratio);
    context.textAlign = 'left';
    context.textBaseline = 'top';
    context.fillText('Q', left, top);
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

window.addEventListener('resize', resizeCanvas);
if ('ResizeObserver' in window) new ResizeObserver(resizeCanvas).observe(canvas);
setView('overview');
resizeCanvas();
connectStream();
window.setInterval(() => {
    if (lastFrameAt && performance.now() - lastFrameAt > 3000) {
        setStatus('waiting', 'Chưa có frame mới trong 3 giây');
    }
}, 1000);
