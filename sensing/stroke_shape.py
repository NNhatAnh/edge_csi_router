"""Tròn vs tam giác theo nét: CSI 11.1 so với chuẩn gyro (dữ liệu của viz/server.py --draw-port, nhãn = intent).

    .venv/bin/python sensing/stroke_shape.py [data/strokes/20261028.jsonl] [--since HH:MM]

Mỗi nét → chuỗi trong nét (gyro x/y, hoặc pha CSI-ratio 0-1/0-2), nội suy đều 48 điểm, làm mượt, chuẩn hoá z (bỏ thời lượng
và biên độ) → so khớp nguyên đường cong bằng DTW, 3 nét gần nhất bỏ phiếu (leave-one-stroke-out). Feature tay (vận tốc, FFT)
đã thử và kém ngay cả trên gyro (2026-10-28) → bỏ. Kiểm lẫn: chỉ thời lượng, chỉ CSI/s, biên độ CSI trước khi chạm (logistic).
viz/server.py dùng `csi_query` + `knn` để đoán hình CHỈ từ CSI cho mỗi nét live.
"""
import json, sys
from pathlib import Path

import numpy as np
from sklearn.linear_model import LogisticRegression
from sklearn.pipeline import make_pipeline
from sklearn.preprocessing import StandardScaler

sys.path.insert(0, str(Path(__file__).parent))
import stroke_csi as SC  # noqa: E402

N = 48
K = 3


def prep(v):
    g = np.interp(np.linspace(0, 1, N), np.linspace(0, 1, len(v)), v)
    g = np.convolve(np.pad(g, 2, "edge"), np.ones(5) / 5, "valid")
    return (g - g.mean()) / (g.std() + 1e-9)


def dtw(a, b, w=8):
    """a, b: N x d. Cửa sổ Sakoe-Chiba ±w."""
    n = len(a)
    d = np.full((n + 1, n + 1), np.inf)
    d[0, 0] = 0
    cost = ((a[:, None, :] - b[None, :, :]) ** 2).sum(2)
    for i in range(1, n + 1):
        for j in range(max(1, i - w), min(n, i + w) + 1):
            d[i, j] = cost[i - 1, j - 1] + min(d[i - 1, j], d[i, j - 1], d[i - 1, j - 1])
    return d[n, n]


def knn(templates, q, k=K):
    """templates: [(nhãn, N x d)] → (nhãn đa số, {nhãn: số phiếu})."""
    near = sorted(templates, key=lambda t: dtw(q, t[1]))[:k]
    votes = {}
    for lab, _ in near:
        votes[lab] = votes.get(lab, 0) + 1
    return max(votes, key=votes.get), votes


def csi_query(csi, pc, t0, t1):
    """CSI (n x ≥3 x 208, phức) + giờ PC từng bản ghi → chuỗi N x 1 của pha 0-1 trong [t0, t1]; None nếu quá ít bản ghi."""
    m = (pc >= t0) & (pc <= t1)
    if m.sum() < 15:
        return None
    ph, _, _ = SC.csi_series(csi[m])
    return prep(ph[:, 0])[:, None]


def loso(x, y):
    p = np.zeros(len(y))
    for i in range(len(y)):
        m = np.arange(len(y)) != i
        clf = make_pipeline(StandardScaler(), LogisticRegression(C=0.5, max_iter=1000)).fit(x[m], y[m])
        p[i] = clf.predict_proba(x[i:i + 1])[0, 1]
    return float(((p > 0.5) == y).mean())


def loso_knn(series, labels):
    ok = 0
    for i in range(len(labels)):
        tpl = [(labels[j], series[j]) for j in range(len(labels)) if j != i]
        ok += knn(tpl, series[i])[0] == labels[i]
    return ok / len(labels)


def main():
    import argparse, time
    ap = argparse.ArgumentParser()
    ap.add_argument("jsonl", nargs="?", default=str(SC.ROOT / "data/strokes/20261028.jsonl"))
    ap.add_argument("--since", default="00:00", help="chỉ nét bắt đầu từ HH:MM (giờ máy)")
    a = ap.parse_args()
    since = tuple(int(v) for v in a.since.split(":"))
    strokes = [s for s in SC.load(a.jsonl) if s["rec"].get("intent") in ("circle", "triangle") and s["rec"]["t1"] - s["rec"]["t0"] >= 0.8
               and time.localtime(s["rec"]["t0"])[3:5] >= since]
    labels = [s["rec"]["intent"] for s in strokes]
    y = np.array([lab == "triangle" for lab in labels])
    src = {k: [] for k in ("gyro x", "gyro y", "gyro x,y", "CSI pha 0-1", "CSI pha 0-2", "CSI pha 0-1,0-2")}
    ctx, track = [], []
    for s in strokes:
        row, se = SC.analyze(s)
        m = se["inside"]
        gx, gy, p1, p2 = prep(se["x"][m]), prep(se["y"][m]), prep(se["ph"][m, 0]), prep(se["ph"][m, 1])
        for k, v in (("gyro x", [gx]), ("gyro y", [gy]), ("gyro x,y", [gx, gy]), ("CSI pha 0-1", [p1]),
                     ("CSI pha 0-2", [p2]), ("CSI pha 0-1,0-2", [p1, p2])):
            src[k].append(np.stack(v, 1))
        pre = se["pc"] < s["rec"]["t0"]
        amp = np.abs(s["csi"][pre]).mean((0, 2)) if pre.sum() else np.abs(s["csi"]).mean((0, 2))
        ctx.append(list(np.log(amp / amp.mean())) + [np.log(amp.mean())])
        track.append((s["rec"]["intent"], row["r_ph01_x"]))
    print(f"{len(strokes)} nét: {int((~y).sum())} tròn, {int(y.sum())} tam giác (bỏ nét < 0.8 s)")
    for lab in ("circle", "triangle"):
        r = np.array([v for k, v in track if k == lab])
        print(f"  {lab:8s} r(pha 0-1, gyro x): trung vị {np.nanmedian(r):.2f}, |r|>0.7: {(np.abs(r) > 0.7).sum()}/{len(r)}, âm {(r < 0).sum()}")
    print(f"\nDTW {K}-NN bỏ-ra-1-nét (tỉ lệ đúng; đoán lớp đông hơn = {max(y.mean(), 1 - y.mean()):.2f}):")
    for k, v in src.items():
        print(f"  {k:28s} {loso_knn(v, labels):.2f}")
    dur = np.array([s["rec"]["t1"] - s["rec"]["t0"] for s in strokes])
    rate = np.array([s["rec"]["csi_rate"] for s in strokes])
    print("Kiểm lẫn (logistic):")
    print(f"  {'chỉ thời lượng':28s} {loso(dur[:, None], y):.2f}   trung vị tròn {np.median(dur[~y]):.1f} s / tam giác {np.median(dur[y]):.1f} s")
    print(f"  {'chỉ CSI/s':28s} {loso(rate[:, None], y):.2f}   trung vị {np.median(rate[~y]):.0f} / {np.median(rate[y]):.0f}")
    print(f"  {'biên độ CSI trước khi chạm':28s} {loso(np.array(ctx), y):.2f}")


if __name__ == "__main__":
    main()
