"""Tròn vs tam giác theo nét trong MIỀN TRỄ: CSI → IFFT → đáp ứng xung kênh (CIR), trừ nền tĩnh, rồi Doppler + ảnh tap × thời gian.

    .venv/bin/python sensing/stroke_cir.py [--router 192.168.11.1] [--label intent|gyro] [--since HH:MM] [--until ..] [--min-rate 25]

Mỗi bản ghi, mỗi chain: cửa sổ Hann qua 208 tone (giảm búp phụ) → IFFT 256 điểm → tap cách nhau 1/(256·312.5 kHz) = 12.5 ns (3.75 m).
Làm sạch (pha/trễ của 1 chain có STO/CFO ngẫu nhiên mỗi gói): dịch vòng để tap mạnh nhất của chain 0 về vị trí PRE, nhân mọi chain với
pha liên hợp của tap đó (giữ pha tương đối giữa chain), chia chuẩn để bỏ AGC; giữ NT tap. Nền = CIR trung bình 0.5 s TRƯỚC khi chạm
(không có thì trung bình cả nét) → phần động = CIR − nền. So với miền tần số (stroke_doppler.py) trên cùng các nét.
"""
import argparse, sys, time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import stroke_csi as SC  # noqa: E402
import stroke_doppler as D  # noqa: E402
import stroke_shape as SS  # noqa: E402

NFFT_CIR, PRE, NT = 256, 4, 24


def cir(h):
    """h: n x chain x 208 → CIR đã làm sạch n x chain x NT (phức)."""
    x = np.fft.ifft(h * np.hanning(h.shape[2]), NFFT_CIR, axis=2)
    pk = np.abs(x[:, 0]).argmax(1)
    out = np.stack([np.roll(x[i], PRE - pk[i], axis=1)[:, :NT] for i in range(len(x))])
    ref = out[:, 0, PRE]
    out = out * (np.conj(ref) / (np.abs(ref) + 1e-12))[:, None, None]
    return out / (np.linalg.norm(out, axis=2, keepdims=True) + 1e-12)


def analyze(st):
    row, se = D.analyze(st)
    r, pc, t, m = st["rec"], st["pc"], se["t"] + st["rec"]["t0"], se["m"]
    c = cir(st["csi"])
    pre = pc < r["t0"]
    bg = c[pre].mean(0) if pre.sum() >= 5 else c.mean(0)
    dyn = c - bg                                                                      # bỏ nền tĩnh
    flat = dyn.reshape(len(dyn), -1)
    g = np.stack([np.interp(t, pc, flat[:, j].real) + 1j * np.interp(t, pc, flat[:, j].imag) for j in range(flat.shape[1])], 1)
    e = (np.abs(g) ** 2).reshape(len(t), c.shape[1], NT).sum(1)                       # năng lượng động theo tap (cộng chain)
    u, s_, _ = np.linalg.svd(g - g.mean(0), full_matrices=False)
    x = u[:, 0] * s_[0]
    xp = np.pad(x, (D.WIN // 2, D.WIN - 1 - D.WIN // 2), "edge")
    frames = np.stack([xp[i:i + D.WIN] * np.hanning(D.WIN) for i in range(len(x))])
    spec = np.abs(np.fft.fftshift(np.fft.fft(frames, D.NFFT, axis=1), axes=1)) ** 2
    spec = spec / (spec.max(1, keepdims=True) + 1e-12)
    f = np.fft.fftshift(np.fft.fftfreq(D.NFFT, 1 / D.FS))
    etot = e.sum(1)
    img_tap = e[m][:, :16]
    img_tap = np.stack([np.interp(np.linspace(0, 1, 24), np.linspace(0, 1, len(img_tap)), img_tap[:, j]) for j in range(16)], 1)
    img_tap = np.log(img_tap + 1e-4); img_tap = (img_tap - img_tap.mean()) / (img_tap.std() + 1e-9)
    row.update(r_cir_energy_speed=float(np.corrcoef(etot[m], se["gs"][m])[0, 1]),
               dyn_ratio=float(etot[m].mean() / (etot[~m].mean() + 1e-12)) if (~m).sum() > 3 else np.nan,
               hf_cir=D.smooth_frac(D.tnorm(etot[m])), corners_cir=D.corners(etot[m], row["dur"])[0])
    se.update(e=e, etot=etot, spec_cir=spec, f_cir=f, img_cir_dfs=D.dfs_image(spec, f, m), img_tap=img_tap, bgtap=np.abs(bg).mean(0))
    return row, se


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("jsonl", nargs="?", default=str(SC.ROOT / "data/strokes/20261028.jsonl"))
    ap.add_argument("--out", default=str(SC.ROOT / "docs/strokes_cir"))
    ap.add_argument("--router", default="192.168.11.1")
    ap.add_argument("--label", choices=("intent", "gyro"), default="intent")
    ap.add_argument("--min-rate", type=float, default=0)
    ap.add_argument("--since", default="00:00")
    ap.add_argument("--until", default="23:59:59")
    ap.add_argument("--dur", default="0.8,99", help="chỉ nét có thời lượng trong khoảng lo,hi giây (cân thời lượng 2 lớp)")
    a = ap.parse_args()
    lo, hi = map(float, a.dur.split(","))
    st = []
    for s in SC.load(a.jsonl, a.router):
        if a.label == "gyro":
            s["rec"]["intent"] = s["rec"]["shape"]
        d = s["rec"]["t1"] - s["rec"]["t0"]
        if s["rec"]["intent"] in ("circle", "triangle") and lo <= d <= hi and s["rec"]["csi_rate"] >= a.min_rate \
                and a.since <= time.strftime("%H:%M:%S", time.localtime(s["rec"]["t0"])) < a.until:
            st.append(s)
    rows, ser = zip(*[analyze(s) for s in st])
    labs = [s["rec"]["intent"] for s in st]
    y = np.array([lab == "triangle" for lab in labs])
    import pandas as pd
    df = pd.DataFrame(rows)
    print(f"{len(st)} nét {a.router}: {(~y).sum()} tròn, {y.sum()} tam giác; CSI/s trung vị {df.csi_rate.median():.0f}; "
          f"thời lượng tròn/tg {df.dur[~y].median():.1f}/{df.dur[y].median():.1f} s; đoán lớp đông {max(y.mean(), 1 - y.mean()):.2f}")
    print(f"CIR động trong nét / trước-sau nét: {df.dyn_ratio.median():.2f}×;  năng lượng CIR động ~ tốc độ gyro r {df.r_cir_energy_speed.median():+.2f}")
    print("Bỏ-ra-1-nét, DTW 3-NN (ảnh thời gian × …):")
    for k, name in (("img_c", "tần số: DFS nhân liên hợp"), ("img_a", "tần số: DFS biên độ CARM"),
                    ("img_cir_dfs", "trễ (IFFT): DFS của CIR động"), ("img_tap", "trễ (IFFT): năng lượng tap × thời gian")):
        print(f"   {name:40s} {SS.loso_knn([s[k] for s in ser], labs):.2f}")
    print(f"   logistic CIR (chỗ khựng, góc)            {D.loso_1nn(df[['corners_cir', 'hf_cir']].to_numpy(), y):.2f}")
    print(f"   chỉ thời lượng                          {SS.loso(df[['dur']].to_numpy(), y):.2f}   gyro {D.loso_1nn(df[['corners_gyro', 'cv_gyro_spd', 'hf_gyro_spd', 'hf_gyro_vx']].to_numpy(), y):.2f}")

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    fig, ax = plt.subplots(2, 3, figsize=(15, 6.5))
    taps = (np.arange(NT) - PRE) * 3.75
    ax[0, 0].plot(taps, np.mean([s["bgtap"] for s in ser], 0), "k")
    ax[0, 0].set_title("CIR nền (trước khi chạm), TB các nét, chain 0", fontsize=9); ax[0, 0].set_xlabel("quãng đường thêm so với đường mạnh nhất (m)")
    for c_, lab in enumerate(("circle", "triangle")):
        i = next(j for j, l_ in enumerate(labs) if l_ == lab)
        s = ser[i]
        ax[0, c_ + 1].pcolormesh(s["t"], taps[:16], np.log(s["e"][:, :16].T + 1e-4), shading="auto", cmap="magma")
        ax[0, c_ + 1].axvline(0, color="c"); ax[0, c_ + 1].axvline(df.dur[i], color="c")
        ax[0, c_ + 1].set_title(f"1 nét {lab}: năng lượng CIR ĐỘNG (đã trừ nền) theo tap", fontsize=9)
        ax[0, c_ + 1].set_ylabel("m"); ax[0, c_ + 1].set_xlabel("giây (vạch xanh = chạm / thả)")
    mc = np.mean([s["img_tap"] for s, l_ in zip(ser, labs) if l_ == "circle"], 0)
    mt = np.mean([s["img_tap"] for s, l_ in zip(ser, labs) if l_ == "triangle"], 0)
    for c_, (img, ttl) in enumerate(((mc, "tròn"), (mt, "tam giác"), (mt - mc, "tam giác − tròn"))):
        ax[1, c_].pcolormesh(np.linspace(0, 1, 24), taps[:16], img.T, shading="auto", cmap="RdBu_r" if c_ == 2 else "magma",
                             **({"vmin": -1, "vmax": 1} if c_ == 2 else {}))
        ax[1, c_].set_title(f"TB lớp {ttl}: năng lượng CIR động", fontsize=9); ax[1, c_].set_xlabel("thời gian trong nét (0–1)"); ax[1, c_].set_ylabel("m")
    fig.tight_layout(); fig.savefig(out / "cir.png", dpi=80)
    print("→", out)


if __name__ == "__main__":
    # self-check: 2 đường (trễ 0 và 30 ns), trễ toàn cục (STO) + pha ngẫu nhiên → tap mạnh nhất về PRE, năng lượng gom quanh PRE
    f = (np.arange(208) - 104) * 312.5e3
    rng = np.random.default_rng(0)
    hh = np.stack([[np.exp(-2j * np.pi * f * (tau := rng.uniform(0, 200e-9))) * np.exp(1j * rng.uniform(0, 6.28))
                    * (1 + 0.5 * np.exp(-2j * np.pi * f * 30e-9))] for _ in range(5)])
    cc = np.abs(cir(hh))[:, 0]
    assert (cc.argmax(1) == PRE).all() and ((cc[:, PRE - 1:PRE + 6] ** 2).sum(1) > 0.8).all(), cc.argmax(1)
    main()
