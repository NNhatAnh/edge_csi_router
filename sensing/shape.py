"""Nhận diện hình vẽ bằng điện thoại cầm tay (circle / triangle / none) trên 11.1, kiểu Widar3.0 / CARM.

    .venv/bin/python sensing/shape.py [data/raw] [models] [--group recording_id|person|session] [--period 20]

1 link → không có BVP (cần ≥ 6 máy thu) → dùng phổ Doppler của 1 link + profile tốc độ / pha CSI-ratio.
Mọi feature bất biến với điểm bắt đầu cửa sổ (pha cửa sổ so với hình vẽ là ngẫu nhiên):
  dfs_b*        phổ Doppler (Widar): PCA thành phần 1 của CSI-ratio đã bỏ phần tĩnh → STFT → công suất theo |f|,
                trung bình theo thời gian (8 dải 0–16 Hz), chuẩn hoá tổng = 1
  spd_f*, spd_cv  "tốc độ" s(t) = |Δ CSI| giữa 2 mẫu; tam giác khựng ở 3 góc → s(t) dao động ở 3× tần số vẽ,
                tròn đi đều → s(t) phẳng. FFT |s| 0.3–4 Hz (chuẩn hoá) + hệ số biến thiên
  ph{k}_f*      pha CSI-ratio H0·conj(Hk) (đổi theo góc tới khi điện thoại di chuyển): FFT |pha| 0.3–4 Hz
Xuất models/<ts>_shape/{shape_rf.pkl, report.json, windows.csv}; viz/server.py nạp shape_rf.pkl để chạy live.
"""
import argparse, json, pickle, sys, time
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.metrics import confusion_matrix

sys.path.insert(0, str(Path(__file__).parent))
import features as F
import train as T

CLASSES = ["circle", "none", "triangle"]
FS = 40.0          # lưới nội suy; 20 ms ≈ 30–50 CSI/s thực tế trên 11.1 + iPhone
WIN_S = F.MOTION_WINDOW_S
SLOW = (0.3, 4.0)  # dải nhịp vẽ (1 hình 0.5–3 s) và nhịp góc tam giác


def _grid(x, t_s, t0):
    g = t0 + np.arange(int(round(WIN_S * FS))) / FS
    return np.stack([np.interp(g, t_s, x[:, j]) for j in range(x.shape[1])], 1)


def _slow_spectrum(x):
    """|FFT| của chuỗi 1-D đã khử trung bình, các bin 0.3–4 Hz, chuẩn hoá tổng = 1 (bất biến biên độ)."""
    x = x - x.mean()
    p = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    f = np.fft.rfftfreq(len(x), 1 / FS)
    p = p[(f >= SLOW[0]) & (f <= SLOW[1])]
    return p / (p.sum() + F.EPS)


def shape_features(h, t_s, t0=None):
    """h: n x chain(≥3) x 208 CSI phức của 1 cửa sổ, t_s: giây (không đều). Trả dict feature."""
    t0 = t_s[0] if t0 is None else t0
    out = {}
    r = np.concatenate([h[:, 0] * np.conj(h[:, k]) for k in (1, 2)], 1)          # n x 416, khử CFO/STO
    r = r / (np.abs(r).mean(1, keepdims=True) + F.EPS)                             # bỏ thang AGC theo record
    g = _grid(r.real, t_s, t0) + 1j * _grid(r.imag, t_s, t0)
    d = g - g.mean(0)                                                              # bỏ phần tĩnh
    u, s, _ = np.linalg.svd(d, full_matrices=False)
    pc = u[:, 0] * s[0]
    # Widar DFS: STFT 0.8 s, bước 0.2 s, công suất theo |f| trung bình theo thời gian
    n, hop = int(0.8 * FS), int(0.2 * FS)
    frames = np.stack([pc[i:i + n] * np.hanning(n) for i in range(0, len(pc) - n + 1, hop)])
    spec = np.abs(np.fft.fft(frames, axis=1)) ** 2
    f = np.abs(np.fft.fftfreq(n, 1 / FS))
    edges = np.linspace(0, 16, 9)
    band = np.array([spec[:, (f >= a) & (f < b)].sum() for a, b in zip(edges[:-1], edges[1:])])
    for i, v in enumerate(band / (band.sum() + F.EPS)):
        out[f"dfs_b{i}"] = v
    spd = np.abs(np.diff(g, axis=0)).mean(1)
    out["spd_cv"] = spd.std() / (spd.mean() + F.EPS)
    for i, v in enumerate(_slow_spectrum(spd)):
        out[f"spd_f{i}"] = v
    for k, c in ((1, slice(0, 208)), (2, slice(208, 416))):
        ph = np.unwrap(np.angle(g[:, c].sum(1)))
        ph = ph - np.polyval(np.polyfit(np.arange(len(ph)), ph, 1), np.arange(len(ph)))
        for i, v in enumerate(_slow_spectrum(ph)):
            out[f"ph{k}_f{i}"] = v
    return out


def shape_names():
    return list(shape_features(np.exp(1j * np.random.default_rng(0).random((80, 3, 208))), np.arange(80) / 25))


def shape_windows(folder):
    return F._time_windows(folder, 3, shape_features, True)


def predict(model, h, t_s, t0):
    """model = dict từ shape_rf.pkl; trả {lớp: xác suất} cho 1 cửa sổ (impute như preprocess của train.py)."""
    f = shape_features(h, t_s, t0)
    p = model["prep"]
    x = np.array([f[c] for c in model["cols"]], float)
    x = np.where(np.isfinite(x), x, p["train_imputation_medians"])
    x = x[[model["cols"].index(c) for c in p["kept_columns"]]]
    return dict(zip(model["rf"].classes_, model["rf"].predict_proba(x[None])[0].tolist()))


def loro(w, cols, group):
    per, preds, truths = [], [], []
    for gname in sorted(w[group].unique()):
        tr, te = w[w[group] != gname], w[w[group] == gname]
        if tr.y.nunique() < 2:
            continue
        xf, xt, _ = T.preprocess(tr, te, cols)
        rf = T.make_rf().fit(xf, tr.y, sample_weight=T.weights(tr))
        p = pd.DataFrame(rf.predict_proba(xt), columns=rf.classes_, index=te.index)
        preds += list(p.idxmax(1))
        truths += list(te.y)
        for rid, pr in p.groupby(te.recording_id):
            per.append((rid, te.y[te.recording_id == rid].iloc[0], pr.mean().idxmax(), pr.mean().round(3).to_dict()))
    ok = sum(t == p for _, t, p, _ in per)
    return dict(per_recording=per, recordings=len(per), recording_correct=ok,
                window_accuracy=float(np.mean(np.array(preds) == np.array(truths))) if preds else float("nan"),
                classes=CLASSES, window_confusion=confusion_matrix(truths, preds, labels=CLASSES).tolist() if preds else [])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data", nargs="?", default="data/raw")
    ap.add_argument("out", nargs="?", default="models")
    ap.add_argument("--group", default="recording_id")
    ap.add_argument("--period", type=int, default=20, help="chỉ dùng lượt có period_ms này (không trộn chu kỳ)")
    a = ap.parse_args()
    folders = [f for f in T.select_folders(a.data, 2, "192.168.11.1", labels=set(CLASSES))
               if json.loads((f / "label.json").read_text()).get("period_ms") == a.period]
    w = pd.concat([shape_windows(f) for f in folders], ignore_index=True) if folders else pd.DataFrame()
    if w.empty or w.label.nunique() < 2:
        sys.exit(f"cần ≥ 2 lớp trong {CLASSES} (11.1, BW=2, {a.period} ms); có {len(folders)} lượt")
    w["y"] = w.label
    cols = shape_names()
    print(w.groupby(["label", "recording_id"]).size().to_string())
    ev = loro(w, cols, a.group)
    xf, _, prep = T.preprocess(w, w, cols)
    rf = T.make_rf().fit(xf, w.y, sample_weight=T.weights(w))
    out = Path(a.out) / (time.strftime("%Y%m%d-%H%M%S") + "_shape")
    out.mkdir(parents=True)
    pickle.dump(dict(rf=rf, prep=prep, cols=cols, period_ms=a.period), (out / "shape_rf.pkl").open("wb"))
    w.to_csv(out / "windows.csv", index=False)
    rep = dict(recordings=[f.name for f in folders], windows=w.label.value_counts().to_dict(), group=a.group, loro=ev,
               top_features=sorted(zip(prep["kept_columns"], rf.feature_importances_.round(4).tolist()), key=lambda x: -x[1])[:12])
    (out / "report.json").write_text(json.dumps(rep, indent=1, ensure_ascii=False))
    print(f"LORO ({a.group}): {ev['recording_correct']}/{ev['recordings']} lượt, {ev['window_accuracy']:.0%} cửa sổ")
    for rid, t, p, pr in ev["per_recording"]:
        print(f"  {'OK ' if t == p else 'SAI'} {rid}: thật {t}, đoán {p} {pr}")
    print("→", out)


if __name__ == "__main__":
    main()
