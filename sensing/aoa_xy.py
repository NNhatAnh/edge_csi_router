"""AoA với 3 anten 11.1 ĐỂ NGUYÊN chỗ (không thẳng hàng): hình học đo tay, chỉ hiệu chuẩn offset pha.

    .venv/bin/python sensing/aoa_xy.py [data/raw] [--since 20261022-1520] [--gain 0|1|2]

Lượt dùng: router 11.1, nhãn "test", Vị trí hoặc Phiên = "A<góc>" (0° = thẳng phía trước router, + = bên phải),
Khoảng cách = "<số> cm|m". Hình học (2026-10-22, người dùng đo; chân anten, gốc = trái-sau, x sang phải, y ra trước):
chain 0 trái-sau (0, 0), chain 1 phải-sau (20, 0), chain 2 phải-giữa (20, 7) cm — giả định phải-giữa nằm phía trước phải-sau.
Mô hình gần trường: Δφ_c0 = −2π/λ·(|S−P_c| − |S−P_0|) + c_c, S = tâm mảng + r·(sinθ, cosθ). Dấu −: chọn trên dữ liệu
21/10 (tốt nhất ở cả 3 gain table) — kiểm lại trên bộ đo mới. Offset c_c đổi theo gain table và mỗi lần bật nguồn →
chỉ so các lượt cùng gain table, cùng một lần khởi động. Đánh giá: bỏ ra từng góc, khớp c trên các góc còn lại.
"""
import argparse, json, re, sys
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).parent))
import features as F

P = np.array([[0, 0], [0.20, 0], [0.20, 0.07]])
TH = np.radians(np.arange(-90, 90.5, 0.5))


def model(th, r, lam):
    s = P.mean(0) + r * np.stack([np.sin(th), np.cos(th)], -1)
    d = np.linalg.norm(s[..., None, :] - P, axis=-1)
    return -2 * np.pi / lam * (d[..., 1:] - d[..., :1])


def meters(text):
    m = re.fullmatch(r"\s*([\d.]+)\s*(cm|m)?\s*", str(text or ""))
    return float(m[1]) / (100 if m[2] == "cm" else 1) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data", nargs="?", default="data/raw", type=Path)
    ap.add_argument("--since", default="", help="chỉ lượt có tên ≥ chuỗi này (vd. 20261022-1520): cùng 1 lần khởi động/kênh")
    ap.add_argument("--gain", type=int, help="chỉ dùng gói có gain table này (mặc định: mọi gain)")
    A = ap.parse_args()
    rows = []
    for f in sorted(A.data.glob("*/label.json")):
        l = json.loads(f.read_text())
        if f.parent.name < A.since or l.get("exit_code"):
            continue
        pos = next((str(l.get(k) or "") for k in ("position", "session") if re.fullmatch(r"A-?\d+", str(l.get(k) or ""))), "")
        r = meters(l.get("distance"))
        mhz = re.search(r"channel (\d+) MHz", " ".join(l.get("quality", [])))
        if l.get("board") != "192.168.11.1" or l.get("label") != "test" or not pos or not r or not mhz:
            continue
        rec, h = F.load_recording(f.parent, iq=True)
        k = F.qc_keep(rec)
        if A.gain is not None:
            k &= pd.to_numeric(rec.wmi_gain_tbl_ch0, errors="coerce").to_numpy() == A.gain
        if k.sum() < 10:
            continue
        m = np.exp(1j * np.angle((h[k, 1:3] * np.conj(h[k, :1])).sum(-1))).mean(0)
        rows.append(dict(run=f.parent.name, angle=float(pos[1:]), r=r, lam=299792458 / (int(mhz[1]) * 1e6), n=int(k.sum()),
                         dphi=np.angle(m), cstd=np.degrees(np.sqrt(-2 * np.log(np.abs(m))))))
    if len(rows) < 3:
        sys.exit(f"cần ≥ 3 lượt góc (Vị trí = A<góc>, có Khoảng cách), mới có {len(rows)}")
    for x in sorted(rows, key=lambda x: x["angle"]):
        print(f"{x['angle']:+5.0f}°  r {x['r']:.2f} m  n {x['n']:4d}  Δφ10 {np.degrees(x['dphi'][0]):+5.0f}°  Δφ20 "
              f"{np.degrees(x['dphi'][1]):+5.0f}°  độ lệch vòng {x['cstd'].round(0)}  {x['run']}")
    errs = []
    for i, x in enumerate(rows):
        c = np.angle(np.mean([np.exp(1j * (y["dphi"] - model(np.radians(y["angle"]), y["r"], y["lam"])))
                              for j, y in enumerate(rows) if j != i], 0))
        cost = (np.angle(np.exp(1j * (x["dphi"] - model(TH, x["r"], x["lam"]) - c))) ** 2).sum(-1)
        errs.append(np.degrees(TH[np.argmin(cost)]) - x["angle"])
        print(f"  bỏ ra {x['angle']:+.0f}°: đoán {x['angle'] + errs[-1]:+.0f}°, sai {errs[-1]:+.0f}°")
    print(f"\nsai số bỏ-ra-1-góc: trung vị {np.median(np.abs(errs)):.0f}°, lớn nhất {np.max(np.abs(errs)):.0f}° "
          "(cổng ghép với RSSI: ≤ 15°)")


if __name__ == "__main__":
    main()
