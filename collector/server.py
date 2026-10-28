"""Web thu + gán nhãn CSI (Phase 1, ghi chú dự án).

    python3 collector/server.py [--port 8080] [--host 127.0.0.1]

Mỗi lượt thu: chạy keep-awake UDP tới station, gọi collector/capture_pull.sh (kit + BW) vào
data/raw/<recording_id>/, ghi label.json và capture.log. Raw không bao giờ bị xoá;
loại bỏ bằng cờ "exclude" trong label.json.
"""
import argparse, json, re, subprocess, sys, threading, time, unicodedata
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "data" / "raw"
TRASH = ROOT / "data" / "trash"  # "xóa" trên web = chuyển vào đây; xóa hẳn bằng tay
CAPTURE = Path(__file__).with_name("capture_pull.sh")  # kit pull + tham số BW
KEEPAWAKE = ROOT / "tools" / "keepawake_udp.py"
INDEX = Path(__file__).with_name("index.html")

LABELS = {"wave", "no_wave", "empty", "calib", "test", "circle", "triangle", "none"}  # calib: phòng YÊN 30–60 s đầu mỗi phiên → nền cho inv_nb
# interface AP 5 GHz và node CFR (debugfs ath12k_hwN) theo router; router khác dùng mặc định RDP499
BOARDS = {"192.168.11.1": dict(ap_if="phy00.0-0", hw="ath12k_hw0"),   # RDP499, 3 chain, driver 0006
          "192.168.4.1": dict(ap_if="phy00.2-0", hw="ath12k_hw2")}    # RDP488, 5 chain, patch 0001-0005
DEFAULT_BOARD = BOARDS["192.168.11.1"]
IP = re.compile(r"^\d{1,3}(\.\d{1,3}){3}$")
MAC = re.compile(r"^[0-9a-f]{2}(:[0-9a-f]{2}){5}$")
TEXT = re.compile(r"^[\w .,:/()-]{0,80}$")  # người/vị trí/ghi chú: không ký tự shell, không xuống dòng

state = {"busy": False, "id": None, "started": None, "secs": 0, "last": None}
lock = threading.Lock()


def validate(req):
    """Kiểm tra form ở ranh giới tin cậy: các giá trị này đi vào lệnh ssh trên router."""
    v = {k: str(req.get(k, "")).strip() for k in
         ["board", "sta", "sta_ip", "label", "person", "position", "distance", "session", "room", "note"]}
    v["sta"] = v["sta"].lower()
    if not IP.match(v["board"]) or (v["sta_ip"] and not IP.match(v["sta_ip"])):  # sta_ip rỗng: station không ngủ (vd. router 11.1 làm STA)
        raise ValueError("IP không hợp lệ")
    if not MAC.match(v["sta"]):
        raise ValueError("MAC không hợp lệ (dạng aa:bb:cc:dd:ee:ff)")
    if v["label"] not in LABELS:
        raise ValueError(f"nhãn phải thuộc {sorted(LABELS)}")
    for k in ["person", "position", "distance", "session", "room", "note"]:
        if not TEXT.match(v[k]):
            raise ValueError(f"{k}: chỉ chữ, số, khoảng trắng và .,:/()-")
    v["period_ms"] = int(req.get("period_ms", 50))
    v["secs"] = int(req.get("secs", 60))
    v["bw"] = int(req.get("bw", 2))
    if v["bw"] not in (0, 2):
        raise ValueError("BW phải là 0 (20 MHz) hoặc 2 (80 MHz)")
    v.update(BOARDS.get(v["board"], DEFAULT_BOARD))
    if not 20 <= v["period_ms"] <= 1000 or not 5 <= v["secs"] <= 600:
        raise ValueError("chu kỳ 20-1000 ms, thời lượng 5-600 s")
    return v


def quality(log):
    """Dòng PASS/WARN/FAIL/info của cfr_quality_report.py, và tốc độ record/s."""
    lines = [l.strip() for l in log.splitlines() if re.match(r"^(PASS|WARN|FAIL|info)\s", l)]
    m = re.search(r"rate ([\d.]+)/s", log)
    return lines, float(m.group(1)) if m else None


def slug(text):
    text = unicodedata.normalize("NFKD", str(text or "").replace("đ", "d").replace("Đ", "D"))
    text = "".join(c for c in text if not unicodedata.combining(c))
    return re.sub(r"[^A-Za-z0-9-]+", "-", text).strip("-")


def make_id(stamp, board, label, session="", position="", person=""):
    """<YYYYmmdd-HHMMSS>_<board 2 octet cuối>_<nhãn>[_phiên][_vị trí][_người]: sắp xếp theo thời gian,
    đọc được nhãn ngay ở tên thư mục, ví dụ 20261020-101949_11-1_no_wave_S1_P1_tuan."""
    parts = [stamp, slug(".".join(board.split(".")[-2:])), label, slug(session), slug(position), slug(person)]
    return "_".join(p for p in parts if p)


def rec_file(rec_id):
    f = RAW / rec_id / "label.json"
    return f if re.fullmatch(r"[\w-]+", rec_id) and f.is_file() else None


def capture(v):
    rec_id = make_id(time.strftime("%Y%m%d-%H%M%S"), v["board"], v["label"], v["session"], v["position"], v["person"])
    out = RAW / rec_id
    out.mkdir(parents=True)
    with lock:
        state.update(id=rec_id, started=time.time(), secs=v["secs"])
    # daemon wifi_sensing (router/) giữ enable_cfr=1 khi có station → kit từ chối thu; dừng nó, thu xong bật lại
    paused = board_ssh(v["board"], "pidof wifi_sensing >/dev/null && /etc/init.d/wifi_sensing stop && sleep 1 "
                                   "&& echo paused").strip() == "paused"
    keep = v["sta_ip"] and subprocess.Popen([sys.executable, str(KEEPAWAKE), v["sta_ip"], str(v["secs"] + 30)])
    try:
        time.sleep(2)  # để station kịp thức trước request đầu tiên
        p = subprocess.run(["sh", str(CAPTURE), v["board"], v["sta"], str(v["period_ms"]),
                            str(v["secs"]), str(out), str(v["bw"]), v["ap_if"], v["hw"]],
                           capture_output=True, text=True, timeout=v["secs"] + 300)
        log = p.stdout + p.stderr
        code = p.returncode
    except subprocess.TimeoutExpired as e:
        log, code = f"timeout: {e}", -1
    finally:
        if keep:
            keep.kill()
        if paused:
            board_ssh(v["board"], "/etc/init.d/wifi_sensing start")
    if paused:
        log = "(collector: đã tạm dừng daemon wifi_sensing trong lúc thu, đã bật lại)\n" + log
    (out / "capture.log").write_text(log)
    lines, rate = quality(log)
    label = dict(v, recording_id=rec_id, exclude=v["label"] == "test" or code != 0,
                 exit_code=code, rate_per_s=rate, quality=lines,
                 captured_at=time.strftime("%Y-%m-%dT%H:%M:%S%z"))
    (out / "label.json").write_text(json.dumps(label, ensure_ascii=False, indent=1))
    with lock:
        state["last"] = label


def run_capture(v):
    try:
        capture(v)
    except Exception as e:  # báo lỗi lên web thay vì kẹt busy mãi
        with lock:
            state["last"] = {"recording_id": state["id"], "error": repr(e)}
    finally:
        with lock:
            state["busy"] = False


def board_ssh(board, cmd, timeout=30):
    """Chạy lệnh trên router (board đã qua kiểm tra IP), trả stdout; lỗi ssh → chuỗi rỗng."""
    try:
        return subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=6", f"root@{board}", cmd],
                              capture_output=True, text=True, timeout=timeout).stdout
    except subprocess.TimeoutExpired:
        return ""


def stations(board):
    """Station trên AP 5 GHz của router + IP. Quét ARP trước vì phone ngủ không có trong bảng neighbour."""
    ap_if = BOARDS.get(board, DEFAULT_BOARD)["ap_if"]
    net = board.rsplit(".", 1)[0]
    cmd = (f"for i in $(seq 2 254); do ping -c1 -W1 {net}.$i >/dev/null 2>&1 & done; wait; "
           f"iw dev {ap_if} station dump | grep -E '^Station|inactive time|signal:'; echo ==; ip -4 neigh show")
    p = subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=6", f"root@{board}", cmd],
                       capture_output=True, text=True, timeout=40)
    dump, _, neigh = p.stdout.partition("==")
    ips = {m.group(2): m.group(1) for m in re.finditer(r"^(\S+) .*lladdr (\S+)", neigh, re.M)}
    out = []
    for block in dump.split("Station ")[1:]:
        mac = block.split()[0]
        inactive = re.search(r"inactive time:\s*(\d+)", block)
        signal = re.search(r"signal:\s*(-?\d+)", block)
        out.append(dict(mac=mac, ip=ips.get(mac, ""), inactive_ms=int(inactive.group(1)) if inactive else None,
                        signal_dbm=int(signal.group(1)) if signal else None))
    return dict(board=board, ap_if=ap_if, stations=out, error=p.stderr.strip()[-200:] if p.returncode else "")


def recordings():
    rows = []
    for f in sorted(RAW.glob("*/label.json"), reverse=True):
        try:
            rows.append(json.loads(f.read_text()))
        except ValueError:
            rows.append({"recording_id": f.parent.name, "note": "label.json hỏng"})
        rows[-1].setdefault("recording_id", f.parent.name)
    return rows


class Handler(BaseHTTPRequestHandler):
    def send(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body, ensure_ascii=False).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype + "; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/":
            return self.send(200, INDEX.read_bytes(), "text/html")
        if self.path == "/api/status":
            with lock:
                return self.send(200, dict(state))
        if self.path == "/api/recordings":
            return self.send(200, recordings())
        if self.path.startswith("/api/stations?board="):
            board = self.path.split("=", 1)[1]
            if not IP.match(board):
                return self.send(400, {"error": "IP không hợp lệ"})
            try:
                return self.send(200, stations(board))
            except subprocess.TimeoutExpired:
                return self.send(504, {"error": "router không trả lời"})
        self.send(404, {"error": "not found"})

    def do_POST(self):
        try:
            req = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
        except ValueError:
            return self.send(400, {"error": "JSON không hợp lệ"})
        if self.path == "/api/capture":
            try:
                v = validate(req)
            except ValueError as e:
                return self.send(400, {"error": str(e)})
            with lock:
                if state["busy"]:
                    return self.send(409, {"error": "đang có lượt thu khác"})
                state["busy"] = True
            threading.Thread(target=run_capture, args=(v,), daemon=True).start()
            return self.send(202, {"ok": True})
        if self.path in ("/api/label", "/api/delete"):
            rec_id = str(req.get("recording_id", ""))
            f = rec_file(rec_id)
            if not f:
                return self.send(404, {"error": "không có recording"})
            with lock:
                if state["busy"] and state["id"] == rec_id:
                    return self.send(409, {"error": "recording đang được thu"})
            if self.path == "/api/delete":
                TRASH.mkdir(parents=True, exist_ok=True)
                f.parent.rename(TRASH / f"{rec_id}__deleted-{time.strftime('%Y%m%d-%H%M%S')}")
                return self.send(200, {"ok": True})
            label = json.loads(f.read_text())
            if "exclude" in req:
                label["exclude"] = bool(req["exclude"])
            if "label" in req and req["label"] != label.get("label"):
                if req["label"] not in LABELS:
                    return self.send(400, {"error": "nhãn không hợp lệ"})
                label.setdefault("label_history", []).append(label.get("label"))
                label["label"] = req["label"]
                # tên thư mục chứa nhãn → dựng lại tên từ metadata với nhãn mới
                new_id = make_id(rec_id[:15], label.get("board", ""), req["label"], label.get("session"),
                                 label.get("position"), label.get("person"))
                if new_id != rec_id and not (RAW / new_id).exists():
                    f.parent.rename(RAW / new_id)
                    f = RAW / new_id / "label.json"
                    label["recording_id"] = new_id
            f.write_text(json.dumps(label, ensure_ascii=False, indent=1))
            return self.send(200, label)
        self.send(404, {"error": "not found"})

    def log_message(self, *a):
        pass


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--host", default="127.0.0.1")
    a = ap.parse_args()
    RAW.mkdir(parents=True, exist_ok=True)
    print(f"collector: http://{a.host}:{a.port}/  data -> {RAW}")
    ThreadingHTTPServer((a.host, a.port), Handler).serve_forever()
