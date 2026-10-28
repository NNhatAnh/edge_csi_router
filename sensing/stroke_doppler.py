"""Tròn vs tam giác theo nét, xử lý kiểu Widar3.0 / CARM / IndoTrack trên CSI 11.1 (gyro iPhone chỉ làm sự thật để đối chiếu).

    .venv/bin/python sensing/stroke_doppler.py [data/strokes/20261028.jsonl] [--out docs/strokes_doppler]

Các bước giống paper (thay 1000 gói/s đều của Intel 5300 bằng ~30–40 CSI/s không đều của 11.1):
  1. Nhân liên hợp giữa anten H0·conj(Hk) (Widar3.0, IndoTrack): khử CFO/STO/SFO chung, giữ phần thay đổi theo chuyển động
  2. Nội suy về lưới đều FS Hz, bỏ thành phần tĩnh (trừ trung bình) — thay bộ lọc Butterworth của Widar
  3. PCA qua subcarrier (CARM): thành phần 1 = biến thiên chung do điện thoại di chuyển
  4. STFT → phổ Doppler có dấu (DFS); tần số trung bình theo công suất f(t) = "vận tốc" của link
  5. Profile tốc độ CARM: công suất DFS ngoài DC theo thời gian; tam giác khựng ở 3 góc, tròn đi đều
Ghi chú vật lý: điện thoại là MÁY PHÁT đang chuyển động → Doppler của chính đường truyền là chung cho mọi anten và BỊ KHỬ ở bước 1;
phần còn lại là chênh lệch đường đi tới 2 anten (đổi theo góc ngang) → f(t) ~ vận tốc NGANG, không thấy chiều dọc.
"""
import argparse, sys, time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import stroke_csi as SC  # noqa: E402
import carm_tune as CT  # noqa: E402

FS = 30.0
WIN, HOP, NFFT = 12, 1, 64   # STFT 0.4 s, bước 1 mẫu
N = 50                       # chuẩn hoá thời gian trong nét → 50 điểm


def dfs(h, pc, lo, hi):
    """CSI n x 3 x 208 + giờ PC → (lưới t, phổ Doppler T x NFFT (tần số có dấu, chuẩn hoá theo khung), f, f_mean(t), tốc độ(t), pha 0-1)."""
    r = np.concatenate([h[:, 0] * np.conj(h[:, k]) for k in range(1, h.shape[1])], 1)
    r = r / (np.abs(r).mean(1, keepdims=True) + 1e-9)
    t = np.arange(lo, hi, 1 / FS)
    g = np.stack([np.interp(t, pc, r[:, j].real) + 1j * np.interp(t, pc, r[:, j].imag) for j in range(r.shape[1])], 1)
    d = g - g.mean(0)
    u, s, _ = np.linalg.svd(d, full_matrices=False)
    x = u[:, 0] * s[0]
    xp = np.pad(x, (WIN // 2, WIN - 1 - WIN // 2), "edge")
    frames = np.stack([xp[i:i + WIN] * np.hanning(WIN) for i in range(len(x))])
    p = np.abs(np.fft.fftshift(np.fft.fft(frames, NFFT, axis=1), axes=1)) ** 2
    f = np.fft.fftshift(np.fft.fftfreq(NFFT, 1 / FS))
    nz = np.abs(f) > 0.3
    fmean = (p[:, nz] * f[nz]).sum(1) / (p[:, nz].sum(1) + 1e-12)
    speed = p[:, nz].sum(1)
    ph = np.unwrap(np.angle(g[:, :208].sum(1)))
    return t, p / (p.max(1, keepdims=True) + 1e-12), f, fmean, speed, ph


def dfs_amp(h, pc, t):
    """CARM: biên độ |H| từng chain (chuẩn hoá theo record → bỏ AGC), bỏ tĩnh, PCA qua tone, STFT → phổ một phía theo |f|,
    cộng các chain. Điện thoại là máy phát nên CFO/STO làm hỏng pha 1 chain, nhưng biên độ thì không: mọi đường truyền đổi độ dài
    khi điện thoại di chuyển → biên độ dao động ở tần số ~ v/λ. Cấu hình chọn bằng carm_tune.py theo độ bám tốc độ gyro (11.1, 49 nét):
    PC1–3, cửa sổ 16 mẫu (0.53 s), tốc độ = năng lượng 1–8 Hz → r 0.67 (cũ PC1/12 mẫu/f50: 0.53). Trả (phổ T x F, f ≥ 0, tốc độ(t))."""
    return CT.carm(h, pc, t, win=16, pcs=(0, 1, 2), proxy="band")


def dfs_image(spec, f, m, fmax=8.0, nt=24):
    """Phổ trong nét → ảnh nt thời gian x các bin |f| ≤ fmax (kiểu đầu vào DFS của Widar), log + chuẩn hoá z."""
    k = np.abs(f) <= fmax
    sp = spec[m][:, k]
    img = np.stack([np.interp(np.linspace(0, 1, nt), np.linspace(0, 1, len(sp)), sp[:, j]) for j in range(sp.shape[1])], 1)
    img = np.log(img + 1e-3)
    return (img - img.mean()) / (img.std() + 1e-9)


def tnorm(v):
    v = np.interp(np.linspace(0, 1, N), np.linspace(0, 1, len(v)), v)
    return (v - v.mean()) / (v.std() + 1e-9)


def corners(spd, dur):
    """số 'chỗ khựng': cực tiểu cục bộ của tốc độ (đã làm mượt) < 50 % trung vị, cách nhau ≥ 12 % thời lượng nét."""
    s = np.convolve(np.pad(spd, 2, "edge"), np.ones(5) / 5, "valid")
    s = s / (np.median(s) + 1e-12)
    idx = [i for i in range(2, len(s) - 2) if s[i] == s[i - 2:i + 3].min() and s[i] < 0.5]
    out, gap = [], 0.12 * len(s)
    for i in idx:
        if not out or i - out[-1] >= gap:
            out.append(i)
    return len(out), np.array(out) / max(len(s) - 1, 1)


def smooth_frac(v):
    """tỉ lệ năng lượng của chuỗi (đã chuẩn hoá thời gian) nằm ở bậc FFT ≥ 3: tròn ~ 1 chu kỳ (bậc 1), tam giác có góc → bậc cao."""
    p = np.abs(np.fft.rfft(v - v.mean())) ** 2
    return float(p[3:].sum() / (p[1:].sum() + 1e-12))


def analyze(st):
    r = st["rec"]
    t0, t1 = r["t0"], r["t1"]
    t, spec, f, fm, spd, ph = dfs(st["csi"], st["pc"], st["pc"][0], st["pc"][-1])
    spec_a, fa, f50 = dfs_amp(st["csi"], st["pc"], t)
    gx = np.interp(t, st["g_pc"], st["g_xy"][:, 0])
    gy = np.interp(t, st["g_pc"], st["g_xy"][:, 1])
    m = (t >= t0) & (t <= t1)
    vx, vy = np.gradient(gx, 1 / FS), np.gradient(gy, 1 / FS)
    gs = np.hypot(vx, vy)
    dur = t1 - t0
    nc_g, pos_g = corners(gs[m], dur)
    nc_c, pos_c = corners(spd[m], dur)
    row = dict(t0=t0, intent=r["intent"], dur=dur, csi_rate=r["csi_rate"],
               r_fmean_vx=float(np.corrcoef(fm[m], vx[m])[0, 1]), r_dphase_vx=float(np.corrcoef(np.gradient(ph[m]), vx[m])[0, 1]),
               r_speed=float(np.corrcoef(spd[m], gs[m])[0, 1]), r_f50_speed=float(np.corrcoef(f50[m], gs[m])[0, 1]),
               corners_f50=corners(f50[m], dur)[0], hf_f50=smooth_frac(tnorm(f50[m])), cv_f50=float(f50[m].std() / (f50[m].mean() + 1e-12)),
               corners_gyro=nc_g, corners_csi=nc_c,
               hf_gyro_vx=smooth_frac(tnorm(vx[m])), hf_csi_fm=smooth_frac(tnorm(fm[m])),
               hf_gyro_spd=smooth_frac(tnorm(gs[m])), hf_csi_spd=smooth_frac(tnorm(spd[m])),
               cv_gyro_spd=float(gs[m].std() / gs[m].mean()), cv_csi_spd=float(spd[m].std() / spd[m].mean()))
    return row, dict(t=t - t0, spec=spec, f=f, fm=fm, spd=spd, vx=vx, gs=gs, gx=gx, gy=gy, m=m, pos_g=pos_g, pos_c=pos_c,
                     spec_a=spec_a, fa=fa, f50=f50, img_c=dfs_image(spec, f, m), img_a=dfs_image(spec_a, fa, m))


def auc(a, b):
    a, b = np.asarray(a), np.asarray(b)
    return float(((a[:, None] > b[None]) + 0.5 * (a[:, None] == b[None])).mean())


def loso_1nn(x, y):
    """chuẩn hoá z rồi logistic bỏ-ra-1-nét."""
    from sklearn.linear_model import LogisticRegression
    from sklearn.pipeline import make_pipeline
    from sklearn.preprocessing import StandardScaler
    ok = 0
    for i in range(len(y)):
        k = np.arange(len(y)) != i
        clf = make_pipeline(StandardScaler(), LogisticRegression(C=0.5, max_iter=1000)).fit(x[k], y[k])
        ok += clf.predict(x[i:i + 1])[0] == y[i]
    return ok / len(y)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("jsonl", nargs="?", default=str(SC.ROOT / "data/strokes/20261028.jsonl"))
    ap.add_argument("--out", default=str(SC.ROOT / "docs/strokes_doppler"))
    ap.add_argument("--min-rate", type=float, default=0, help="chỉ nét có CSI/s ≥ giá trị này")
    ap.add_argument("--since", default="00:00", help="chỉ nét bắt đầu từ HH:MM[:SS] (giờ máy)")
    ap.add_argument("--until", default="23:59:59", help="chỉ nét bắt đầu trước HH:MM[:SS]")
    ap.add_argument("--router", default="192.168.11.1", help="chỉ nét của router này (không trộn router)")
    ap.add_argument("--label", choices=("intent", "gyro"), default="intent",
                    help="gyro = nhãn là hình $1 nhận từ gyro (khi quên đổi ô 'Định vẽ'); nét gyro 'unknown' bị bỏ")
    a = ap.parse_args()
    loaded = SC.load(a.jsonl, a.router)
    if a.label == "gyro":
        for s in loaded:
            s["rec"]["intent"] = s["rec"]["shape"]
    strokes = [s for s in loaded if s["rec"].get("intent") in ("circle", "triangle")
               and s["rec"]["t1"] - s["rec"]["t0"] >= 0.8 and s["rec"]["csi_rate"] >= a.min_rate
               and a.since <= time.strftime("%H:%M:%S", time.localtime(s["rec"]["t0"])) < a.until]
    rows, ser = zip(*[analyze(s) for s in strokes])
    import pandas as pd
    df = pd.DataFrame(rows)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    df.to_csv(out / "strokes.csv", index=False)
    c, tr = df[df.intent == "circle"], df[df.intent == "triangle"]
    print(f"{len(df)} nét: {len(c)} tròn, {len(tr)} tam giác (≥ 0.8 s, CSI ≥ {a.min_rate}/s)\n")
    print("1) CSI có bám chuyển động không (trung vị qua nét, tròn / tam giác):")
    for k, name in (("r_fmean_vx", "Doppler TB f(t) ~ vận tốc ngang gyro"), ("r_dphase_vx", "đạo hàm pha 0-1 ~ vận tốc ngang"),
                    ("r_speed", "tốc độ (công suất DFS) ~ tốc độ gyro"), ("r_f50_speed", "CARM tốc độ (năng lượng 1–8 Hz) ~ tốc độ gyro")):
        print(f"   {name:40s} {c[k].median():+.2f} / {tr[k].median():+.2f}")
    print("\n2) Dấu hiệu hình (tròn / tam giác, AUC = khả năng tách 1 feature; 0.5 = không tách):")
    for k, name in (("corners_gyro", "số chỗ khựng — gyro"), ("corners_csi", "số chỗ khựng — CSI"),
                    ("cv_gyro_spd", "dao động tốc độ CV — gyro"), ("cv_csi_spd", "dao động tốc độ CV — CSI"),
                    ("hf_gyro_spd", "tốc độ có góc (bậc ≥3) — gyro"), ("hf_csi_spd", "tốc độ có góc (bậc ≥3) — CSI"),
                    ("hf_gyro_vx", "vận tốc ngang có góc — gyro"), ("hf_csi_fm", "Doppler f(t) có góc — CSI"),
                    ("corners_f50", "chỗ khựng — CARM tốc độ"), ("hf_f50", "CARM tốc độ có góc (bậc ≥3)"),
                    ("dur", "thời lượng (kiểm lẫn)"), ("csi_rate", "CSI/s (kiểm lẫn)")):
        print(f"   {name:34s} {c[k].median():6.2f} / {tr[k].median():6.2f}   AUC {max(auc(tr[k], c[k]), 1 - auc(tr[k], c[k])):.2f}")
    y = (df.intent == "triangle").to_numpy()
    print(f"\n3) Bỏ-ra-1-nét (logistic), đoán lớp đông hơn = {max(y.mean(), 1 - y.mean()):.2f}:")
    for name, cols in (("gyro (corners, cv, hf)", ["corners_gyro", "cv_gyro_spd", "hf_gyro_spd", "hf_gyro_vx"]),
                       ("CSI (corners, cv, hf)", ["corners_csi", "cv_csi_spd", "hf_csi_spd", "hf_csi_fm"]),
                       ("CSI CARM (tốc độ)", ["corners_f50", "hf_f50", "cv_f50"]),
                       ("chỉ thời lượng", ["dur"])):
        print(f"   {name:26s} {loso_1nn(df[cols].to_numpy(), y):.2f}")
    import stroke_shape as SS
    labs = list(df.intent)
    for k, name in (("img_c", "ảnh DFS nhân liên hợp (Widar)"), ("img_a", "ảnh DFS biên độ (CARM)")):
        print(f"   {name:26s} {SS.loso_knn([s[k] for s in ser], labs):.2f}  (DTW 3-NN trên ảnh thời gian x tần số)")

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    pick = [i for i in np.argsort(-df.csi_rate.to_numpy()) if df.intent[i] == "circle"][:2] + \
           [i for i in np.argsort(-df.csi_rate.to_numpy()) if df.intent[i] == "triangle"][:2]
    fig, ax = plt.subplots(len(pick), 4, figsize=(16, 3 * len(pick)), squeeze=False)
    for row_ax, i in zip(ax, pick):
        s = ser[i]
        z = lambda v: (v - v[s["m"]].mean()) / (v[s["m"]].std() + 1e-9)  # noqa: E731
        row_ax[0].plot(s["gx"][s["m"]], s["gy"][s["m"]], "k")
        row_ax[0].set_aspect("equal"); row_ax[0].set_title(f"{df.intent[i]} — gyro (sự thật)", fontsize=9)
        row_ax[1].pcolormesh(s["t"], s["f"], s["spec"].T, shading="auto", cmap="magma")
        row_ax[1].plot(s["t"], s["fm"], "c", lw=1)
        row_ax[1].set_ylim(-8, 8); row_ax[1].set_ylabel("Hz"); row_ax[1].set_title("phổ Doppler CSI (DFS) + f(t)", fontsize=9)
        row_ax[2].plot(s["t"], z(s["vx"]), "k", label="gyro vận tốc ngang"); row_ax[2].plot(s["t"], z(s["fm"]), "c", label="CSI Doppler f(t)")
        row_ax[2].set_title(f"r = {df.r_fmean_vx[i]:+.2f}", fontsize=9)
        row_ax[3].plot(s["t"], z(s["gs"]), "k", label="gyro tốc độ"); row_ax[3].plot(s["t"], z(s["spd"]), "m", label="CSI tốc độ (CARM)")
        row_ax[3].set_title(f"chỗ khựng gyro {df.corners_gyro[i]} / CSI {df.corners_csi[i]}", fontsize=9)
        for k in (1, 2, 3):
            row_ax[k].axvspan(0, df.dur[i], color="0.92", zorder=-1)
        row_ax[2].legend(fontsize=6); row_ax[3].legend(fontsize=6)
    ax[-1][2].set_xlabel("giây từ lúc chạm (nền xám = đang vẽ)")
    fig.tight_layout(); fig.savefig(out / "examples.png", dpi=80)
    fig, ax = plt.subplots(1, 4, figsize=(16, 3.2))
    for k, (col, name) in enumerate((("corners_gyro", "chỗ khựng gyro"), ("corners_csi", "chỗ khựng CSI"),
                                     ("hf_gyro_spd", "tốc độ có góc — gyro"), ("hf_csi_spd", "tốc độ có góc — CSI"))):
        ax[k].hist([c[col], tr[col]], bins=8, label=["tròn", "tam giác"]); ax[k].set_title(name, fontsize=9)
    ax[0].legend(); fig.tight_layout(); fig.savefig(out / "features.png", dpi=80)
    # phổ Doppler trung bình mỗi lớp (thời gian trong nét chuẩn hoá 0–1): nhìn bằng mắt 2 lớp có khác nhau không
    fig, ax = plt.subplots(2, 3, figsize=(15, 6))
    for r_, (k, fk, name) in enumerate((("img_c", "f", "DFS nhân liên hợp H0·H1* (Hz, có dấu)"), ("img_a", "fa", "DFS biên độ CARM (|f| Hz)"))):
        fr = ser[0][fk][np.abs(ser[0][fk]) <= 8.0]
        mc = np.mean([s[k] for s, lab in zip(ser, labs) if lab == "circle"], 0)
        mt = np.mean([s[k] for s, lab in zip(ser, labs) if lab == "triangle"], 0)
        for c_, (img, ttl) in enumerate(((mc, f"tròn (n={labs.count('circle')})"), (mt, f"tam giác (n={labs.count('triangle')})"), (mt - mc, "tam giác − tròn"))):
            ax[r_, c_].pcolormesh(np.linspace(0, 1, img.shape[0]), fr, img.T, shading="auto", cmap="RdBu_r" if c_ == 2 else "magma",
                                  **({"vmin": -1, "vmax": 1} if c_ == 2 else {}))
            ax[r_, c_].set_title(f"{name}: {ttl}", fontsize=8); ax[r_, c_].set_ylabel("Hz")
    for c_ in range(3):
        ax[1, c_].set_xlabel("thời gian trong nét (0 = chạm, 1 = thả)")
    fig.tight_layout(); fig.savefig(out / "class_mean.png", dpi=80)
    print("→", out)


if __name__ == "__main__":
    t = np.arange(0, 2, 1 / FS)
    assert corners(np.abs(np.sin(np.pi * 1.5 * t)) + 0.05, 2)[0] in (2, 3)   # tam giác: 3 lần dừng
    assert corners(np.ones_like(t) + 0.05 * np.sin(t), 2)[0] == 0             # tròn: đều
    assert smooth_frac(tnorm(np.sin(2 * np.pi * t / 2))) < 0.05 < smooth_frac(tnorm(np.sign(np.sin(2 * np.pi * 1.5 * t / 2))))
    main()
