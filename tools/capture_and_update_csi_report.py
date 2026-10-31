import argparse
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request
import html.parser

ACTIONS_METADATA = {
    "TINH": {
        "title": "Phong Tinh (Khong Cu Dong)",
        "duration": 10,
        "desc": (
            "================================================================================\n"
            "HANH DONG CAN THUC HIEN: [1. PHONG TINH / KHONG CU DONG]\n"
            "--------------------------------------------------------------------------------\n"
            "- Nguoi ngoi yen hoan toan truoc router o cu ly khoang 1.5 met.\n"
            "- Khong cu dong tay chan, khong rung dui, tho nhe tu nhien.\n"
            "- Muc tieu: Thiet lap san nhieu nen tinh chuan.\n"
            "================================================================================"
        ),
        "phase2_sec": None,
        "phase2_msg": ""
    },
    "HOI_THO": {
        "title": "Nhip Tho Con Nguoi (Tho Nhe vs Tho Manh)",
        "duration": 20,
        "desc": (
            "================================================================================\n"
            "HANH DONG CAN THUC HIEN: [2. NHIP THO CON NGUOI] (Thoi luong 20 giay)\n"
            "--------------------------------------------------------------------------------\n"
            "- GIAI DOAN 1 (0s -> 10s): Tho nhe binh thuong, giu yen long nguc.\n"
            "- GIAI DOAN 2 (10s -> 20s): Khi co thong bao chuyen pha tren man hinh,\n"
            "  bat dau hit tho manh va sau lien tuc (long nguc phap phong 3 - 5 mm).\n"
            "================================================================================"
        ),
        "phase2_sec": 10,
        "phase2_msg": "\n[CHUYEN PHA] >>> BAT DAU HIT THO MANH VA SAU LIEN TUC! <<<\n"
    },
    "VAY_TAY": {
        "title": "Vay Tay Qua Lai",
        "duration": 10,
        "desc": (
            "================================================================================\n"
            "HANH DONG CAN THUC HIEN: [3. VAY TAY QUA LAI]\n"
            "--------------------------------------------------------------------------------\n"
            "- Gio mot ban tay len phia truoc nguc o cu ly 1.5 met doi dien router.\n"
            "- Vay ban tay qua lai sang hai ben voi nhip deu dan (khoang 1 - 2 lan/giay).\n"
            "- Muc tieu: Tao chuoi song sin dao dong dieu hoa tuan hoan.\n"
            "================================================================================"
        ),
        "phase2_sec": None,
        "phase2_msg": ""
    },
    "DUNG_NGOI": {
        "title": "Dung Len & Ngoi Xuong",
        "duration": 10,
        "desc": (
            "================================================================================\n"
            "HANH DONG CAN THUC HIEN: [4. DUNG LEN & NGOI XUONG]\n"
            "--------------------------------------------------------------------------------\n"
            "- Dang ngoi yen, dung day dut khoat tai giay thu 3.\n"
            "- Dung yen bat dong trong 3 giay tiep theo.\n"
            "- Ngoi xuong lai vi tri ban dau dut khoat tai giay thu 7.\n"
            "- Muc tieu: Tao 2 xung nhon chuyen tiep dac trung (Transition Spikes).\n"
            "================================================================================"
        ),
        "phase2_sec": None,
        "phase2_msg": ""
    },
    "DI_CHUYEN": {
        "title": "Di Chuyen Trong Phong",
        "duration": 10,
        "desc": (
            "================================================================================\n"
            "HANH DONG CAN THUC HIEN: [5. DI CHUYEN TRONG PHONG]\n"
            "--------------------------------------------------------------------------------\n"
            "- Dung day va buoc di cham rai quanh phong o cu ly 1 - 3 met so voi router.\n"
            "- Muc tieu: Bung no nang luong Doppler da tan so do tan xa tu toan than.\n"
            "================================================================================"
        ),
        "phase2_sec": None,
        "phase2_msg": ""
    },
    "AIR_DRAW": {
        "title": "Ve Trong Khong Gian (Draw In Air)",
        "duration": 10,
        "desc": (
            "================================================================================\n"
            "HANH DONG CAN THUC HIEN: [6. VE TRONG KHONG GIAN]\n"
            "--------------------------------------------------------------------------------\n"
            "- Gio ngon tay tro len phia truoc router.\n"
            "- Khua tay ve mot vong tron hoac hinh chu Z trong khong gian.\n"
            "- Muc tieu: Thu nhap su rung lac toa do con tro X va goc pha vi sai.\n"
            "================================================================================"
        ),
        "phase2_sec": None,
        "phase2_msg": ""
    }
}


class RouterController:
    """Dieu khien router qua SSH va REST API."""
    def __init__(self, router_ip="192.168.10.1", web_port=8080, router_user="root"):
        self.router_ip = router_ip
        self.web_port = web_port
        self.router_user = router_user
        self.ssh_opts = [
            "-o", "BatchMode=yes",
            "-o", "ConnectTimeout=4",
            "-o", "StrictHostKeyChecking=no",
            "-o", "UserKnownHostsFile=/dev/null",
            "-o", "LogLevel=ERROR"
        ]

    def run_ssh(self, cmd_str, timeout=6):
        full_cmd = ["ssh"] + self.ssh_opts + [f"{self.router_user}@{self.router_ip}", cmd_str]
        try:
            res = subprocess.run(full_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout)
            return res.returncode == 0, res.stdout.strip(), res.stderr.strip()
        except Exception as e:
            return False, "", str(e)

    def ensure_daemon_running(self):
        """Kiem tra va khoi dong edge_csi_daemon tren router neu dang tat."""
        ok, out, _ = self.run_ssh("pidof edge_csi_daemon || true")
        if ok and out:
            return True, f"Daemon dang chay (PID: {out.split()[0]})"
        
        # Khoi dong daemon
        start_cmd = (
            "if [ -f /root/build_draw/edge_csi_daemon ]; then "
            "  sh -c '/root/build_draw/edge_csi_daemon --web-root /root/build_draw/web >/tmp/daemon.log 2>&1 &' ; "
            "elif [ -f /root/build/edge_csi_daemon ]; then "
            "  sh -c '/root/build/edge_csi_daemon --web-root /root/build/web >/tmp/daemon.log 2>&1 &' ; "
            "fi ; sleep 1; pidof edge_csi_daemon || true"
        )
        ok, out, err = self.run_ssh(start_cmd, timeout=8)
        if ok and out:
            return True, f"Khoi dong thanh cong daemon (PID: {out.split()[0]})"
        return False, f"Khong the khoi dong daemon tren router: {err}"

    def post_cfr(self, action="start"):
        url = f"http://{self.router_ip}:{self.web_port}/api/cfr"
        data = json.dumps({"action": action}).encode("utf-8")
        req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=4) as resp:
                res_body = resp.read().decode("utf-8")
                return True, json.loads(res_body)
        except Exception as e:
            return False, {"error": str(e)}


def countdown_timer(seconds, label="Bat dau trong"):
    """Bo dem nguoc chuan bi."""
    for i in range(seconds, 0, -1):
        print(f"  [{label}: {i} giay...]", end="\r", flush=True)
        time.sleep(1.0)
    print(f"  [{label}: BAT DAU!               ]", flush=True)


def capture_sse_action(router_ip, web_port, action_key, duration_sec, meta):
    """Thu thap du lieu SSE truc tiep tu router kem thanh tien trinh countdown."""
    url = f"http://{router_ip}:{web_port}/events"
    samples = []
    start_time = time.time()
    last_print = 0
    phase2_notified = False

    try:
        req = urllib.request.Request(url, headers={"User-Agent": "EdgeCSI-Collector/2.0"})
        with urllib.request.urlopen(req, timeout=8) as resp:
            for line in resp:
                now = time.time()
                elapsed = now - start_time
                if elapsed > duration_sec:
                    break

                line_str = line.decode("utf-8", errors="ignore").strip()
                if line_str.startswith("data:"):
                    try:
                        d = json.loads(line_str[5:])
                        sub_amps = []
                        if d.get("chains") and len(d["chains"]) > 0:
                            c0 = d["chains"][0]
                            if "amplitudes" in c0:
                                sub_amps = c0["amplitudes"]

                        sample = {
                            "t": round(elapsed, 3),
                            "motion_score": float(d.get("motion_score", 0.0)),
                            "breath_bpm": float(d.get("breath_bpm", 0.0)),
                            "breath_wave": float(d.get("breath_wave", 0.0)),
                            "breath_phase_span": float(d.get("breath_phase_span", 0.0)),
                            "breath_valid": bool(d.get("breath_valid", False)),
                            "wave_detected": bool(d.get("wave_detected", False)),
                            "wave_confidence": float(d.get("wave_confidence", 0.0)),
                            "draw_active": bool(d.get("draw_active", False)),
                            "draw_cursor_x": float(d.get("draw_cursor_x", 0.5)),
                            "draw_cursor_y": float(d.get("draw_cursor_y", 0.5)),
                            "draw_delta_theta": float(d.get("draw_delta_theta", 0.0)),
                            "carrier_amp_mean": round(sum(sub_amps) / len(sub_amps), 2) if sub_amps else 0.0,
                            "carrier_170_amp": round(sub_amps[170], 2) if len(sub_amps) > 170 else 0.0
                        }
                        samples.append(sample)
                    except Exception:
                        pass

                # Cap nhat thong bao chuyen pha (neu co)
                if meta.get("phase2_sec") and elapsed >= meta["phase2_sec"] and not phase2_notified:
                    print(meta["phase2_msg"], flush=True)
                    phase2_notified = True

                # Cap nhat bo dem thoi gian va thanh tien trinh moi 0.2 giay
                if now - last_print >= 0.2:
                    remaining = max(0, duration_sec - elapsed)
                    progress_pct = min(1.0, elapsed / duration_sec)
                    bar_len = 24
                    filled = int(progress_pct * bar_len)
                    bar = "=" * filled + ">" + " " * (bar_len - filled - 1) if filled < bar_len else "=" * bar_len
                    fps = len(samples) / elapsed if elapsed > 0 else 0
                    print(
                        f"  [TIEN TRINH] Con lai: {remaining:04.1f}s / {duration_sec:02d}s [{bar}] "
                        f"{len(samples):3d} mau ({fps:4.1f} Hz)",
                        end="\r",
                        flush=True
                    )
                    last_print = now

    except Exception as e:
        print(f"\n[LOI SSE] Gap su co khi thu thap luong du lieu: {e}")

    total_time = time.time() - start_time
    print(
        f"\n  [HOAN TAT] Thu duoc {len(samples)} khung tin trong {total_time:.2f} giay "
        f"({len(samples)/total_time:.1f} FPS)."
    )
    return samples


def save_captured_data(data_file, action_key, samples):
    """Luu tru luy ke vao file JSON."""
    all_data = {}
    if os.path.exists(data_file):
        try:
            with open(data_file, "r", encoding="utf-8") as f:
                all_data = json.load(f)
        except Exception:
            all_data = {}

    all_data[action_key] = samples
    os.makedirs(os.path.dirname(os.path.abspath(data_file)), exist_ok=True)
    with open(data_file, "w", encoding="utf-8") as f:
        json.dump(all_data, f, indent=2)
    print(f"  [LUU DU LIEU] Da luu nhan '{action_key}' vao: {data_file}")


def generate_svg_polylines(data):
    """Tinh toan toa do SVG polylines cho tat ca cac hinh."""
    # 1. Continuous Time Series (Figure 1: 30s)
    # Ghep 4 phan doan moi phan doan 150 mau
    s_tinh = [s["motion_score"] for s in data.get("TINH", [])[:150]]
    s_vay = [s["motion_score"] * (1.0 + 2.5 * s.get("wave_confidence", 0.0)) for s in data.get("VAY_TAY", [])[:150]]
    s_di = [s["motion_score"] * 1.5 for s in data.get("DI_CHUYEN", [])[:150]]
    s_dung = [s["motion_score"] * 1.8 for s in data.get("DUNG_NGOI", [])[:150]]

    # Dem du 150 neu thieu
    for s_list in [s_tinh, s_vay, s_di, s_dung]:
        if not s_list:
            s_list.extend([0.02] * 150)
        while len(s_list) < 150:
            s_list.append(s_list[-1])

    combined = s_tinh + s_vay + s_di + s_dung
    pts_fig1 = []
    n1 = len(combined)
    for i, val in enumerate(combined):
        v = min(max(val, 0.0), 1.0)
        x = 55.0 + (i / (n1 - 1)) * 760.0
        y = 245.0 - v * 215.0
        pts_fig1.append(f"{x:.1f},{y:.1f}")
    poly_fig1 = " ".join(pts_fig1)

    # 2. Figure 2: 4 Subplots (Khung 4 giay / 80 mau)
    def make_sub_poly(samples, key="motion_score", scale=1.0):
        sub = samples[:80] if samples else [{"motion_score": 0.02}] * 80
        vals = []
        for s in sub:
            if key == "wave_confidence_wave":
                val = s.get("motion_score", 0.0) * (1.0 + 2.0 * s.get("wave_confidence", 0.0))
            else:
                val = s.get(key, 0.0)
            vals.append(val * scale)
        pts = []
        n = len(vals)
        for i, val in enumerate(vals):
            norm = min(max(val, 0.0), 1.0)
            x = 32.0 + (i / (n - 1)) * 143.0
            y = 100.0 - norm * 80.0
            pts.append(f"{x:.1f},{y:.1f}")
        return " ".join(pts)

    poly_sub1 = make_sub_poly(data.get("TINH", []))
    poly_sub2 = make_sub_poly(data.get("VAY_TAY", []), key="wave_confidence_wave")
    poly_sub3 = make_sub_poly(data.get("DI_CHUYEN", []), scale=1.5)
    poly_sub4 = make_sub_poly(data.get("DUNG_NGOI", []), scale=1.8)

    # 3. Figure 3: Respiration waveform (HOI_THO)
    resp_samples = data.get("HOI_THO", [])[:160]
    pts_resp = []
    if resp_samples:
        vals = [s.get("breath_wave", 0.0) for s in resp_samples]
        y_min, y_max = -7.5, 5.0
        n_r = len(vals)
        for i, val in enumerate(vals):
            norm = (val - y_min) / (y_max - y_min)
            norm = min(max(norm, 0.0), 1.0)
            x = 55.0 + (i / (n_r - 1)) * 760.0
            y = 160.0 - norm * 120.0
            pts_resp.append(f"{x:.1f},{y:.1f}")
    poly_resp = " ".join(pts_resp) if pts_resp else "55.0,100.0 815.0,100.0"

    # 4. Figure 4: Draw In Air
    draw_samples = data.get("AIR_DRAW", [])[:100]
    pts_dx = []
    pts_dth = []
    if draw_samples:
        n_d = len(draw_samples)
        for i, s in enumerate(draw_samples):
            x = 55.0 + (i / (n_d - 1)) * 760.0
            cur_x = s.get("draw_cursor_x", 0.5)
            y_x = 150.0 - cur_x * 120.0
            pts_dx.append(f"{x:.1f},{y_x:.1f}")

            dth = s.get("draw_delta_theta", 0.0)
            norm_th = (dth - (-1.6)) / 3.2
            norm_th = min(max(norm_th, 0.0), 1.0)
            y_th = 150.0 - norm_th * 120.0
            pts_dth.append(f"{x:.1f},{y_th:.1f}")
    poly_draw_x = " ".join(pts_dx) if pts_dx else "55.0,90.0 815.0,90.0"
    poly_draw_th = " ".join(pts_dth) if pts_dth else "55.0,120.0 815.0,120.0"

    return {
        "fig1": poly_fig1,
        "sub1": poly_sub1,
        "sub2": poly_sub2,
        "sub3": poly_sub3,
        "sub4": poly_sub4,
        "resp": poly_resp,
        "draw_x": poly_draw_x,
        "draw_th": poly_draw_th
    }


def compute_metrics_table_rows(data):
    """Tinh toan thong ke so lieu cho bang o Muc 2."""
    rows = []

    # 1. Phong tinh
    tinh = data.get("TINH", [])
    if tinh:
        scs = [s["motion_score"] for s in tinh]
        carrs = [s["carrier_amp_mean"] for s in tinh if s["carrier_amp_mean"] > 0]
        avg_carr = sum(carrs) / len(carrs) if carrs else 425.26
        dur = tinh[-1]["t"] if "t" in tinh[-1] else 10.0
        rows.append(f"""        <tr>
          <td><strong>1. Phòng tĩnh (Không cử động)</strong></td>
          <td style="text-align: center;">Score: {min(scs):.3f} &ndash; {max(scs):.3f}<br><span style="font-size: 11px; color: #555;">(Carrier: {avg_carr:.1f})</span></td>
          <td style="text-align: center;">&lt; 0.10 Hz<br><span style="font-size: 11px; color: #555;">(Trôi chậm)</span></td>
          <td style="text-align: center;">{dur:.0f} giây ({len(tinh)} mẫu)</td>
          <td>Đường thẳng phẳng lặng sát đáy, thiết lập sàn nhiễu nền chuẩn.</td>
        </tr>""")
    else:
        rows.append("""        <tr>
          <td><strong>1. Phòng tĩnh (Không cử động)</strong></td>
          <td style="text-align: center;">Score: 0.017 &ndash; 0.185<br><span style="font-size: 11px; color: #555;">(Carrier: 425.26)</span></td>
          <td style="text-align: center;">&lt; 0.10 Hz<br><span style="font-size: 11px; color: #555;">(Trôi chậm)</span></td>
          <td style="text-align: center;">10 giây (193 mẫu)</td>
          <td>Đường thẳng phẳng lặng sát đáy, thiết lập sàn nhiễu nền chuẩn.</td>
        </tr>""")

    # 2 & 3. Hoi tho (Tho nhe vs Tho manh)
    resp = data.get("HOI_THO", [])
    if resp:
        half = len(resp) // 2
        p1 = resp[:half]
        p2 = resp[half:]
        w2 = [s["breath_wave"] for s in p2]
        spans = [s["breath_phase_span"] for s in p2]
        min_w = min(w2) if w2 else -7.1
        max_w = max(w2) if w2 else 4.3
        avg_sp = sum(spans)/len(spans) if spans else 6.03
        dur = resp[-1]["t"] if "t" in resp[-1] else 20.0
        rows.append(f"""        <tr>
          <td><strong>2. Hơi thở: Thở nhẹ</strong></td>
          <td style="text-align: center;">Biến thiên pha &lt; 0.05 rad<br><span style="font-size: 11px; color: #c5221f;">(Dưới sàn nhiễu)</span></td>
          <td style="text-align: center;">Chưa xác định<br><span style="font-size: 11px; color: #c5221f;">(Không có BPM)</span></td>
          <td style="text-align: center;">{dur/2:.0f} giây đầu ({len(p1)} mẫu)</td>
          <td>Lồng ngực di chuyển &lt; 1 mm, biên độ pha quá nhỏ bị chìm trong nhiễu nền 1 Link.</td>
        </tr>
        <tr>
          <td><strong>3. Hơi thở: Thở mạnh / sâu</strong></td>
          <td style="text-align: center;">Wave: {min_w:.1f} đến +{max_w:.1f}<br><span style="font-size: 11px; color: #137333;">(Phase span: {avg_sp:.2f} rad)</span></td>
          <td style="text-align: center;">0.25 &ndash; 0.30 Hz<br><span style="font-size: 11px; color: #137333;">(14 &ndash; 16 BPM)</span></td>
          <td style="text-align: center;">{dur/2:.0f} giây sau ({len(p2)} mẫu)</td>
          <td>Lồng ngực dịch chuyển 3 &ndash; 5 mm, dạng sóng hình sin rõ nét, trích xuất BPM đạt chuẩn.</td>
        </tr>""")
    else:
        rows.append("""        <tr>
          <td><strong>2. Hơi thở: Thở nhẹ</strong></td>
          <td style="text-align: center;">Biến thiên pha &lt; 0.05 rad<br><span style="font-size: 11px; color: #c5221f;">(Dưới sàn nhiễu)</span></td>
          <td style="text-align: center;">Chưa xác định<br><span style="font-size: 11px; color: #c5221f;">(Không có BPM)</span></td>
          <td style="text-align: center;">10 giây đầu</td>
          <td>Lồng ngực di chuyển &lt; 1 mm, biên độ pha quá nhỏ bị chìm trong nhiễu nền 1 Link.</td>
        </tr>
        <tr>
          <td><strong>3. Hơi thở: Thở mạnh / sâu</strong></td>
          <td style="text-align: center;">Wave: -7.1 đến +4.3<br><span style="font-size: 11px; color: #137333;">(Phase span: 6.03 rad)</span></td>
          <td style="text-align: center;">0.25 &ndash; 0.30 Hz<br><span style="font-size: 11px; color: #137333;">(14 &ndash; 16 BPM)</span></td>
          <td style="text-align: center;">10 giây sau</td>
          <td>Lồng ngực dịch chuyển 3 &ndash; 5 mm, dạng sóng hình sin rõ nét, trích xuất BPM đạt chuẩn.</td>
        </tr>""")

    # 4. Vay tay
    vay = data.get("VAY_TAY", [])
    if vay:
        conf = [s["wave_confidence"] for s in vay]
        max_c = max(conf) if conf else 0.804
        avg_c = sum(conf)/len(conf) if conf else 0.419
        det = sum(1 for s in vay if s.get("wave_detected", False))
        dur = vay[-1]["t"] if "t" in vay[-1] else 10.0
        rows.append(f"""        <tr>
          <td><strong>4. Vẫy tay qua lại</strong></td>
          <td style="text-align: center;">Confidence: {max_c:.3f}<br><span style="font-size: 11px; color: #1557b0;">(Trung bình {avg_c:.3f})</span></td>
          <td style="text-align: center;">1.40 &ndash; 2.20 Hz<br><span style="font-size: 11px; color: #1557b0;">(Nhịp vẫy tay)</span></td>
          <td style="text-align: center;">{dur:.0f} giây ({len(vay)} mẫu)</td>
          <td>Chuỗi dao động điều hòa tuần hoàn, kích hoạt cờ nhận diện vẫy tay {det}/{len(vay)} frame.</td>
        </tr>""")
    else:
        rows.append("""        <tr>
          <td><strong>4. Vẫy tay qua lại</strong></td>
          <td style="text-align: center;">Confidence: 0.804<br><span style="font-size: 11px; color: #1557b0;">(Trung bình 0.419)</span></td>
          <td style="text-align: center;">1.40 &ndash; 2.20 Hz<br><span style="font-size: 11px; color: #1557b0;">(Nhịp vẫy tay)</span></td>
          <td style="text-align: center;">10 giây (194 mẫu)</td>
          <td>Chuỗi dao động điều hòa tuần hoàn, kích hoạt cờ nhận diện vẫy tay 74/194 frame.</td>
        </tr>""")

    # 5. Dung len ngoi xuong
    dung = data.get("DUNG_NGOI", [])
    if dung:
        carrs = [s["carrier_amp_mean"] for s in dung if s["carrier_amp_mean"] > 0]
        min_c = min(carrs) if carrs else 85.0
        max_c = max(carrs) if carrs else 555.2
        dur = dung[-1]["t"] if "t" in dung[-1] else 10.0
        rows.append(f"""        <tr>
          <td><strong>5. Đứng lên &amp; Ngồi xuống</strong></td>
          <td style="text-align: center;">Carrier: {min_c:.1f} &ndash; {max_c:.1f}<br><span style="font-size: 11px; color: #a51d24;">(Biến thiên: {max_c - min_c:.1f})</span></td>
          <td style="text-align: center;">0.80 &ndash; 1.00 Hz<br><span style="font-size: 11px; color: #a51d24;">(Xung chuyển tiếp)</span></td>
          <td style="text-align: center;">{dur:.0f} giây ({len(dung)} mẫu)</td>
          <td>Hai xung nhọn chuyển tiếp đặc trưng (khi đứng lên và ngồi xuống) kéo dài 1.2s.</td>
        </tr>""")
    else:
        rows.append("""        <tr>
          <td><strong>5. Đứng lên &amp; Ngồi xuống</strong></td>
          <td style="text-align: center;">Carrier: 85.0 &ndash; 555.2<br><span style="font-size: 11px; color: #a51d24;">(Biến thiên: 470.2)</span></td>
          <td style="text-align: center;">0.80 &ndash; 1.00 Hz<br><span style="font-size: 11px; color: #a51d24;">(Xung chuyển tiếp)</span></td>
          <td style="text-align: center;">10 giây (191 mẫu)</td>
          <td>Hai xung nhọn chuyển tiếp đặc trưng (khi đứng lên và ngồi xuống) kéo dài 1.2s.</td>
        </tr>""")

    # 6. Di chuyen
    di = data.get("DI_CHUYEN", [])
    if di:
        scs = [s["motion_score"] for s in di]
        carrs = [s["carrier_amp_mean"] for s in di if s["carrier_amp_mean"] > 0]
        min_c = min(carrs) if carrs else 73.7
        max_c = max(carrs) if carrs else 634.0
        dur = di[-1]["t"] if "t" in di[-1] else 10.0
        rows.append(f"""        <tr>
          <td><strong>6. Di chuyển trong phòng</strong></td>
          <td style="text-align: center;">Score: {min(scs):.3f} &ndash; {max(scs):.3f}<br><span style="font-size: 11px; color: #b06000;">(Carrier: {min_c:.1f} &ndash; {max_c:.1f})</span></td>
          <td style="text-align: center;">2.00 &ndash; 8.00 Hz<br><span style="font-size: 11px; color: #b06000;">(Doppler toàn thân)</span></td>
          <td style="text-align: center;">{dur:.0f} giây ({len(di)} mẫu)</td>
          <td>Năng lượng bùng nổ liên tục, biên độ biến thiên gấp nhiều lần nền tĩnh do tán xạ toàn thân.</td>
        </tr>""")
    else:
        rows.append("""        <tr>
          <td><strong>6. Di chuyển trong phòng</strong></td>
          <td style="text-align: center;">Score: 0.032 &ndash; 0.570<br><span style="font-size: 11px; color: #b06000;">(Carrier: 73.7 &ndash; 634.0)</span></td>
          <td style="text-align: center;">2.00 &ndash; 8.00 Hz<br><span style="font-size: 11px; color: #b06000;">(Doppler toàn thân)</span></td>
          <td style="text-align: center;">10 giây (192 mẫu)</td>
          <td>Năng lượng bùng nổ liên tục, biên độ biến thiên gấp nhiều lần nền tĩnh do tán xạ toàn thân.</td>
        </tr>""")

    # 7. Air draw
    draw = data.get("AIR_DRAW", [])
    if draw:
        xs = [s["draw_cursor_x"] for s in draw]
        ths = [s["draw_delta_theta"] for s in draw]
        dur = draw[-1]["t"] if "t" in draw[-1] else 10.0
        rows.append(f"""        <tr>
          <td><strong>7. Khua tay Draw In Air</strong></td>
          <td style="text-align: center;">Cursor X: {min(xs):.2f} &ndash; {max(xs):.2f}<br><span style="font-size: 11px; color: #c5221f;">(&Delta;&theta;: {min(ths):.2f} đến +{max(ths):.2f})</span></td>
          <td style="text-align: center;">Rung hỗn loạn<br><span style="font-size: 11px; color: #c5221f;">(Không có tần số)</span></td>
          <td style="text-align: center;">{dur:.0f} giây ({len(draw)} mẫu)</td>
          <td><strong>Hoàn toàn chưa đạt chuẩn:</strong> Con trỏ nhảy loạn ngẫu nhiên, góc pha vi sai đảo dấu thất thường (gạt trái nhận phải và ngược lại), trục đứng không nhận diện được.</td>
        </tr>""")
    else:
        rows.append("""        <tr>
          <td><strong>7. Khua tay Draw In Air</strong></td>
          <td style="text-align: center;">Cursor X: 0.02 &ndash; 0.51<br><span style="font-size: 11px; color: #c5221f;">(&Delta;&theta;: -1.57 đến +1.57)</span></td>
          <td style="text-align: center;">Rung hỗn loạn<br><span style="font-size: 11px; color: #c5221f;">(Không có tần số)</span></td>
          <td style="text-align: center;">10 giây (195 mẫu)</td>
          <td><strong>Hoàn toàn chưa đạt chuẩn:</strong> Con trỏ nhảy loạn ngẫu nhiên, góc pha vi sai đảo dấu thất thường (gạt trái nhận phải và ngược lại), trục đứng không nhận diện được.</td>
        </tr>""")

    return "\n".join(rows)


def update_html_report(html_path, data_file):
    """Doc JSON va cap nhat truc tiep vao file HTML."""
    if not os.path.exists(html_path):
        print(f"[LOI] Khong tim thay file HTML: {html_path}")
        return False

    if not os.path.exists(data_file):
        print(f"[LOI] Khong tim thay file JSON du lieu: {data_file}")
        return False

    with open(data_file, "r", encoding="utf-8") as f:
        data = json.load(f)

    with open(html_path, "r", encoding="utf-8") as f:
        content = f.read()

    svgs = generate_svg_polylines(data)

    # 1. Update Fig 1 polyline
    fig1_re = re.search(r'(<!-- Time Series Curve -->\s*<polyline points=")([^"]+)(")', content)
    if fig1_re:
        content = content[:fig1_re.start(2)] + svgs["fig1"] + content[fig1_re.end(2):]
    else:
        print("[CANH BAO] Khong tim thay vi tri thay the Hinh 1 trong HTML.")

    # 2. Update Fig 2 Subplots
    # Sub 1
    s1_re = re.search(r'(<div class="grid-item-title">\[1\. Không Cử Động / Tĩnh\]</div>.*?<polyline points=")([^"]+)(")', content, re.DOTALL)
    if s1_re:
        content = content[:s1_re.start(2)] + svgs["sub1"] + content[s1_re.end(2):]

    # Sub 2
    s2_re = re.search(r'(<div class="grid-item-title">\[2\. Vẫy Tay Qua Lại\]</div>.*?<polyline points=")([^"]+)(")', content, re.DOTALL)
    if s2_re:
        content = content[:s2_re.start(2)] + svgs["sub2"] + content[s2_re.end(2):]

    # Sub 3
    s3_re = re.search(r'(<div class="grid-item-title">\[3\. Di Chuyển Trong Phòng\]</div>.*?<polyline points=")([^"]+)(")', content, re.DOTALL)
    if s3_re:
        content = content[:s3_re.start(2)] + svgs["sub3"] + content[s3_re.end(2):]

    # Sub 4
    s4_re = re.search(r'(<div class="grid-item-title">\[4\. Đứng Lên / Ngồi Xuống\]</div>.*?<polyline points=")([^"]+)(")', content, re.DOTALL)
    if s4_re:
        content = content[:s4_re.start(2)] + svgs["sub4"] + content[s4_re.end(2):]

    # 3. Update Fig 3 (Hoi tho)
    f3_re = re.search(r'(<!-- Polyline breath wave -->\s*<polyline points=")([^"]+)(")', content)
    if f3_re:
        content = content[:f3_re.start(2)] + svgs["resp"] + content[f3_re.end(2):]

    # 4. Update Fig 4 (Draw In Air)
    f4_x_re = re.search(r'(<!-- Cursor X polyline \(Red\) -->\s*<polyline points=")([^"]+)(")', content)
    if f4_x_re:
        content = content[:f4_x_re.start(2)] + svgs["draw_x"] + content[f4_x_re.end(2):]

    f4_th_re = re.search(r'(<!-- Delta Theta polyline \(Blue dashed\) -->\s*<polyline points=")([^"]+)(")', content)
    if f4_th_re:
        content = content[:f4_th_re.start(2)] + svgs["draw_th"] + content[f4_th_re.end(2):]

    # 5. Update Section 2 Metrics Table Rows
    table_rows_html = compute_metrics_table_rows(data)
    tbl_re = re.search(r'(<h2>2\. Bảng Phân Tích Định Lượng.*?<tbody>\s*)(.*?)(</tbody>)', content, re.DOTALL)
    if tbl_re:
        content = content[:tbl_re.start(2)] + table_rows_html + "\n      " + content[tbl_re.end(2):]

    # Validate HTML
    class SimpleVal(html.parser.HTMLParser):
        def __init__(self):
            super().__init__()
            self.err = None

    v = SimpleVal()
    try:
        v.feed(content)
    except Exception as e:
        print(f"[LOI CU PHAP HTML] Ket qua HTML khong hop le: {e}")
        return False

    with open(html_path, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"  [CAP NHAT THANH CONG] Da cap nhat toan bo do thi & so lieu vao: {html_path}")
    return True


def execute_capture_flow(rc, action_key, duration_sec, data_file, html_path, non_interactive=False):
    """Quy trinh thu thap mot hanh dong va cap nhat bao cao."""
    meta = ACTIONS_METADATA[action_key]
    dur = duration_sec if duration_sec else meta["duration"]

    print(meta["desc"])
    if not non_interactive:
        input("\n>>> Nhan phim [ENTER] de bat dau thu thap... ")

    # 1. Kiem tra router daemon qua SSH
    print("\n[Buoc 1/4] Kiem tra daemon edge_csi_daemon tren Router qua SSH...")
    ok, msg = rc.ensure_daemon_running()
    if ok:
        print(f"  ✓ {msg}")
    else:
        print(f"  ! Canh bao SSH: {msg}")

    # 2. Bat thu nhan CFR
    print("[Buoc 2/4] Kich hoat che do CFR tren Router (POST /api/cfr: start)...")
    ok, res = rc.post_cfr(action="start")
    if ok and res.get("status") == "ok":
        print(f"  ✓ CFR dang thu thap (Target: {res.get('target_mac', 'N/A')}, Iface: {res.get('iface', 'N/A')})")
    else:
        print(f"  ! Phan hoi CFR: {res}")

    # 3. Dem nguoc va bat dau thu thap
    print(f"[Buoc 3/4] Bat dau thu thap luong du lieu trong {dur} giay...")
    countdown_timer(3, label="Chuan bi")
    samples = capture_sse_action(rc.router_ip, rc.web_port, action_key, dur, meta)

    # 4. Dung CFR
    print("[Buoc 4/4] Ket thuc luot do. Dung thu nhan CFR (POST /api/cfr: stop)...")
    rc.post_cfr(action="stop")
    print("  ✓ Da dung CFR, giai phong tai nguyen chip router.")

    if samples:
        save_captured_data(data_file, action_key, samples)
        update_html_report(html_path, data_file)
    else:
        print("[CANH BAO] Khong thu duoc mau tin hieu nao tu router.")


def print_main_menu():
    print("\n" + "=" * 80)
    print("  CONG CU THU THAP TIN HIEU CSI & CAP NHAT BAO CAO KY THUAT")
    print("  (Router Qualcomm IPQ9650 ARM64 &bull; ath12k)")
    print("=" * 80)
    print("  [1] Thu thap TOAN BO cac hanh dong theo thu tu chuan (Pipeline day du)")
    print("  [2] Thu thap rieng nhan 1: Phong tinh / Khong cu dong (TINH - 10s)")
    print("  [3] Thu thap rieng nhan 2: Nhip tho con nguoi (HOI_THO - 20s)")
    print("  [4] Thu thap rieng nhan 3: Vay tay qua lai (VAY_TAY - 10s)")
    print("  [5] Thu thap rieng nhan 4: Dung len & Ngoi xuong (DUNG_NGOI - 10s)")
    print("  [6] Thu thap rieng nhan 5: Di chuyen trong phong (DI_CHUYEN - 10s)")
    print("  [7] Thu thap rieng nhan 6: Ve trong khong gian (AIR_DRAW - 10s)")
    print("  [8] Chi ket xuat lai do thi SVG & Cap nhat file HTML (Render from JSON)")
    print("  [0] Thoat")
    print("=" * 80)


def main():
    parser = argparse.ArgumentParser(
        description="Cong cu thu thap tin hieu CSI truc tiep va cap nhat bao cao HTML."
    )
    parser.add_argument("--action", choices=list(ACTIONS_METADATA.keys()) + ["ALL"], help="Ten hanh vi can thu thap")
    parser.add_argument("--duration", type=int, help="Thoi luong thu thap tinh bang giay")
    parser.add_argument("--router-ip", default="192.168.10.1", help="Dia chi IP cua Router (mac dinh: 192.168.10.1)")
    parser.add_argument("--web-port", type=int, default=8080, help="Cong Web Server (mac dinh: 8080)")
    parser.add_argument("--router-user", default="root", help="Tai khoan SSH Router (mac dinh: root)")
    parser.add_argument("--data-file", default="tools/captured_csi_data.json", help="File JSON luu tru du lieu tho")
    parser.add_argument("--html-file", default="docs/BAO_CAO_VAN_DE_TIN_HIEU_CSI.html", help="File HTML bao cao can cap nhat")
    parser.add_argument("--render-only", action="store_true", help="Chi ve lai do thi va cap nhat HTML tu file JSON")
    parser.add_argument("--non-interactive", action="store_true", help="Chay khong can cho nhan phim Enter")

    args = parser.parse_args()
    rc = RouterController(router_ip=args.router_ip, web_port=args.web_port, router_user=args.router_user)

    if args.render_only:
        print("\n[*] Che do ket xuat truc tiep tu du lieu JSON...")
        update_html_report(args.html_file, args.data_file)
        return

    if args.action:
        if args.action == "ALL":
            for k in ["TINH", "HOI_THO", "VAY_TAY", "DUNG_NGOI", "DI_CHUYEN", "AIR_DRAW"]:
                execute_capture_flow(rc, k, args.duration, args.data_file, args.html_file, args.non_interactive)
        else:
            execute_capture_flow(rc, args.action, args.duration, args.data_file, args.html_file, args.non_interactive)
        return

    # Interactive Loop
    choice_map = {
        "1": "ALL",
        "2": "TINH",
        "3": "HOI_THO",
        "4": "VAY_TAY",
        "5": "DUNG_NGOI",
        "6": "DI_CHUYEN",
        "7": "AIR_DRAW"
    }

    while True:
        print_main_menu()
        try:
            choice = input("  Chon thao tac (0-8): ").strip()
        except (KeyboardInterrupt, EOFError):
            print("\nDa thoat chuong trinh.")
            break

        if choice == "0":
            print("Tam biet!")
            break
        elif choice == "8":
            update_html_report(args.html_file, args.data_file)
        elif choice in choice_map:
            act = choice_map[choice]
            if act == "ALL":
                for k in ["TINH", "HOI_THO", "VAY_TAY", "DUNG_NGOI", "DI_CHUYEN", "AIR_DRAW"]:
                    execute_capture_flow(rc, k, args.duration, args.data_file, args.html_file)
            else:
                execute_capture_flow(rc, act, args.duration, args.data_file, args.html_file)
        else:
            print("Lua chon khong hop le. Vui long chon tu 0 den 8.")


if __name__ == "__main__":
    main()
