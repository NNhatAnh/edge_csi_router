"""Vẽ trên không bằng hướng iPhone (kiểu bút laser) + nhận diện hình bằng $1 recognizer (Wobbrock 2007).

Mẫu từ trang viz/phone.html: t (ms), o = [alpha, beta, gamma] của DeviceOrientationEvent — hướng iOS đã hợp nhất từ gyro +
gia tốc kế (không tự tích phân rotationRate: trôi, và gia tốc tay lẫn vào "trọng lực" làm méo nét). Theo đặc tả W3C,
R = Rz(alpha)·Rx(beta)·Ry(gamma) đổi trục máy → trục Trái Đất (x đông, y bắc, z lên). Hướng "bút": máy nằm ngang → đỉnh
máy (+y); máy dựng đứng → camera sau (−z). Con trỏ = (−Δyaw, Δpitch) độ so với lúc bắt đầu nét.
    .venv/bin/python viz/draw.py   # self-check
"""
import math

import numpy as np

N = 64
SIZE = 250.0
SMOOTH = 6  # mẫu (~60 Hz)
MIN_SCORE = 0.70  # 31 nét thật 12:03–12:10 (sau khi dùng deviceorientation + cắt ở chỗ khép): 0.70 → 29 đúng, 1 sai, 1 unknown; 0.80 → 21 đúng, 10 unknown


def rotation(alpha, beta, gamma):
    a, b, g = np.radians([alpha, beta, gamma])
    rz = np.array([[math.cos(a), -math.sin(a), 0], [math.sin(a), math.cos(a), 0], [0, 0, 1]])
    rx = np.array([[1, 0, 0], [0, math.cos(b), -math.sin(b)], [0, math.sin(b), math.cos(b)]])
    ry = np.array([[math.cos(g), 0, math.sin(g)], [0, 1, 0], [-math.sin(g), 0, math.cos(g)]])
    return rz @ rx @ ry


def integrate(samples):
    """Danh sách mẫu → điểm (x, y) độ, bắt đầu ở (0, 0)."""
    if not samples:
        return []
    r0 = rotation(*samples[0]["o"])
    axis = np.array([0, 1.0, 0]) if abs(r0[2, 2]) > 0.7 else np.array([0, 0, -1.0])  # mặt máy hướng lên/xuống = nằm ngang
    v = np.array([rotation(*s["o"]) @ axis for s in samples])
    yaw = np.degrees(np.unwrap(np.arctan2(v[:, 1], v[:, 0])))
    pitch = np.degrees(np.arcsin(np.clip(v[:, 2], -1, 1)))
    xy = np.stack([-(yaw - yaw[0]), pitch - pitch[0]], 1)
    k = min(SMOOTH, len(xy))  # trung bình trượt ~100 ms: bỏ rung tay, nét trễ thêm ~50 ms
    xy = np.stack([np.convolve(np.pad(c, (k // 2, k - 1 - k // 2), "edge"), np.ones(k) / k, "valid") for c in xy.T], 1)
    return [tuple(r) for r in xy.tolist()]


def stroke(samples, skip_ms=100):
    """Nét cuối khi thả tay: bỏ 100 ms đầu (rung lúc chạm), rồi cắt ở điểm nét khép về gần điểm đầu (hình kín) — bỏ phần
    thừa sau khi vẽ xong và lúc nhấc ngón tay."""
    s = [x for x in samples if x["t"] >= samples[0]["t"] + skip_ms] if samples else []
    p = np.asarray(integrate(s), float)
    if len(p) < 10:
        return p.tolist()
    length = np.r_[0, np.cumsum(np.hypot(*np.diff(p, axis=0).T))]
    cand = np.where(length >= 0.6 * length[-1])[0]
    d = np.hypot(*(p[cand] - p[0]).T)
    i = cand[d.argmin()]
    return (p[:i + 1] if d.min() < 0.25 * np.ptp(p, axis=0).max() else p).tolist()


# --- $1 recognizer -------------------------------------------------------------------------------------------
def _resample(p, n=N):
    p = np.asarray(p, float)
    d = np.r_[0, np.cumsum(np.hypot(*np.diff(p, axis=0).T))]
    if d[-1] == 0:
        return np.repeat(p[:1], n, 0)
    s = np.linspace(0, d[-1], n)
    return np.stack([np.interp(s, d, p[:, 0]), np.interp(s, d, p[:, 1])], 1)


def _rotate(p, th):
    c, s = math.cos(th), math.sin(th)
    return p @ np.array([[c, s], [-s, c]])


def _normalize(p):
    p = _resample(p)
    p = p - p.mean(0)
    p = _rotate(p, -math.atan2(p[0, 1], p[0, 0]))       # góc chỉ thị → 0
    span = np.ptp(p, axis=0)
    p = p / np.maximum(span, 1e-9) * SIZE
    return p - p.mean(0)


def _dist_at(p, t, th):
    return np.hypot(*(_rotate(p, th) - t).T).mean()


def _best(p, t, lo=-math.radians(45), hi=math.radians(45), eps=math.radians(2)):
    phi = 0.5 * (math.sqrt(5) - 1)
    x1, x2 = phi * lo + (1 - phi) * hi, (1 - phi) * lo + phi * hi
    f1, f2 = _dist_at(p, t, x1), _dist_at(p, t, x2)
    while abs(hi - lo) > eps:
        if f1 < f2:
            hi, x2, f2 = x2, x1, f1
            x1 = phi * lo + (1 - phi) * hi
            f1 = _dist_at(p, t, x1)
        else:
            lo, x1, f1 = x1, x2, f2
            x2 = (1 - phi) * lo + phi * hi
            f2 = _dist_at(p, t, x2)
    return min(f1, f2)


def _polygon(k, start, cw, m=60):
    v = [(math.cos(a), math.sin(a)) for a in math.pi / 2 + (np.arange(k + 1) + start) * 2 * math.pi / k * (-1 if cw else 1)]
    return [(x0 + (x1 - x0) * u, y0 + (y1 - y0) * u) for (x0, y0), (x1, y1) in zip(v, v[1:]) for u in np.linspace(0, 1, m // k, endpoint=False)]


def _templates():
    t = []
    for cw in (False, True):
        a = np.linspace(0, 2 * math.pi, 60) * (-1 if cw else 1)
        t.append(("circle", _normalize(np.stack([np.cos(a), np.sin(a)], 1))))
        for s in range(3):  # $1 phụ thuộc điểm bắt đầu và chiều vẽ → 1 mẫu cho mỗi đỉnh/chiều
            t.append(("triangle", _normalize(_polygon(3, s, cw))))
    return t


TEMPLATES = _templates()


def recognize(points):
    """→ (hình, điểm 0–1, {hình: điểm tốt nhất}). Nét quá ngắn → ("unknown", 0, {})."""
    p = np.asarray(points, float)
    if len(p) < 8 or np.ptp(p, axis=0).max() < 5:  # < 5° xoay: chưa vẽ gì
        return "unknown", 0.0, {}
    q = _normalize(p)
    half = 0.5 * math.hypot(SIZE, SIZE)
    scores = {}
    for name, t in TEMPLATES:
        scores[name] = max(scores.get(name, 0), 1 - _best(q, t) / half)
    name = max(scores, key=scores.get)
    return (name if scores[name] >= MIN_SCORE else "unknown"), round(scores[name], 3), {k: round(v, 3) for k, v in scores.items()}


if __name__ == "__main__":
    rng = np.random.default_rng(0)
    a = np.linspace(0, 2 * math.pi, 80)
    for name, pts in [("circle", np.stack([30 * np.cos(a), 20 * np.sin(a)], 1)),       # elip, như vẽ tay
                      ("triangle", np.array(_polygon(3, 1, True, 90)) * [25, 30])]:
        for k in range(5):
            noisy = _rotate(pts, rng.uniform(-0.4, 0.4)) + rng.normal(0, 1.2, pts.shape)
            got = recognize(noisy)
            assert got[0] == name, (name, got)
    assert recognize([(0, 0), (1, 1)] * 5)[0] == "unknown"
    # máy nằm ngang, đỉnh chỉ về bắc; quay trái 30° (alpha +30) → con trỏ sang trái 30°
    x, y = integrate([dict(t=0, o=[0, 0, 0])] + [dict(t=16 * i, o=[30, 0, 0]) for i in range(1, 11)])[-1]
    assert abs(x + 30) < 0.5 and abs(y) < 0.5, (x, y)
    # nằm ngang, ngóc đỉnh lên 20° (beta +20) → con trỏ lên 20°
    x, y = integrate([dict(t=0, o=[0, 0, 0])] + [dict(t=16 * i, o=[0, 20, 0]) for i in range(1, 11)])[-1]
    assert abs(x) < 0.5 and abs(y - 20) < 0.5, (x, y)
    # dựng đứng (beta 90, màn hình về phía người, camera về phía router); quay phải 25° (alpha −25) → con trỏ sang phải
    x, y = integrate([dict(t=0, o=[0, 90, 0])] + [dict(t=16 * i, o=[-25, 90, 0]) for i in range(1, 11)])[-1]
    assert abs(x - 25) < 0.5 and abs(y) < 0.5, (x, y)
    # dựng đứng rồi ngửa màn hình lên trời (beta 90 → 75): camera sau chỉ xuống → con trỏ xuống 15°
    x, y = integrate([dict(t=0, o=[0, 90, 0])] + [dict(t=16 * i, o=[0, 75, 0]) for i in range(1, 11)])[-1]
    assert abs(x) < 0.5 and abs(y + 15) < 0.5, (x, y)
    # vòng tròn + đuôi thừa lúc thả tay → cắt ở chỗ khép, nhận circle
    ang = np.linspace(0, 2 * math.pi, 100)
    s = [dict(t=i * 16.7, o=[20 * math.cos(u), 15 * math.sin(u), 0]) for i, u in enumerate(ang)]
    s += [dict(t=(100 + i) * 16.7, o=[20 - i * 1.5, -i * 1.0, 0]) for i in range(20)]  # đuôi
    p = stroke(s)
    assert len(p) < 110 and recognize(p)[0] == "circle", (len(p), recognize(p))
    print("PASS draw")
