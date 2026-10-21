"""So sánh bộ feature bằng leave-one-recording-out (LORO), cùng RF như train.py.

    .venv/bin/python sensing/compare_features.py [data/raw] [--group recording_id|position|session|person]

--group position: bỏ ra cả một vị trí mỗi vòng (kiểm tra tổng quát hoá sang vị trí mới) — cần ≥ 2 vị trí.
Mỗi vòng train trên các nhóm còn lại, test nhóm bị bỏ ra; báo % lượt đúng (trung bình P(wave) > 0.5)
và % cửa sổ đúng. Với ít lượt, con số chỉ là chỉ báo, không phải độ chính xác thật.
"""
import argparse, json, sys, warnings
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).parent))
import features as F
import train as T

warnings.filterwarnings("ignore")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data", nargs="?", default="data/raw")
    ap.add_argument("--group", default="recording_id")
    ap.add_argument("--bw", type=int, default=2)
    ap.add_argument("--board", default="192.168.11.1")
    ap.add_argument("--chains", type=int, default=3, help="motion: số chain đầu (4.1 có 5)")
    a = ap.parse_args()
    folders = T.select_folders(a.data, a.bw, a.board)
    amp = pd.concat([F.recording_windows(f)[0] for f in folders], ignore_index=True)
    mot = pd.concat([F.motion_windows(f, a.chains) for f in folders], ignore_index=True)
    for w in (amp, mot):
        w["y"] = np.where(w.label == "wave", "wave", "no_wave")
    fam = lambda k: F.AMP_CATALOG[k]["family"]
    sets = [("amp84 (notebook, 64 rec không chồng)", amp, F.AMP_FEATURES),
            ("amp level+shape (tĩnh)", amp, [k for k in F.AMP_FEATURES if fam(k) in ("level", "shape")]),
            ("amp dynamic+spectral", amp, [k for k in F.AMP_FEATURES if fam(k) in ("dynamic", "spectral")]),
            (f"motion (TSF, 3.2 s trượt 0.8 s, {a.chains} chain)", mot, F.motion_feature_names(a.chains)),
            (f"motion core ({a.chains} chain)", mot, F.motion_core_names(a.chains))]
    print(f"{len(folders)} lượt, nhóm LORO = {a.group}: "
          f"{amp.groupby('y').recording_id.nunique().to_dict()}\n")
    print(f"{'bộ feature':40s} {'số f':>4s} {'cửa sổ':>7s} {'lượt đúng':>10s} {'cửa sổ đúng':>11s}")
    for name, w, cols in sets:
        ev = T.loro(w, cols, a.group)
        print(f"{name:40s} {len(cols):4d} {len(w):7d} {ev['recording_correct']:>6d}/{ev['recordings']:<3d} "
              f"{ev['window_accuracy']:11.0%}   P(wave) theo lượt: "
              + " ".join(f"{r[0][9:15]}{'W' if r[1] == 'wave' else 'N'}={r[2]:.2f}" for r in ev["per_recording"]))


if __name__ == "__main__":
    main()
