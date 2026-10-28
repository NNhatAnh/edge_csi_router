"""CSI 11.1 ↔ gyro iPhone theo từng nét vẽ (data/strokes/<ngày>.jsonl + csi/*.npz, lưu bởi viz/server.py --draw-port).

    .venv/bin/python sensing/stroke_csi.py [data/strokes/20261028.jsonl] [--out docs/strokes_csi]

Mỗi nét: nội suy quỹ đạo gyro (x, y độ) về thời điểm từng bản ghi CSI (giờ PC), rồi đo
  - tốc độ: |Δ CSI-ratio| mỗi bản ghi (như shape.py) so với tốc độ góc gyro → tương quan Pearson
  - quỹ đạo: pha CSI-ratio H0·conj(Hk) (k = 1, 2) và biên độ chuẩn hoá từng chain so với x, y gyro
  - lúc vẽ vs đệm 0.5 s trước/sau nét (ngón tay đã chạm/nhấc nhưng tay chưa vẽ)
Không phải đánh giá model: chỉ xem CSI có mang thông tin chuyển động của điện thoại không.
"""
import argparse, json, sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "viz"))
import draw as DR  # noqa: E402


def load(jsonl, router=None):
    """router: chỉ nét của router này (nét cũ không ghi router = 11.1); None = tất cả."""
    out = []
    for line in Path(jsonl).read_text().splitlines():
        r = json.loads(line)
        if not r.get("csi_file") or len(r["samples"]) < 20 or router and r.get("router", "192.168.11.1") != router:
            continue
        z = np.load(Path(jsonl).parent / r["csi_file"])
        s = r["samples"]
        g_pc = r["t0"] + (np.array([x["t"] for x in s]) - s[0]["t"]) / 1000  # mẫu đầu ≈ lúc chạm (t0)
        g_xy = np.array(DR.integrate(s))
        # giờ PC lúc gói TỚI máy (pc_time) đến theo cụm qua ssh (khoảng cách trung vị 0.04 ms, lệch tới ~60 ms) → thời điểm đo thật
        # = TSF của chip (đều ~21 ms) + độ trễ nhỏ nhất (gói tới sớm nhất ≈ trễ thật)
        tsf = z["t_tsf_s"]
        out.append(dict(rec=r, pc=tsf + (z["pc_time"] - tsf).min(), pc_arrival=z["pc_time"], csi=z["csi"].astype(complex),
                        g_pc=g_pc, g_xy=g_xy))
    return out


def csi_series(h):
    r = np.stack([h[:, 0] * np.conj(h[:, k]) for k in range(1, h.shape[1])], 1)  # n x cặp x 208 (5.1: chỉ 2 chain → 1 cặp)
    r = r / (np.abs(r).mean(2, keepdims=True) + 1e-9)
    ph = np.unwrap(np.angle(r.sum(2)), axis=0)                               # pha CSI-ratio 0-1, 0-2
    amp = np.abs(h).mean(2)
    amp = amp / amp.mean(0)                                                  # biên độ chuẩn hoá từng chain
    spd = np.r_[np.nan, np.abs(np.diff(r.reshape(len(r), -1), axis=0)).mean(1)]
    return ph, amp, spd


def corr(a, b):
    ok = np.isfinite(a) & np.isfinite(b)
    return float(np.corrcoef(a[ok], b[ok])[0, 1]) if ok.sum() > 5 and np.std(a[ok]) > 0 and np.std(b[ok]) > 0 else np.nan


def analyze(st):
    r, pc = st["rec"], st["pc"]
    ph, amp, spd = csi_series(st["csi"])
    inside = (pc >= r["t0"]) & (pc <= r["t1"])
    x = np.interp(pc, st["g_pc"], st["g_xy"][:, 0])
    y = np.interp(pc, st["g_pc"], st["g_xy"][:, 1])
    gs = np.r_[np.nan, np.hypot(np.diff(x), np.diff(y)) / np.maximum(np.diff(pc), 1e-3)]  # °/s
    m = inside
    row = dict(t0=r["t0"], dur=r["t1"] - r["t0"], shape=r["shape"], n_in=int(m.sum()),
               spd_in=float(np.nanmedian(spd[m])), spd_pad=float(np.nanmedian(spd[~m])) if (~m).sum() > 3 else np.nan,
               r_speed=corr(spd[m], gs[m]))
    for i, k in enumerate(range(1, ph.shape[1] + 1)):
        row[f"r_ph0{k}_x"], row[f"r_ph0{k}_y"] = corr(ph[m, i], x[m]), corr(ph[m, i], y[m])
    for c in range(amp.shape[1]):
        row[f"r_amp{c}_x"], row[f"r_amp{c}_y"] = corr(amp[m, c], x[m]), corr(amp[m, c], y[m])
    # quỹ đạo từ CSI: hồi quy tuyến tính x, y ~ [pha 0-1, 0-2, biên độ 3 chain] trong nét (in-sample, 5 tham số → lạc quan)
    a = np.c_[ph[m] - ph[m].mean(0), amp[m] - amp[m].mean(0), np.ones(m.sum())]
    for name, v in (("x", x[m]), ("y", y[m])):
        if m.sum() > 12:
            coef, *_ = np.linalg.lstsq(a, v, rcond=None)
            row[f"r2_{name}"] = float(1 - np.var(v - a @ coef) / (np.var(v) + 1e-9))
    return row, dict(pc=pc, ph=ph, amp=amp, spd=spd, x=x, y=y, gs=gs, inside=inside)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("jsonl", nargs="?", default=str(ROOT / "data/strokes/20261028.jsonl"))
    ap.add_argument("--out", default=str(ROOT / "docs/strokes_csi"))
    ap.add_argument("--router", default=None, help="chỉ nét của router này (vd. 192.168.5.1)")
    a = ap.parse_args()
    strokes = load(a.jsonl, a.router)
    rows, series = zip(*[analyze(s) for s in strokes])
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    import pandas as pd
    df = pd.DataFrame(rows)
    df.to_csv(out / "strokes.csv", index=False)
    print(df.round(2).to_string())
    print("\nTrung vị |r| qua các nét:")
    for c in [c for c in df.columns if c.startswith("r_")]:
        print(f"  {c:12s} {df[c].abs().median():.2f}")
    print(f"  R² hồi quy x {df.get('r2_x', pd.Series()).median():.2f}, y {df.get('r2_y', pd.Series()).median():.2f} (in-sample)")
    print(f"Tốc độ CSI trong nét / đệm (trung vị theo nét): {(df.spd_in / df.spd_pad).median():.2f}×, "
          f"nét có trong > đệm: {(df.spd_in > df.spd_pad).sum()}/{df.spd_pad.notna().sum()}")
    # null: ghép CSI của nét i với gyro của nét khác (cùng độ dài cắt ngắn) → |r| ngẫu nhiên khi không liên quan
    rng = np.random.default_rng(0)
    null = []
    for _ in range(200):
        i, j = rng.choice(len(series), 2, replace=False)
        si, sj = series[i], series[j]
        a_, b_ = si["ph"][si["inside"], 0], sj["x"][sj["inside"]]
        n = min(len(a_), len(b_))
        if n > 10:
            null.append(abs(corr(a_[:n], b_[:n])))
    print(f"Mốc ngẫu nhiên |r| pha0-1 ~ x khi ghép sai nét: trung vị {np.median(null):.2f}, p90 {np.percentile(null, 90):.2f}")

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    pick = [i for i, r in enumerate(rows) if r["n_in"] > 30][:4]
    fig, axes = plt.subplots(len(pick), 3, figsize=(13, 2.6 * len(pick)), squeeze=False)
    for ax, i in zip(axes, pick):
        s = series[i]
        t = s["pc"] - rows[i]["t0"]
        z = lambda v: (v - np.nanmean(v)) / (np.nanstd(v) + 1e-9)  # noqa: E731
        ax[0].plot(t, z(s["x"]), "k", label="gyro x"); ax[0].plot(t, z(s["ph"][:, 0]), label="pha 0-1")
        if s["ph"].shape[1] > 1:
            ax[0].plot(t, z(s["ph"][:, 1]), label="pha 0-2")
        ax[1].plot(t, z(s["y"]), "k", label="gyro y")
        for c in range(s["amp"].shape[1]):
            ax[1].plot(t, z(s["amp"][:, c]), label=f"biên độ c{c}")
        ax[2].plot(t, z(s["gs"]), "k", label="tốc độ gyro"); ax[2].plot(t, z(s["spd"]), label="tốc độ CSI")
        for a2 in ax:
            a2.axvspan(0, rows[i]["dur"], color="0.9", zorder=-1)
            a2.legend(fontsize=6, loc="upper right")
        ax[0].set_ylabel(f"nét {i} ({rows[i]['shape']})", fontsize=8)
    axes[-1][0].set_xlabel("giây từ lúc chạm (nền xám = đang giữ)")
    fig.tight_layout()
    fig.savefig(out / "csi_vs_gyro.png", dpi=80)
    print("→", out)


if __name__ == "__main__":
    main()
