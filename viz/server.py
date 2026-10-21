"""3D visualization CSI live từ router 11.1 (mức 1: hiển thị TÍN HIỆU, không suy ra vị trí/tư thế người).

    .venv/bin/python viz/server.py [--router 192.168.11.1] [--port 8090] [--period 20]   → http://127.0.0.1:8090/

11.1 tự request CFR tới peer 4.1 trên iface STA `phy00.0-1` (link 11.1 → 4.1, không cần điện thoại), đọc relay qua
ssh (stdout), parse bằng viz/relay.py; đẩy biên độ CSI 10 lần/s, mỗi 0.4 s tính features.invariant_features trên cửa sổ
1.6 s (chỉ hiển thị; model dùng 3.2 s), báo rõ cửa sổ bị bỏ vì gap > 300 ms hoặc thiếu bản ghi; JSON
qua Server-Sent Events `/events`. Remote tự trả request + enable_cfr về 0 khi ssh đứt (trap EXIT; vòng đọc tự thoát khi stdin của phiên ssh đóng).
Nền (cho mức chuyển động) = phân vị 20 % của 60 s gần nhất — shortcut: thích nghi, sai khi chuyển động kéo dài > 60 s;
dùng nền hiệu chỉnh cố định nếu cần.
"""
import argparse, collections, json, re, socket, subprocess, sys, threading, time
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

ap = argparse.ArgumentParser()
ap.add_argument("--router", default="192.168.11.1")
ap.add_argument("--sta-if", default="phy00.0-1", help="iface STA của 11.1 (link tới 4.1)")
ap.add_argument("--hw", default="ath12k_hw0")
ap.add_argument("--period", type=int, default=20, help="chu kỳ request CFR (ms, 20–1000)")
ap.add_argument("--port", type=int, default=8090)
ap.add_argument("--single-ap", action="store_true", help="chỉ dùng 11.1; tắt luồng CFR link STA dễ treo khi peer mất")
ap.add_argument("--position-file", type=Path, help="JSON {MAC: {aoa: ..., ftm: ...}} từ các bộ đo đã xác minh")
ap.add_argument("--phone", action="append", default=[], help="MAC điện thoại trên AP 11.1; lặp lại để hiện nhiều máy")
ap.add_argument("--ap-if", default="phy00.0-0", help="iface AP của 11.1 (điện thoại kết nối vào đây)")
ap.add_argument("--model-router", default="192.168.4.1",
                help="router chạy daemon wifi_sensing (model vẫy tay); '' = không hiện model")
A = ap.parse_args()
if A.single_ap:
    A.model_router = ""
assert A.model_router == "" or re.fullmatch(r"\d+\.\d+\.\d+\.\d+", A.model_router)
assert all(re.fullmatch(r"([0-9a-f]{2}:){5}[0-9a-f]{2}", mac.lower()) for mac in A.phone)
assert re.fullmatch(r"\d+\.\d+\.\d+\.\d+", A.router) and re.fullmatch(r"[\w.-]+", A.sta_if + A.ap_if + A.hw) and 20 <= A.period <= 1000

REMOTE = r"""
D=/sys/kernel/debug/ieee80211/phy00; EN=$D/{hw}/enable_cfr; R=$D/{hw}/cfr_capture0
P=$(ls -d $D/netdev:{sta}/stations/*/ 2>/dev/null | head -1)
[ -n "$P" ] || {{ echo "VIZERR không có peer trên {sta} (11.1 chưa nối 4.1?)" >&2; exit 2; }}
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
  P=$(ls -d $D/netdev:{sta}/stations/*/ 2>/dev/null | head -1); [ -n "$P" ] || continue
  REQ=${{P}}cfr_capture; [ -e $REQ ] || REQ=${{P}}link-0/cfr_capture
  grep -q "cfr_enabled = 1" $REQ 2>/dev/null || echo "1 2 {period} 0" > $REQ; done
"""

lock = threading.Lock()
ring = collections.deque(maxlen=4096)  # (t_s, h 3×208)
state = dict(status="waiting for AoA + FTM" if A.single_ap else "đang kết nối", peer="", n=0, t_last=0.0)
model = dict()  # status mới nhất của daemon trên model-router + các lần phát hiện
events = collections.deque(maxlen=8)
history = collections.deque(maxlen=150)  # ~60 s cửa sổ (0.4 s/cửa sổ) cho nền thích nghi
listeners = []


def reader():
    while True:
        cmd = ["ssh", "-o", "BatchMode=yes", "-o", "ServerAliveInterval=5", f"root@{A.router}",
               REMOTE.format(hw=A.hw, sta=A.sta_if, period=A.period)]
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
                    ring.append((t, h[:3]))
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
                   amp=[[round(float(v), 3) for v in (latest[c] / (latest[c].mean() + 1e-9))[::4]] for c in range(3)])
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
            elif ack is not None and ack < 0 and ina < 1000:
                phone["samples"].append(ack)
                phone["t"] = now


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
    if fresh:
        r = float(np.median(s_))
        d = 10 ** ((phone["p1m"] - r) / (10 * phone["n"]))
        out.update(rssi=round(r, 1), dist=round(d, 2), lo=round(d * 10 ** (-4 / (10 * phone["n"])), 2),
                   hi=round(d * 10 ** (4 / (10 * phone["n"])), 2))
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


def publish(msg):
    msg["single_ap"] = A.single_ap
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


if not A.single_ap:
    threading.Thread(target=reader, daemon=True).start()
threading.Thread(target=analyzer, daemon=True).start()
threading.Thread(target=model_poller, daemon=True).start()
threading.Thread(target=phone_poller, daemon=True).start()
if phones:
    threading.Thread(target=keepawake, daemon=True).start()
print(f"3D viz: http://127.0.0.1:{A.port}/  (CSI live từ {A.router} {A.sta_if}, {A.period} ms)")
ThreadingHTTPServer(("127.0.0.1", A.port), H).serve_forever()
