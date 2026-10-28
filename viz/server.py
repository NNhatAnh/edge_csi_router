"""3D visualization CSI live từ router 11.1 (mức 1: hiển thị TÍN HIỆU, không suy ra vị trí/tư thế người).

    .venv/bin/python viz/server.py [--router 192.168.11.1] [--port 8090] [--period 20]   → http://127.0.0.1:8090/

11.1 tự request CFR tới peer 4.1 trên iface STA `phy00.0-1` (hoặc `--csi-peer <MAC>` = điện thoại trên AP), đọc relay qua
ssh (stdout), parse bằng viz/relay.py; đẩy biên độ CSI 10 lần/s, mỗi 0.4 s tính features.invariant_features trên cửa sổ
1.6 s (chỉ hiển thị; model dùng 3.2 s), báo rõ cửa sổ bị bỏ vì gap > 300 ms hoặc thiếu bản ghi; JSON
qua Server-Sent Events `/events`. Remote tự trả request + enable_cfr về 0 khi ssh đứt (trap EXIT; vòng đọc tự thoát khi stdin của phiên ssh đóng).
Nền (cho mức chuyển động) = phân vị 20 % của 60 s gần nhất — shortcut: thích nghi, sai khi chuyển động kéo dài > 60 s;
dùng nền hiệu chỉnh cố định nếu cần.
"""
import argparse, collections, json, math, re, socket, subprocess, sys, threading, time
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "sensing"))
sys.path.insert(0, str(HERE))
import features as F  # noqa: E402
import relay  # noqa: E402
from single_ap_position import fuse  # noqa: E402
import shape as SH  # noqa: E402
import draw as DR  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--router", default="192.168.11.1")
ap.add_argument("--sta-if", default="phy00.0-1", help="iface STA của 11.1 (link tới 4.1)")
ap.add_argument("--csi-peer", default="", help="MAC lấy CSI trên --ap-if (vd. điện thoại); trống = peer 4.1 trên --sta-if")
ap.add_argument("--hw", default="ath12k_hw0")
ap.add_argument("--period", type=int, default=20, help="chu kỳ request CFR (ms, 20–1000)")
ap.add_argument("--port", type=int, default=8090)
ap.add_argument("--single-ap", action="store_true", help="chỉ dùng 11.1; tắt luồng CFR link STA dễ treo khi peer mất")
ap.add_argument("--position-file", type=Path, help="JSON {MAC: {aoa: ..., ftm: ...}} từ các bộ đo đã xác minh")
ap.add_argument("--phone", action="append", default=[], help="MAC điện thoại trên AP 11.1; lặp lại để hiện nhiều máy")
ap.add_argument("--ap-if", default="phy00.0-0", help="iface AP của 11.1 (điện thoại kết nối vào đây)")
ap.add_argument("--model-router", default="192.168.4.1",
                help="router chạy daemon wifi_sensing (model vẫy tay); '' = không hiện model")
ap.add_argument("--shape-model", type=Path, help="models/<ts>_shape/shape_rf.pkl (sensing/shape.py): nhận diện hình vẽ live")
ap.add_argument("--draw-port", type=int, default=0, help="HTTPS cho trang vẽ trên iPhone (vd. 8443; 0 = tắt), cert từ tools/make_cert.sh")
ap.add_argument("--draw-host", default="192.168.11.178", help="IP của PC phía mạng 11.1 (nghe ở đây; phải khớp SAN của cert)")
ap.add_argument("--draw-url-host", default="", help="IP iPhone mở trang (vd. 192.168.5.240 = WAN 11.1 chuyển cổng về PC khi iPhone ở mạng 5.1)")
ap.add_argument("--no-csi", action="store_true", help="không đọc CFR (để collector thu CSI cùng lúc)")
A = ap.parse_args()
if A.single_ap:
    A.model_router = ""
assert A.csi_peer == "" or re.fullmatch(r"([0-9a-f]{2}:){5}[0-9a-f]{2}", A.csi_peer)
assert A.model_router == "" or re.fullmatch(r"\d+\.\d+\.\d+\.\d+", A.model_router)
assert all(re.fullmatch(r"([0-9a-f]{2}:){5}[0-9a-f]{2}", mac.lower()) for mac in A.phone)
assert re.fullmatch(r"\d+\.\d+\.\d+\.\d+", A.router) and re.fullmatch(r"[\w.-]+", A.sta_if + A.ap_if + A.hw) and 20 <= A.period <= 1000

REMOTE = r"""
D=/sys/kernel/debug/ieee80211/phy00; EN=$D/{hw}/enable_cfr; R=$D/{hw}/cfr_capture0
P=$(ls -d $D/netdev:{sta}/stations/{peer}/ 2>/dev/null | head -1)
[ -n "$P" ] || {{ echo "VIZERR không có peer {peer} trên {sta}" >&2; exit 2; }}
REQ=${{P}}cfr_capture; [ -e $REQ ] || REQ=${{P}}link-0/cfr_capture
[ "$(cat $EN)" = 0 ] || {{ echo "VIZERR enable_cfr đã = 1 (đang có người thu)" >&2; exit 3; }}
trap 'echo 0 > $REQ 2>/dev/null; echo 0 > $EN' EXIT
trap 'exit 1' HUP INT TERM PIPE
echo 1 > $EN; echo "1 2 {period} 0" > $REQ
echo "VIZPEER $(basename $P)" >&2
# dropbear không có pty giữ phiên sống khi client chết → canh stdin: client/server Python dừng → EOF → TERM → trap trả CFR
exec 3<&0; ( cat <&3 > /dev/null; kill $$ ) &  # nền không tương tác bị gán stdin=/dev/null: phải chuyển tường minh
# STA mất beacon → nối lại → driver tạo peer mới, request CFR bị xoá (cfr_enabled = 0): kiểm mỗi ~1 s và gửi lại
i=0; while :; do cat $R || exit 1; usleep 50000; i=$((i+1)); [ $((i % 20)) = 0 ] || continue
  P=$(ls -d $D/netdev:{sta}/stations/{peer}/ 2>/dev/null | head -1); [ -n "$P" ] || continue
  REQ=${{P}}cfr_capture; [ -e $REQ ] || REQ=${{P}}link-0/cfr_capture
  grep -q "cfr_enabled = 1" $REQ 2>/dev/null || echo "1 2 {period} 0" > $REQ; done
"""

lock = threading.Lock()
ring = collections.deque(maxlen=4096)  # (t_s theo TSF, h 3×208, giờ PC lúc nhận — để cắt CSI theo nét vẽ)
state = dict(status="waiting for AoA + FTM" if A.single_ap else "đang kết nối", peer="", n=0, t_last=0.0)
model = dict()  # status mới nhất của daemon trên model-router + các lần phát hiện
events = collections.deque(maxlen=8)
history = collections.deque(maxlen=150)  # ~60 s cửa sổ (0.4 s/cửa sổ) cho nền thích nghi
listeners = []
shape_model = None
if A.shape_model:
    import pickle
    shape_model = pickle.loads(A.shape_model.read_bytes())  # file tự train (sensing/shape.py), không nạp file lạ
    assert shape_model["period_ms"] == A.period, f"model train ở {shape_model['period_ms']} ms, viz đang {A.period} ms"


def reader():
    while True:
        cmd = ["ssh", "-o", "BatchMode=yes", "-o", "ServerAliveInterval=5", f"root@{A.router}",
               REMOTE.format(hw=A.hw, sta=A.ap_if if A.csi_peer else A.sta_if, peer=A.csi_peer.lower() or "*", period=A.period)]
        p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)  # stdin mở = còn sống
        threading.Thread(target=lambda: [state.update(peer=l.decode().split()[1]) if l.startswith(b"VIZPEER")
                                         else state.update(status=l.decode().strip()) for l in p.stderr], daemon=True).start()
        buf, tsf0 = bytearray(), None
        state["status"] = "đang thu"
        while True:
            chunk = p.stdout.read1(65536)
            if not chunk:
                break
            buf += chunk
            recs, used = relay.parse(buf)
            del buf[:used]
            with lock:
                for tsf, h in recs:
                    tsf0 = tsf0 or tsf
                    t = (tsf - tsf0) / 1e6
                    # bản ghi không phải 80 MHz (chỉ 1 khối 20 MHz có tín hiệu) → bỏ, như QC bw80
                    blk = np.abs(h[0]).reshape(4, 52).mean(1)
                    if blk.min() < 0.2 * blk.max() or (ring and t <= ring[-1][0]):
                        continue
                    ring.append((t, h[:3], time.time()))
                    state["n"] += 1
                    state["t_last"] = time.time()
        p.wait()
        if not str(state["status"]).startswith("VIZERR"):
            state["status"] = f"ssh thoát ({p.returncode}), nối lại sau 3 s"
        time.sleep(3)


VIZ_WIN_S = 1.6   # cửa sổ mức chuyển động cho hiển thị (nhanh hơn 3.2 s của model; phổ phân giải 0.625 Hz)
TICK_S = 0.1      # đẩy biên độ CSI 10 lần/s
FEAT_EVERY = 4    # tính feature mỗi 0.4 s


def analyzer():
    tick, skipped, done = 0, 0, 0
    while True:
        time.sleep(TICK_S)
        tick += 1
        with lock:
            recs = list(ring)[-200:]
        if len(recs) < 2:
            publish(dict(status=state["status"], n=state["n"], phones=phone_messages(), model=dict(model, events=list(events)) if A.model_router else None))
            continue
        t = np.array([r[0] for r in recs])
        latest = np.abs(recs[-1][1])
        msg = dict(status=state["status"], peer=state["peer"], n=state["n"], phones=phone_messages(),
                   model=dict(model, events=list(events)) if A.model_router else None,
                   rate=round(float((t >= t[-1] - 1.0).sum()), 1),  # bản ghi trong 1 s gần nhất
                   stale=time.time() - state["t_last"] > 2,
                   amp=[[round(float(v), 3) for v in (latest[c] / (latest[c].mean() + 1e-9))[::4]] for c in range(len(latest))],
                   # mức thu từng chain (dB, TB 1 s): che từng anten để biết anten nào là chain nào (hiệu chuẩn AoA)
                   chain_db=[round(float(20 * np.log10(np.mean([np.abs(r[1][c]).mean() for r, tt in zip(recs, t) if tt >= t[-1] - 1.0]) + 1e-9)), 1)
                             for c in range(len(latest))])
        if tick % FEAT_EVERY == 0:
            sel = t >= t[-1] - VIZ_WIN_S
            ts = t[sel]
            # khoảng trống tính cả 2 đầu cửa sổ (record đầu/cuối cách mép cửa sổ) — như nội suy giữ giá trị biên
            gaps = np.diff(np.r_[t[-1] - VIZ_WIN_S, ts, t[-1]]) if len(ts) else np.array([VIZ_WIN_S])
            need = int(np.ceil(F.MIN_COVERAGE * VIZ_WIN_S / 0.0512))
            max_gap_ms = round(float(gaps.max()) * 1000)
            if len(ts) < need or max_gap_ms * 1000 > F.TIME_MAX_GAP_US:
                skipped += 1
                msg["skip"] = dict(reason=f"gap {max_gap_ms} ms > {F.TIME_MAX_GAP_US // 1000} ms" if max_gap_ms * 1000 > F.TIME_MAX_GAP_US
                                   else f"chỉ {len(ts)} bản ghi < {need}", max_gap_ms=max_gap_ms, n=int(len(ts)))
            else:
                done += 1
                f = F.invariant_features(np.stack([r[1] for r, s in zip(recs, sel) if s]), ts, t[-1] - VIZ_WIN_S, VIZ_WIN_S)
                history.append(f)
                base = np.percentile([h["inv_ratio_hf_01"] for h in history], 20)
                nb = float(np.log((f["inv_ratio_hf_01"] + 1e-9) / (base + 1e-9)))
                msg.update(features={k: round(float(f[k]), 4) for k in
                                     ("inv_std_c0", "inv_hf_c0", "inv_ffrac_c0", "inv_ratio_hf_01", "inv_ratio_hfrac_01")},
                           motion=round(max(0.0, min(1.0, nb / np.log(10))), 3),  # 0 = nền, 1 = gấp 10× nền (dải 1–4 Hz)
                           rhythm=round(float(f["inv_ratio_hfrac_01"]), 3), max_gap_ms=max_gap_ms)
            msg["windows"] = dict(ok=done, skipped=skipped)
            if shape_model:
                sel = t >= t[-1] - SH.WIN_S
                ts = t[sel]
                gap = np.diff(np.r_[t[-1] - SH.WIN_S, ts, t[-1]]).max()
                if len(ts) >= F.MIN_COVERAGE * SH.WIN_S / 0.0512 and gap * 1e6 <= F.TIME_MAX_GAP_US:
                    pr = SH.predict(shape_model, np.stack([r[1] for r, s in zip(recs, sel) if s]), ts, t[-1] - SH.WIN_S)
                    msg["shape"] = dict(p={k: round(v, 3) for k, v in pr.items()}, label=max(pr, key=pr.get))
                else:
                    msg["shape"] = dict(p={}, label="", skip=f"gap {gap * 1000:.0f} ms / {len(ts)} bản ghi")
        publish(msg)


def model_poller():
    """Đọc /tmp/wifi_sensing.status của daemon (ghi mỗi 2 s) qua 1 phiên ssh, 2 lần/s; detections tăng = 1 lần báo vẫy."""
    last_det = None
    while A.model_router:
        p = subprocess.Popen(["ssh", "-o", "BatchMode=yes", "-o", "ServerAliveInterval=5", f"root@{A.model_router}",
                              "exec 3<&0; ( cat <&3 > /dev/null; kill $$ ) & while :; do cat /tmp/wifi_sensing.status; "
                              "grep -o '\"feature_set\": \"[a-z_0-9]*\"' /root/wifi_sensing/model_report.json; "
                              "echo THR $(uci -q get wifi_sensing.main.on_threshold); usleep 500000; done"],
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        fs = ""
        for line in p.stdout:
            if line.startswith('"feature_set"'):
                fs = line.split('"')[3]
                continue
            if line.startswith("THR"):
                model["threshold"] = float(line.split()[1]) if len(line.split()) > 1 else None
                continue
            try:
                st = json.loads(line)
            except ValueError:
                continue
            if last_det is not None and st.get("detections", 0) > last_det:
                events.appendleft(dict(t=time.strftime("%H:%M:%S"), p=st.get("p_wave")))
            last_det = st.get("detections", 0)
            model.update(st, feature_set=fs, age=round(time.time() - st.get("time", 0), 1), seen=time.time())
        p.wait()
        model.update(state="mất kết nối tới router model")
        time.sleep(3)


# Khoảng cách điện thoại → 11.1 theo mô hình suy hao log-distance: d = 10^((P1m − RSSI) / (10·n)).
# RSSI = "last ack signal" (ACK của điện thoại thu tại 11.1); máy này bắn UDP keep-awake để điện thoại không ngủ.
# shortcut: 1 router chỉ cho khoảng cách (vòng tròn), không cho hướng; sai số trong nhà ~±4 dB → ×0.7–1.4 khoảng cách.
phones = {mac.lower(): dict(mac=mac.lower(), name=mac.lower(), ip="", p1m=-40.0, n=2.5,
                           samples=collections.deque(maxlen=15)) for mac in A.phone}
phone_lock = threading.Lock()


def update_phones(lines):
    """Một snapshot DHCP + station dump cho tất cả máy; cập nhật lại IP sau reboot."""
    leases, stations, section, cur = {}, {}, "", None
    for line in lines:
        line = line.strip()
        if line in ("LEASES", "STATIONS"):
            section, cur = line, None
        elif section == "LEASES":
            fields = line.split()
            if len(fields) >= 4 and fields[1].lower() in phones:
                leases[fields[1].lower()] = (fields[2], fields[3])
        elif section == "STATIONS":
            if line.startswith("Station "):
                cur = stations.setdefault(line.split()[1].lower(), {})
            elif cur is not None and "last ack signal:" in line:
                cur.setdefault("ack", int(line.split(":", 1)[1].split()[0]))
            elif cur is not None and "inactive time:" in line:
                cur.setdefault("inactive", int(line.split(":", 1)[1].split()[0]))
    valid = not any("command failed" in line for line in lines)
    now = time.time()
    with phone_lock:
        for mac, phone in phones.items():
            ip, name = leases.get(mac, ("", phone["name"]))
            phone.update(ip=ip, name=name if name != "*" else mac,
                         connected=(mac in stations) if valid else None, seen=now if valid else 0)
            station = stations.get(mac, {})
            ack, ina = station.get("ack"), station.get("inactive", 1e9)
            if not valid or mac not in stations:
                phone["samples"].clear()
                phone.pop("kf", None)
            elif ack is not None and ack < 0 and ina < 1000:
                phone["samples"].append(ack)
                kalman_update(phone, ack, now)
                phone["t"] = now


RSSI_SIGMA_DB = 3.0   # độ dao động 1 mẫu ack signal (đo 2026-10-21: 2.3 dB khi đứng yên)
ACCEL_SIGMA = 0.7     # m/s² — người đi bộ đổi tốc độ cỡ này


def kalman_update(phone, rssi, now):
    """Kalman vận tốc không đổi trên khoảng cách: x = [d, v]. Đo d từ 1 mẫu RSSI; nhiễu đo theo d vì sai số dB là
    nhân: σ_d = d·ln10·σ_dB/(10n). Bỏ mẫu lệch > 3σ (cổng ngoại lai) trừ khi 5 mẫu liền bị bỏ (đổi chỗ thật)."""
    z = 10 ** ((phone["p1m"] - rssi) / (10 * phone["n"]))
    kf = phone.get("kf")
    if kf is None:
        phone["kf"] = dict(x=np.array([z, 0.0]), P=np.diag([(z * 0.5) ** 2, 1.0]), t=now, rej=0)
        return
    dt = max(1e-3, now - kf["t"])
    Fm = np.array([[1, dt], [0, 1]])
    Q = ACCEL_SIGMA ** 2 * np.array([[dt ** 4 / 4, dt ** 3 / 2], [dt ** 3 / 2, dt ** 2]])
    x, P = Fm @ kf["x"], Fm @ kf["P"] @ Fm.T + Q
    r = (max(x[0], 0.1) * np.log(10) * RSSI_SIGMA_DB / (10 * phone["n"])) ** 2
    S = P[0, 0] + r
    if (z - x[0]) ** 2 > 9 * S and kf["rej"] < 5:
        kf.update(x=x, P=P, t=now, rej=kf["rej"] + 1)
        return
    K = P[:, 0] / S
    x = x + K * (z - x[0])
    P = P - np.outer(K, P[0, :])
    kf.update(x=np.array([max(x[0], 0.05), x[1]]), P=P, t=now, rej=0)


def phone_poller():
    while phones:
        p = subprocess.Popen(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5", "-o", "ServerAliveInterval=5",
                              "-o", "ServerAliveCountMax=2", f"root@{A.router}",
                              f"exec 3<&0; ( cat <&3 > /dev/null; kill $$ ) & while :; do "
                              f"echo LEASES; cat /tmp/dhcp.leases; echo STATIONS; iw dev {A.ap_if} station dump 2>&1; "
                              f"echo END; usleep 200000; done"],
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        lines = []
        for line in p.stdout:
            if line.strip() == "END":
                update_phones(lines)
                lines = []
            else:
                lines.append(line)
        p.wait()
        with phone_lock:
            for phone in phones.values():
                phone["samples"].clear()
                phone.update(connected=None, seen=0)
        time.sleep(3)


def keepawake():
    """Gói UDP nhỏ 10 lần/s/máy để router cập nhật RSSI của ACK, như chế độ một máy."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    while True:
        for phone in phone_messages():
            if phone["connected"] and re.fullmatch(r"\d+\.\d+\.\d+\.\d+", phone["ip"]):
                try:
                    sock.sendto(b"\0" * 64, (phone["ip"], 9))
                except OSError:
                    pass
        time.sleep(0.1)


def phone_msg(phone):
    s_ = list(phone["samples"])
    seen = time.time() - phone.get("seen", 0) < 5
    fresh = seen and s_ and time.time() - phone.get("t", 0) < 5
    out = dict(mac=phone["mac"], name=phone["name"], ip=phone["ip"], connected=phone.get("connected") if seen else None,
               p1m=phone["p1m"], n=phone["n"], fresh=bool(fresh))
    if fresh and phone.get("kf"):
        r = float(np.median(s_))
        kf = phone["kf"]
        d, v = float(kf["x"][0]), float(kf["x"][1])
        sd = float(np.sqrt(kf["P"][0, 0]))
        out.update(rssi=round(r, 1), dist=round(d, 2), lo=round(max(0.05, d - 2 * sd), 2), hi=round(d + 2 * sd, 2),
                   vel=round(v, 2), trend="approaching" if v < -0.15 else "leaving" if v > 0.15 else "still",
                   zone="near" if d < 1.5 else "mid" if d < 3.5 else "far")
    return out


def phone_messages():
    positions = {}
    if A.position_file:
        try:
            positions = json.loads(A.position_file.read_text())
            if not isinstance(positions, dict):
                positions = {}
        except (OSError, ValueError):
            pass
    with phone_lock:
        messages = [phone_msg(phone) for phone in phones.values()]
    for phone in messages:
        phone["position"] = fuse(positions.get(phone["mac"]), phone["mac"])
    return messages


ROOT = HERE.parent
CERTS = ROOT / "data" / "certs"
STROKES = ROOT / "data" / "strokes"
INTENTS = {"circle", "triangle", "none", ""}  # hình người dùng định vẽ (nhãn thật cho model CSI); "" = không ghi
CSI_PAD_S = 0.5
dlock = threading.Lock()
drawing = dict(active=False, samples=[], t0=0.0, last=None, n=0)


def horizontal_trace(samples, t0, t1):
    """Chiều NGANG của nét theo thời gian: gyro x và −pha CSI-ratio 0-1 (bám ngược chiều gyro x, docs/strokes_csi),
    mỗi chuỗi chuẩn hoá z (không cùng đơn vị) → [[giây từ lúc chạm, giá trị]] + tương quan r."""
    if len(samples) < 10:
        return None
    gt = np.array([x["t"] for x in samples]) / 1000
    gx = np.array([p[0] for p in DR.integrate(samples)])
    with lock:
        seg = [(r[2], r[1]) for r in ring if t0 <= r[2] <= t1]
    if len(seg) < 10:
        return dict(gyro=[], csi=[], r=None, n_csi=len(seg))
    pc = np.array([w for w, _ in seg]) - t0
    h = np.stack([c for _, c in seg])
    ph = -np.unwrap(np.angle((h[:, 0] * np.conj(h[:, 1])).sum(1)))
    ph = np.convolve(np.pad(ph, 2, "edge"), np.ones(5) / 5, "valid")
    gx_at = np.interp(pc, gt - gt[0], gx)
    z = lambda v: (v - v.mean()) / (v.std() + 1e-9)  # noqa: E731
    r = float(np.corrcoef(ph, gx_at)[0, 1]) if gx_at.std() > 0 and ph.std() > 0 else None
    step = max(1, len(gt) // 150)
    return dict(gyro=[[round(float(t), 2), round(float(v), 2)] for t, v in zip((gt - gt[0])[::step], z(gx)[::step])],
                csi=[[round(float(t), 2), round(float(v), 2)] for t, v in zip(pc, z(ph))], r=r and round(r, 2), n_csi=len(seg))


def draw_msg():
    with dlock:
        pts = DR.integrate(drawing["samples"]) if drawing["active"] else (drawing["last"] or {}).get("pts", [])
        last = drawing["last"]
        samples = list(drawing["samples"])
        t0, t1 = drawing["t0"], time.time() if drawing["active"] else (last or {}).get("t1", 0)
        active = drawing["active"]
    return dict(active=active, pts=[[round(x, 1), round(y, 1)] for x, y in pts[::max(1, len(pts) // 200)]],
                shape=last and last["shape"], score=last and last["score"], scores=last and last["scores"],
                age=round(time.time() - last["t1"], 1) if last else None, n=drawing["n"],
                horizontal=horizontal_trace(samples, t0, t1) if not A.no_csi else None)


def draw_post(req):
    """{start: bool, intent: str, end: bool, samples: [{t, o: [alpha, beta, gamma]}]} từ phone.html.
    Hết nét: nhận diện + lưu CSI 11.1 của nét (±0.5 s) vào data/strokes/csi/<id>.npz (cần viz đọc CSI: --csi-peer)."""
    samples = [dict(t=float(x["t"]), o=[float(v) for v in x["o"][:3]]) for x in req.get("samples", [])[:2000]]
    if not all(len(x["o"]) == 3 and all(math.isfinite(v) for v in x["o"]) for x in samples):
        raise ValueError("o phải là [alpha, beta, gamma]")
    with dlock:
        if req.get("start"):
            intent = str(req.get("intent", ""))
            if intent not in INTENTS:
                raise ValueError("intent")
            drawing.update(active=True, samples=[], t0=time.time(), intent=intent)
        if drawing["active"]:
            drawing["samples"] += samples
        if req.get("end") and drawing["active"]:
            pts = DR.stroke(drawing["samples"])
            shape, score, scores = DR.recognize(pts)
            rec = dict(t0=drawing["t0"], t1=time.time(), router=A.router, intent=drawing["intent"], shape=shape, score=score, scores=scores,
                       n_samples=len(drawing["samples"]), pts=[[round(x, 2), round(y, 2)] for x, y in pts])
            drawing.update(active=False, last=rec, n=drawing["n"] + 1)
            with lock:
                seg = [r for r in ring if rec["t0"] - CSI_PAD_S <= r[2] <= rec["t1"] + CSI_PAD_S]
            rec["n_csi"] = len(seg)
            rec["csi_rate"] = round(len(seg) / (rec["t1"] - rec["t0"] + 2 * CSI_PAD_S), 1)
            if seg:
                (STROKES / "csi").mkdir(parents=True, exist_ok=True)
                name = time.strftime("%Y%m%d-%H%M%S", time.localtime(rec["t0"])) + f"_{rec['intent']}_{drawing['n']}"
                rec["csi_file"] = f"csi/{name}.npz"
                np.savez_compressed(STROKES / rec["csi_file"], t_tsf_s=np.array([r[0] for r in seg]),
                                    pc_time=np.array([r[2] for r in seg]), csi=np.stack([r[1] for r in seg]).astype(np.complex64))
            STROKES.mkdir(parents=True, exist_ok=True)
            with (STROKES / time.strftime("%Y%m%d.jsonl")).open("a") as f:  # nhãn từng nét (giờ PC) để cắt CSI
                f.write(json.dumps(dict(rec, samples=drawing["samples"])) + "\n")
            return rec
        return dict(pts=[[round(x, 1), round(y, 1)] for x, y in DR.integrate(drawing["samples"])[::3]])  # nét đang vẽ cho điện thoại


class DrawH(BaseHTTPRequestHandler):
    """Chỉ phục vụ trang vẽ + nhận mẫu gyro; nghe trên mạng LAN 11.1, không có endpoint nào khác."""
    def log_message(self, *a):
        pass

    def _send(self, code, body, ctype):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path in ("/", "/phone"):
            return self._send(200, (HERE / "phone.html").read_bytes(), "text/html; charset=utf-8")
        if self.path == "/ping":  # điện thoại gọi đều → Wi-Fi không ngủ → 11.1 có CSI liên tục
            return self._send(200, b"{}", "application/json")
        if self.path == "/ca.crt":
            return self._send(200, (CERTS / "public" / "ca.crt").read_bytes(), "application/x-x509-ca-cert")
        self.send_error(404)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        if self.path != "/imu" or not 0 < n <= 1 << 20:
            return self.send_error(400)
        try:
            rec = draw_post(json.loads(self.rfile.read(n)))
        except (ValueError, KeyError, TypeError, IndexError):
            return self.send_error(400)
        body = json.dumps(dict(ok=True, pts=rec["pts"][::3], **{k: rec.get(k) for k in ("shape", "score", "n_csi", "csi_rate")})).encode()
        self._send(200, body, "application/json")


def publish(msg):
    msg["single_ap"] = A.single_ap
    msg["no_41"] = A.single_ap or A.no_csi or bool(A.csi_peer)  # CSI không đi qua link 11.1↔4.1 → trang ẩn 4.1
    if A.draw_port:
        msg["draw"] = draw_msg()
    data = ("data: " + json.dumps(msg, ensure_ascii=False) + "\n\n").encode()
    for q in list(listeners):
        q.append(data)


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_POST(self):
        url = urlsplit(self.path)
        mac = parse_qs(url.query).get("mac", [next(iter(phones), "")])[0].lower()
        with phone_lock:
            phone = phones.get(mac)
            calibrated = url.path == "/calibrate" and phone is not None and phone_msg(phone)["fresh"]
            if calibrated:
                phone["p1m"] = float(np.median(list(phone["samples"])))
                body = json.dumps(dict(mac=mac, p1m=phone["p1m"])).encode()
        if calibrated:
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(body)
            return
        self.send_error(409 if url.path == "/calibrate" else 404)

    def do_GET(self):
        if self.path == "/events":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            q = collections.deque(maxlen=50)
            listeners.append(q)
            try:
                while True:
                    while q:
                        self.wfile.write(q.popleft())
                        self.wfile.flush()
                    time.sleep(0.03)
            except (BrokenPipeError, ConnectionResetError):
                pass
            finally:
                listeners.remove(q)
            return
        if self.path in ("/", "/index.html"):
            body = (HERE / "index.html").read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        self.send_error(404)


if A.no_csi:
    state["status"] = "CSI off (--no-csi)"
elif not A.single_ap:
    threading.Thread(target=reader, daemon=True).start()
if A.draw_port:
    import ssl
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(CERTS / "server.crt", CERTS / "server.key")
    https = ThreadingHTTPServer((A.draw_host, A.draw_port), DrawH)
    https.socket = ctx.wrap_socket(https.socket, server_side=True)
    threading.Thread(target=https.serve_forever, daemon=True).start()
    # http chỉ để tải CA lần đầu (trước khi iPhone tin cert)
    threading.Thread(target=ThreadingHTTPServer((A.draw_host, A.draw_port + 1), DrawH).serve_forever, daemon=True).start()
    u = A.draw_url_host or A.draw_host
    print(f"Trang vẽ iPhone: https://{u}:{A.draw_port}/phone  (cài CA: http://{u}:{A.draw_port + 1}/ca.crt)")
threading.Thread(target=analyzer, daemon=True).start()
threading.Thread(target=model_poller, daemon=True).start()
threading.Thread(target=phone_poller, daemon=True).start()
if phones:
    threading.Thread(target=keepawake, daemon=True).start()
mode = "CSI off" if A.no_csi else "single AP; waiting for measured AoA + FTM" if A.single_ap else f"CSI live từ {A.router} {A.ap_if + ' ' + A.csi_peer if A.csi_peer else A.sta_if}, {A.period} ms"
print(f"3D viz: http://127.0.0.1:{A.port}/  ({mode})")
ThreadingHTTPServer(("127.0.0.1", A.port), H).serve_forever()
