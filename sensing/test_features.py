"""Self-check features.py: .venv/bin/python sensing/test_features.py [recording dir]"""
import ast, sys
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).parent))
import features as F

ROOT = Path(__file__).resolve().parent.parent


def notebook_amplitude_features():
    """Lấy đúng hàm amplitude_features từ notebook, chạy độc lập."""
    tree = ast.parse((ROOT / "data80mhz_full_workflow_colab.py").read_text())
    fn = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "amplitude_features")
    ns = dict(np=np, pd=pd, EPS=1e-09, AMP_CATALOG={})
    exec(compile(ast.Module([fn], []), "notebook", "exec"), ns)
    return ns["amplitude_features"]


# 1. công thức: tín hiệu hằng không có dynamic; 84 feature
static = np.ones((64, 3, 208)) * 100
meta = pd.DataFrame({**{f"agc_ch{c}": [50] * 64 for c in range(3)}, "wmi_cfo_raw": [0] * 64,
                     "wmi_rssi_dbm_ch0": [-50] * 64, "wmi_rssi_dbm_ch1": [-55] * 64,
                     "kernel_ts_s": np.arange(64) * 0.05, "chip_tsf_us": np.arange(64) * 50000})
f = F.amplitude_features(static, meta)
assert f["dynamic_relative_delta_c0"] == 0 and f["dynamic_cv_median_c0"] == 0
assert len(F.AMP_FEATURES) == 84

# 2. segments: bỏ 1-2 record (gap ≤ 160 ms) KHÔNG cắt; gap > 160 ms hoặc TSF lùi thì cắt
r = pd.DataFrame({"chip_tsf_us": [0, 51000, 102000, 153000, 400000, 450000, 440000]})
keep = np.array([1, 0, 1, 1, 1, 1, 1], bool)
assert [list(x) for x in F.segments(r, keep)] == [[0, 2, 3], [4, 5], [6]]

# 3. parity với notebook trên dữ liệu thật
runs = [Path(sys.argv[1])] if len(sys.argv) > 1 else sorted(
    p.parent for p in (ROOT / "data/raw").glob("*_11-1_*/label.json") if '"bw": 2' in p.read_text())
if runs:
    records, a = F.load_recording(runs[-1])
    use = F.segments(records, F.qc_keep(records))[0][:64]
    if len(use) == 64:
        ours = F.amplitude_features(a[use], records.iloc[use])
        theirs = notebook_amplitude_features()(a[use], records.iloc[use])
        assert ours.keys() == theirs.keys()
        bad = [k for k in ours if not (ours[k] == theirs[k] or (np.isnan(ours[k]) and np.isnan(theirs[k])))]
        assert not bad, bad
        print(f"parity với notebook: {len(ours)} feature giống hệt trên {runs[-1].name}")
    w, qc = F.recording_windows(runs[-1])
    print("qc:", qc)
print("PASS features.py")

# 4. motion: tín hiệu hằng → 0 chuyển động; điều biến 2 Hz lấy mẫu KHÔNG đều (bỏ record) → đỉnh ~2 Hz, dải wave
rng = np.random.default_rng(0)
t = np.cumsum(rng.choice([0.0512, 0.1024], size=90, p=[0.8, 0.2]))
t -= t[0]
t = t[t < F.MOTION_WINDOW_S]
base = rng.uniform(50, 150, (3, 208))
m0 = F.motion_features(np.broadcast_to(base, (len(t), 3, 208)), t)
assert m0["motion_std_c0"] < 1e-12 and m0["motion_step_c0"] < 1e-12
tone_gain = np.sin(np.linspace(0, 6, 208))  # mỗi tone dao động khác pha/biên → hình dạng kênh thay đổi
wave = base * (1 + 0.2 * np.sin(2 * np.pi * 2.0 * t)[:, None, None] * tone_gain)
m = F.motion_features(wave * rng.uniform(0.5, 2, (len(t), 1, 1)), t)  # nhân thang AGC ngẫu nhiên mỗi record
assert abs(m["motion_pc1_peak_hz"] - 2.0) < 0.4, m["motion_pc1_peak_hz"]
assert m["motion_pc1_wave_fraction"] > 0.8, m["motion_pc1_wave_fraction"]
assert m["motion_std_c0"] > 0.05
print(f"motion: {len(F.MOTION_FEATURES)} feature; 2 Hz lấy mẫu không đều + AGC ngẫu nhiên → đỉnh {m['motion_pc1_peak_hz']:.2f} Hz")
print("PASS motion")

# 5. dữ liệu 4.1 (RDP488): 5 chain, không có wmi_* → QC theo rx_bw
r41 = sorted((ROOT / "data/raw").glob("*_4-1_*/label.json"))
if r41:
    rec41, a41 = F.load_recording(r41[-1].parent)
    assert a41.shape[1:] == (5, 208), a41.shape
    assert rec41.wmi_bw.isna().all() and F.bw80(rec41).mean() > 0.9
    w5 = F.motion_windows(r41[-1].parent, chains=None)
    w3, qc41 = F.recording_windows(r41[-1].parent)
    assert set(F.motion_feature_names(5)) <= set(w5.columns) if len(w5) else True
    print(f"4.1: {a41.shape[1]} chain, bw80 {F.bw80(rec41).mean():.0%}, kept {F.qc_keep(rec41).sum()}/{len(rec41)}, "
          f"cửa sổ motion(5 chain) {len(w5)}, amp84(3 chain) {qc41['windows']}")
    print("PASS 4.1")
