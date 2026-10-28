"""Thử AoA 1 router (11.1, 3 chain): điện thoại ở các góc BIẾT TRƯỚC → lệch pha liên chain có đổi theo góc không?

    .venv/bin/python sensing/aoa_test.py [data/raw]

Lượt dùng: router 11.1, nhãn "test", Vị trí hoặc Phiên = "A<góc độ>" (vd. A-60, A0, A30), Khoảng cách = mét (vd. 2).
Mỗi lượt: Δφ_ab = arg Σ_tone H_a·conj(H_b) từng gói (chỉ gói 80 MHz, ưu tiên gain table 0 — kit guide §6: offset pha
đổi theo gain), trung bình vòng + độ lệch vòng. Mô hình mảng thẳng: Δφ_ab(θ) = 2π·d_ab·sinθ/λ + c_ab (mod 2π); d_ab, c_ab
khớp bằng lưới (|d| ≤ 0.25 m) — đây là HIỆU CHUẨN thiết bị (offset + khoảng cách hiệu dụng), không phải khảo sát phòng.
Đánh giá: bỏ ra từng góc, khớp trên các góc còn lại, đoán góc bị bỏ ra (θ ∈ [−90°, 90°]) → sai số góc thật.
shortcut: chỉ mô hình 1 đường truyền thẳng (không tách đa đường kiểu SpotFi) — nếu sai số lớn, bước sau là MUSIC/SpotFi.
"""
import json, sys
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).parent))
import features as F

LAMBDA = 299792458 / 5.21e9  # tâm kênh 80 MHz 5210 MHz
PAIRS = [(0, 1), (0, 2), (1, 2)]
D_GRID = np.linspace(-0.25, 0.25, 1001)
TH_GRID = np.radians(np.linspace(-90, 90, 361))


def wrap(x):
    return (x + np.pi) % (2 * np.pi) - np.pi


def run_stats(folder):
    rec, h = F.load_recording(folder, iq=True)
    keep = F.qc_keep(rec)
    g = pd.to_numeric(rec.wmi_gain_tbl_ch0, errors="coerce").to_numpy()
    use = keep & (g == 0)
    gain_note = "gain0"
    if use.sum() < 20:  # quá ít gói gain 0 → dùng mọi gói, ghi chú
        use, gain_note = keep, "mọi gain"
    out = dict(n=int(use.sum()), gain=gain_note,
               rssi=float(pd.to_numeric(rec.wmi_rssi_dbm_ch0, errors="coerce")[use].median()))
    for a, b in PAIRS:
        per = np.angle((h[use, a] * np.conj(h[use, b])).sum(1))
        m = np.mean(np.exp(1j * per))
        out[f"dphi_{a}{b}"] = float(np.angle(m))
        out[f"cstd_{a}{b}"] = float(np.degrees(np.sqrt(-2 * np.log(max(abs(m), 1e-9)))))
    return out


def fit_pair(theta, dphi):
    """d, c tốt nhất (lưới d, c = trung bình vòng phần dư) cho Δφ = k·sinθ + c."""
    best = None
    for d in D_GRID:
        pred = 2 * np.pi * d * np.sin(theta) / LAMBDA
        c = np.angle(np.mean(np.exp(1j * (dphi - pred))))
        res = np.sqrt(np.mean(wrap(dphi - pred - c) ** 2))
        if best is None or res < best[2]:
            best = (d, c, res)
    return best


def estimate(params, dphis):
    cost = np.zeros_like(TH_GRID)
    for (a, b), (d, c, _) in params.items():
        cost += wrap(dphis[(a, b)] - 2 * np.pi * d * np.sin(TH_GRID) / LAMBDA - c) ** 2
    return TH_GRID[np.argmin(cost)]


def main():
    data = Path(sys.argv[1] if len(sys.argv) > 1 else "data/raw")
    rows = []
    for f in sorted(data.glob("*/label.json")):
        l = json.loads(f.read_text())
        # góc ghi ở Vị trí hoặc Phiên dạng "A<độ>" (2026-10-21 người dùng điền ở Phiên)
        pos = next((str(l.get(k) or "") for k in ("position", "session") if str(l.get(k) or "").upper().startswith("A")), "")
        if l.get("board") != "192.168.11.1" or l.get("label") != "test" or not pos:
            continue
        try:
            ang = float(pos[1:])
        except ValueError:
            continue
        rows.append(dict(run=f.parent.name, angle=ang, dist=l.get("distance"), **run_stats(f.parent)))
    if len(rows) < 3:
        sys.exit(f"cần ≥ 3 lượt góc khác nhau (Vị trí = A<góc>), mới có {len(rows)}")
    T = pd.DataFrame(rows).sort_values("angle")
    print(T[["run", "angle", "dist", "n", "gain", "rssi"] + [f"dphi_{a}{b}" for a, b in PAIRS] +
            [f"cstd_{a}{b}" for a, b in PAIRS]].round(2).to_string(index=False))
    th = np.radians(T.angle.to_numpy())
    dph = {p: T[f"dphi_{p[0]}{p[1]}"].to_numpy() for p in PAIRS}
    full = {p: fit_pair(th, dph[p]) for p in PAIRS}
    print("\nkhớp toàn bộ: " + "; ".join(f"cặp {a}-{b}: d = {d * 100:.1f} cm, dư {np.degrees(r):.1f}°" for (a, b), (d, c, r) in full.items()))
    errs = []
    for i in range(len(T)):
        tr = np.arange(len(T)) != i
        params = {p: fit_pair(th[tr], dph[p][tr]) for p in PAIRS}
        est = estimate(params, {p: dph[p][i] for p in PAIRS})
        errs.append(np.degrees(est - th[i]))
        print(f"  bỏ ra {T.angle.iloc[i]:+.0f}°: đoán {np.degrees(est):+.0f}°, sai {errs[-1]:+.0f}°")
    errs = np.abs(errs)
    print(f"\nsai số góc (bỏ-ra-1-góc): trung vị {np.median(errs):.0f}°, lớn nhất {errs.max():.0f}°")
    if len(T) < 5:
        print("cảnh báo: < 5 góc → khớp 2 tham số mỗi cặp gần như không còn bậc tự do; thu đủ 5 góc trở lên")


if __name__ == "__main__":
    main()
