"""Self-check shape.py: mô phỏng điện thoại vẽ tròn / tam giác (khựng ở góc) trước 3 anten → feature khác nhau,
RF tách được trên dữ liệu mô phỏng. Không phải bằng chứng trên dữ liệu thật.

    .venv/bin/python sensing/test_shape.py
"""
import sys
from pathlib import Path

import numpy as np
from sklearn.ensemble import RandomForestClassifier

sys.path.insert(0, str(Path(__file__).parent))
import shape as S

C = 3e8
FREQ = 5775e6 + (np.arange(208) - 104) * 312.5e3
ANT = np.array([[0, 0, 0], [0.20, 0, 0], [0.20, 0.07, 0]])
REFL = np.array([[1.5, 2.0, 0.5], [-1.0, 1.5, -0.3]])  # 2 vật phản xạ tĩnh


def path(kind, t, period, r=0.25, center=(0.1, 0.0, 1.0)):
    u = (t / period) % 1
    if kind == "circle":
        xy = np.stack([np.cos(2 * np.pi * u), np.sin(2 * np.pi * u)], 1) * r
    elif kind == "triangle":
        v = r * np.array([[np.cos(a), np.sin(a)] for a in np.pi / 2 + np.arange(4) * 2 * np.pi / 3])
        seg = np.minimum((u * 3).astype(int), 2)
        s = u * 3 - seg
        s = 0.5 - 0.5 * np.cos(np.pi * s)  # chậm lại ở mỗi góc
        xy = v[seg] + (v[seg + 1] - v[seg]) * s[:, None]
    else:
        xy = np.zeros((len(t), 2)) + 0.003 * np.sin(2 * np.pi * 0.3 * t)[:, None]  # cầm yên, rung nhẹ
    return np.array(center) + np.stack([xy[:, 0], np.zeros(len(t)), xy[:, 1]], 1)


def csi(pos, rng):
    h = np.zeros((len(pos), 3, 208), complex)
    for a, ant in enumerate(ANT):
        d = np.linalg.norm(pos - ant, axis=1)
        h[:, a] = np.exp(-2j * np.pi * FREQ * d[:, None] / C) / d[:, None]
        for rf in REFL:
            dr = np.linalg.norm(pos - rf, axis=1) + np.linalg.norm(rf - ant)
            h[:, a] += 0.4 * np.exp(-2j * np.pi * FREQ * dr[:, None] / C) / dr[:, None]
    cfo = np.exp(1j * rng.uniform(0, 2 * np.pi, len(pos)))[:, None, None]  # pha chung ngẫu nhiên mỗi record
    agc = rng.uniform(0.7, 1.3, (len(pos), 3, 1))
    return h * cfo * agc + 0.01 * (rng.normal(size=h.shape) + 1j * rng.normal(size=h.shape))


def windows(kind, rng, n=30):
    rows = []
    for _ in range(n):
        t = np.sort(rng.uniform(0, S.WIN_S, int(S.WIN_S * 30)))  # ~30 CSI/s, lấy mẫu không đều
        period = rng.uniform(1.5, 2.5)
        pos = path(kind, t + rng.uniform(0, period), period, r=rng.uniform(0.15, 0.3))
        rows.append(S.shape_features(csi(pos, rng), t, 0.0))
    return rows


rng = np.random.default_rng(1)
cols = S.shape_names()
data = {k: np.array([[r[c] for c in cols] for r in windows(k, rng)]) for k in S.CLASSES}
cv = {k: data[k][:, cols.index("spd_cv")].mean() for k in S.CLASSES}
print("spd_cv trung bình:", {k: round(v, 3) for k, v in cv.items()})
assert np.isfinite(np.concatenate(list(data.values()))).all()
assert cv["triangle"] > cv["circle"], "tam giác (khựng ở góc) phải có tốc độ biến thiên hơn tròn"

x = np.concatenate([data[k][:20] for k in S.CLASSES])
y = np.repeat(S.CLASSES, 20)
xt = np.concatenate([data[k][20:] for k in S.CLASSES])
yt = np.repeat(S.CLASSES, 10)
acc = (RandomForestClassifier(96, max_depth=4, random_state=0).fit(x, y).predict(xt) == yt).mean()
print(f"RF trên mô phỏng: {acc:.0%}")
assert acc >= 0.8
print("PASS")
