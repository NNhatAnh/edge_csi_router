"""Chỉnh Doppler biên độ (CARM) cho phần cứng RDP499 (~30 CSI/s không đều, AGC nhảy, 2–3 chain) theo ĐỘ BÁM TỐC ĐỘ GYRO.

    .venv/bin/python sensing/carm_tune.py [--router 192.168.11.1] [--min-rate 25]

Mục tiêu không cần nhãn hình (r giữa "tốc độ" từ CSI và tốc độ góc gyro, trung vị qua nét, cho phép trễ ±0.3 s) → chọn cấu hình
không khớp vào nhãn tròn/tam giác. Biến thể:
  win     cửa sổ STFT (mẫu @30 Hz): ngắn = theo kịp góc tam giác, dài = phân giải tần số
  pcs     thành phần PCA: (0,) = PC1; (1,2,3) = CARM gốc (bỏ PC1 vì chứa nhiễu xung/AGC)
  top     chỉ giữ tỉ lệ tone biến thiên mạnh nhất (tone nhạy chuyển động)
  med     lọc trung vị 3 mẫu theo thời gian từng tone (bỏ gói lỗi/nhảy AGC đơn lẻ)
  proxy   f50 (đường 50 % năng lượng, CARM) / band (năng lượng 1–8 Hz) / fmean
"""
import argparse, itertools, sys
from pathlib import Path

import numpy as np
from scipy.ndimage import median_filter

sys.path.insert(0, str(Path(__file__).parent))
import stroke_csi as SC  # noqa: E402

FS = 30.0
NFFT = 64


def carm(h, pc, t, win=12, pcs=(0,), top=1.0, med=False, proxy="f50"):
    """h: n x chain x 208, pc: giờ đo (s), t: lưới đều → (phổ T x F một phía, f, chuỗi tốc độ T)."""
    tot = 0
    for c in range(h.shape[1]):
        a = np.abs(h[:, c]); a = a / (a.mean(1, keepdims=True) + 1e-9)
        g = np.stack([np.interp(t, pc, a[:, j]) for j in range(a.shape[1])], 1)
        if med:
            g = median_filter(g, size=(3, 1), mode="nearest")
        g = g - g.mean(0)
        if top < 1:
            v = g.var(0); g = g[:, v >= np.quantile(v, 1 - top)]
        u, s, _ = np.linalg.svd(g, full_matrices=False)
        for k in pcs:
            if k >= len(s):
                continue
            x = u[:, k] * s[k]
            xp = np.pad(x, (win // 2, win - 1 - win // 2), "edge")
            fr = np.stack([xp[i:i + win] * np.hanning(win) for i in range(len(x))])
            tot = tot + np.abs(np.fft.rfft(fr - fr.mean(1, keepdims=True), NFFT, axis=1)) ** 2
    f = np.fft.rfftfreq(NFFT, 1 / FS)
    p = tot[:, 1:]; ff = f[1:]
    if proxy == "f50":
        cum = np.cumsum(p, 1); sp = ff[(cum < 0.5 * cum[:, -1:]).sum(1)]
    elif proxy == "band":
        sp = p[:, (ff >= 1) & (ff <= 8)].sum(1)
    else:
        sp = (p * ff).sum(1) / (p.sum(1) + 1e-12)
    return tot / (tot.max(1, keepdims=True) + 1e-12), f, sp


def lagged_r(a, b, max_lag):
    best = -2
    for L in range(-max_lag, max_lag + 1):
        x, y = (a[L:], b[:len(b) - L]) if L >= 0 else (a[:L], b[-L:])
        if len(x) > 8 and x.std() > 0 and y.std() > 0:
            best = max(best, float(np.corrcoef(x, y)[0, 1]))
    return best


def prep(s):
    r = s["rec"]
    t = np.arange(s["pc"][0], s["pc"][-1], 1 / FS)
    gx = np.interp(t, s["g_pc"], s["g_xy"][:, 0]); gy = np.interp(t, s["g_pc"], s["g_xy"][:, 1])
    gs = np.hypot(np.gradient(gx, 1 / FS), np.gradient(gy, 1 / FS))
    gs = np.convolve(np.pad(gs, 2, "edge"), np.ones(5) / 5, "valid")
    return t, gs, (t >= r["t0"]) & (t <= r["t1"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--router", default="192.168.11.1")
    ap.add_argument("--min-rate", type=float, default=25)
    a = ap.parse_args()
    st = [s for s in SC.load(SC.ROOT / "data/strokes/20261028.jsonl", a.router)
          if s["rec"]["t1"] - s["rec"]["t0"] >= 1.0 and s["rec"]["csi_rate"] >= a.min_rate and len(s["pc"]) >= 30]
    pre = [prep(s) for s in st]
    print(f"{len(st)} nét {a.router} (≥ 1 s, ≥ {a.min_rate} CSI/s). r = tương quan tốc độ CSI ~ tốc độ gyro trong nét, trung vị (trễ ±0.3 s)")
    res = []
    for win, pcs, top, med, proxy in itertools.product((8, 12, 16), ((0,), (1, 2, 3), (0, 1, 2)), (1.0, 0.5), (False, True), ("f50", "band", "fmean")):
        rs = []
        for s, (t, gs, m) in zip(st, pre):
            _, _, sp = carm(s["csi"], s["pc"], t, win, pcs, top, med, proxy)
            rs.append(lagged_r(sp[m], gs[m], int(0.3 * FS)))
        res.append((float(np.median(rs)), float(np.mean(np.array(rs) > 0.3)), win, pcs, top, med, proxy))
    res.sort(key=lambda x: -x[0])
    print(f"{'r trung vị':>10s} {'%nét r>0.3':>10s}  win  pcs        top  med    proxy")
    for r_, frac, *cfg in res[:8] + [x for x in res if x[2:] == [12, (0,), 1.0, False, "f50"] or tuple(x[2:]) == (12, (0,), 1.0, False, "f50")][:1]:
        print(f"{r_:10.2f} {frac:10.0%}  {cfg[0]:3d}  {str(cfg[1]):9s}  {cfg[2]:.1f}  {str(cfg[3]):5s}  {cfg[4]}")
    print("(dòng cuối = cấu hình cũ trong stroke_doppler: win 12, PC1, mọi tone, không lọc, f50)")


if __name__ == "__main__":
    # self-check: đường phản xạ có độ dài đổi với tốc độ tăng dần (Doppler 2 → 6 Hz) chồng lên đường thẳng → fading chọn lọc tần số
    # (chuẩn hoá theo record khử thang chung, nên phải là fading khác nhau giữa tone) → proxy f50 tăng theo
    tt = np.arange(0, 3, 1 / 31.0)
    ph = 2 * np.pi * np.cumsum(np.linspace(2, 6, len(tt))) / 31.0
    fk = (np.arange(208) - 104) * 312.5e3
    hh = (1 + 0.4 * np.exp(1j * (ph[:, None] + 2 * np.pi * fk[None] * 40e-9)))[:, None, :]
    hh = hh + 0.01 * np.random.default_rng(0).normal(size=hh.shape)
    _, _, sp = carm(hh, tt, np.arange(0, 3, 1 / FS))
    assert np.corrcoef(sp[10:-10], np.linspace(2, 6, len(sp))[10:-10])[0, 1] > 0.7, sp
    main()
