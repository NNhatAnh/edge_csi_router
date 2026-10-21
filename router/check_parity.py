"""So feature của daemon C (--replay relay.bin) với sensing/features.invariant_windows trên cùng một lượt.
    ../.venv/bin/python check_parity.py ../data/raw/<lượt>"""
import subprocess, sys
from io import StringIO
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "sensing"))
import features as F

run = Path(sys.argv[1])
c = pd.read_csv(StringIO(subprocess.run([str(Path(__file__).parent / "build/wifi_sensing_host"), "--replay",
                                         str(run / "relay.bin")], capture_output=True, text=True, check=True).stdout))
py = F.invariant_windows(run, chains=3)
if len(c) == len(py) == 0:
    sys.exit(f"{run.name}: cả C và Python đều 0 cửa sổ (lượt quá thưa) — không có gì để so")
cols = F.invariant_names(3)
assert list(c.columns[3:-1]) == cols, f"tên feature C khác Python: {list(c.columns[3:-1])}"
assert len(c) == len(py), f"số cửa sổ khác: C {len(c)} vs Python {len(py)}"
err = np.abs(c[cols].to_numpy() - py[cols].to_numpy()) / (np.abs(py[cols].to_numpy()) + 1e-6)
print(f"{run.name}: {len(c)} cửa sổ, sai lệch tương đối lớn nhất {err.max():.2e} ({cols[err.max(0).argmax()]})")
assert err.max() < 1e-4, err.max()
print("P(wave) theo cửa sổ (C):", c.p_wave.round(2).tolist())
print("PASS parity C/Python")

# Đường có nền (model inv_nb): nền giả = trung vị feature Python của chính lượt này → C đọc baseline.txt,
# tính nb = log(f/nền) + P(wave); so với model C (ctypes) chạy trên nb tính bằng Python.
import ctypes, re, tempfile
fmap_h = (Path(__file__).parent / "build/feature_map.h").read_text()
if "NEEDS_BASELINE 1" in fmap_h:
    fmap = [int(x) for x in re.search(r"\{([\d,]*)\}", fmap_h).group(1).split(",")]
    base = py[cols].median()
    with tempfile.TemporaryDirectory() as d:
        bf = Path(d) / "baseline.txt"
        bf.write_text("".join(f"{k} {v:.9g}\n" for k, v in base.items()))
        cb = pd.read_csv(StringIO(subprocess.run([str(Path(__file__).parent / "build/wifi_sensing_host"), "--replay",
                                                  str(run / "relay.bin"), "-b", str(bf)], capture_output=True, text=True, check=True).stdout))
        so = Path(d) / "m.so"
        subprocess.check_call(["cc", "-O2", "-shared", "-fPIC", str(Path(__file__).parent / "build/model.c"), "-lm", "-o", str(so)])
        fn = ctypes.CDLL(str(so)).csi_rf_predict
        fn.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_double)]
        allf = np.hstack([py[cols].to_numpy(), np.log((py[cols].to_numpy() + F.EPS) / (base.to_numpy(float) + F.EPS))])
        labels = (ctypes.c_char_p * 2).in_dll(ctypes.CDLL(str(so)), "csi_rf_labels")
        wi = [l.decode() for l in labels].index("wave")
        pw = []
        for row in allf[:, fmap].astype(np.float32):
            out = (ctypes.c_double * 2)()
            fn(np.ascontiguousarray(row).ctypes.data_as(ctypes.POINTER(ctypes.c_float)), out)
            pw.append(out[wi])
    d = np.max(np.abs(np.array(pw) - cb.p_wave.to_numpy()))
    print(f"có nền: P(wave) C vs Python lệch tối đa {d:.1e}; P(wave) = {np.round(pw, 2).tolist()}")
    assert d < 1e-3, d
    print("PASS parity đường có nền (nb)")
