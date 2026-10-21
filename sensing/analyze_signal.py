"""Phân tích tín hiệu CSI và đặc trưng → docs/figures/*.png + docs/analysis_numbers.json.

    .venv/bin/python sensing/analyze_signal.py

Dùng mọi lượt trong data/raw (kể cả lượt test để xem tín hiệu); phần độ tách đặc trưng chỉ dùng lượt dataset
80 MHz của 11.1 (cùng router, cùng chu kỳ). Chạy lại khi có thêm dữ liệu; báo cáo: docs/PHAN_TICH_TIN_HIEU_DAC_TRUNG.md.
"""
import json, sys, warnings
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).parent))
import features as F
import train as T

warnings.filterwarnings("ignore")
ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "data/raw"
FIG = ROOT / "docs/figures"
FIG.mkdir(parents=True, exist_ok=True)
NUM = {}
C_WAVE, C_NO, C_GREY = "#d1495b", "#00798c", "#8d99ae"


def run(name):
    return next(RAW.glob(f"{name}*"))


def label(folder):
    return json.loads((folder / "label.json").read_text())


def save(fig, name):
    fig.tight_layout()
    fig.savefig(FIG / name, dpi=110)
    plt.close(fig)


# ---------------------------------------------------------------- 1. cấu trúc CSI: chain × tone × khối 20 MHz
fig, axes = plt.subplots(3, 1, figsize=(10, 8), sharex=True)
for ax, (name, title) in zip(axes, [("20261020-102612_11-1_no_wave", "11.1, request BW=0 (ACK 20 MHz)"),
                                    ("20261020-104320_11-1_no_wave", "11.1, request BW=2 (ACK 80 MHz)"),
                                    ("20261020-134520_4-1_test", "4.1 (RDP488, 5 chain), BW=2")]):
    iq = pd.read_csv(run(name) / "export_csi/csi_iq.csv", usecols=["chain", "tone_index", "amplitude"])
    m = iq.groupby(["chain", "tone_index"]).amplitude.mean().unstack(0)
    for c in m.columns:
        ax.plot(m.index, m[c], lw=1, label=f"chain {c}")
    for b in range(1, 4):
        ax.axvline(b * 53 - 0.5, color="k", lw=0.5, ls=":")
    ax.set_yscale("log")
    ax.set_title(title, fontsize=10)
    ax.set_ylabel("biên độ TB")
    ax.legend(fontsize=7, ncol=5, loc="lower left")
    blocks = iq.assign(b=iq.tone_index // 53).groupby("b").amplitude.mean().round(1).tolist()
    NUM[f"block_mean_amplitude::{title}"] = blocks
axes[-1].set_xlabel("tone_index (4 khối × 53 tone; DC ở tone 26 mỗi khối)")
save(fig, "1_cau_truc_csi.png")

# ---------------------------------------------------------------- 2. lấy mẫu: khoảng cách giữa các record
fig, ax = plt.subplots(figsize=(9, 4))
bins = np.arange(0, 400, 10)
for name, lab_, col in [("20261020-104320_11-1_no_wave", "11.1 @50 ms", C_NO),
                        ("20261020-134413_4-1_test", "4.1 @50 ms", C_WAVE),
                        ("20261020-134520_4-1_test", "4.1 @20 ms", "#edae49")]:
    r = pd.read_csv(run(name) / "export_csi/records.csv", usecols=["chip_tsf_us"])
    g = np.diff(r.chip_tsf_us.to_numpy()) / 1e3
    secs = (r.chip_tsf_us.iloc[-1] - r.chip_tsf_us.iloc[0]) / 1e6
    NUM[f"sampling::{lab_}"] = dict(rate_per_s=round(len(r) / secs, 1), median_ms=round(float(np.median(g)), 1),
                                    gaps_over_160ms_per_min=round(float((g > 160).sum() / secs * 60), 1))
    ax.hist(np.clip(g, 0, 395), bins, histtype="step", lw=1.5, color=col,
            label=f"{lab_}: {len(r) / secs:.1f} rec/s, {(g > 160).sum() / secs * 60:.0f} gap>160 ms/phút")
ax.axvline(160, color="k", ls="--", lw=0.8)
ax.text(163, ax.get_ylim()[1] * 0.8, "ngưỡng cắt đoạn 160 ms", fontsize=8)
ax.set_xlabel("khoảng cách giữa 2 record liên tiếp (ms)")
ax.set_ylabel("số lần")
ax.set_yscale("log")
ax.legend(fontsize=8)
save(fig, "2_lay_mau.png")

# ---------------------------------------------------------------- 3. AGC / gain: thang biên độ nhảy theo record
name = "20261020-104320_11-1_no_wave"
recs, a = F.load_recording(run(name))
t = (recs.chip_tsf_us - recs.chip_tsf_us.iloc[0]).to_numpy() / 1e6
level = a.mean(axis=2)  # n × chain
norm = a / (a.mean(axis=2, keepdims=True) + F.EPS)
gain = pd.to_numeric(recs.wmi_gain_tbl_ch0, errors="coerce").to_numpy()
fig, axes = plt.subplots(3, 1, figsize=(10, 7), sharex=True)
for c in range(3):
    axes[0].plot(t, level[:, c], lw=0.7, label=f"chain {c}")
axes[0].set_ylabel("biên độ TB (thô)")
axes[0].legend(fontsize=7)
axes[0].set_title(f"{name} (không vẫy): mức biên độ thô nhảy theo AGC/gain", fontsize=10)
axes[1].step(t, gain, where="post", color=C_GREY)
axes[1].set_ylabel("gain table ch0")
axes[2].plot(t, norm[:, 0, 60], lw=0.7, label="tone 60 / TB record")
axes[2].plot(t, norm[:, 0, 150], lw=0.7, label="tone 150 / TB record")
axes[2].set_ylabel("biên độ chuẩn hoá")
axes[2].set_xlabel("thời gian (s)")
axes[2].legend(fontsize=7)
save(fig, "3_agc_chuan_hoa.png")
jump = np.abs(np.diff(np.log(level[:, 0])))
NUM["agc"] = dict(recording=name, raw_level_cv_chain0=round(float(level[:, 0].std() / level[:, 0].mean()), 3),
                  normalized_shape_cv_tone60=round(float(norm[:, 0, 60].std() / norm[:, 0, 60].mean()), 3),
                  gain_table_counts=pd.Series(gain).value_counts().to_dict(),
                  records_with_level_jump_over_20pct=int((jump > np.log(1.2)).sum()), records=len(level))

# ---------------------------------------------------------------- 4. chuyển động: tone × thời gian
fig, axes = plt.subplots(1, 2, figsize=(11, 4), sharey=True)
for ax, name, lab_ in zip(axes, ["20261020-104542_11-1_wave", "20261020-104320_11-1_no_wave"], ["vẫy tay", "không vẫy"]):
    recs, a = F.load_recording(run(name))
    keep = F.qc_keep(recs)
    tt = (recs.chip_tsf_us.to_numpy() - recs.chip_tsf_us.iloc[0]) / 1e6
    sel = keep & (tt >= 10) & (tt < 20)
    x = a[sel][:, 0] / (a[sel][:, 0].mean(axis=1, keepdims=True) + F.EPS)
    x = x / x.mean(axis=0) - 1
    im = ax.imshow(x.T, aspect="auto", cmap="RdBu_r", vmin=-0.6, vmax=0.6,
                   extent=[tt[sel][0], tt[sel][-1], 208, 0])
    ax.set_title(f"{lab_} — chain 0, 10 s", fontsize=10)
    ax.set_xlabel("thời gian (s)")
axes[0].set_ylabel("tone (bỏ DC)")
fig.colorbar(im, ax=axes, label="lệch so với TB tone", shrink=0.8)
fig.savefig(FIG / "4_chuyen_dong_heatmap.png", dpi=110)
plt.close(fig)

# ---------------------------------------------------------------- 5. phổ thời gian (PC1) theo Hz
dataset = T.select_folders(RAW, 2, "192.168.11.1")
mot = pd.concat([F.motion_windows(f) for f in dataset], ignore_index=True)
mot["y"] = np.where(mot.label == "wave", "wave", "no_wave")
amp = pd.concat([F.recording_windows(f)[0] for f in dataset], ignore_index=True)
amp["y"] = np.where(amp.label == "wave", "wave", "no_wave")


def pc1_psd(folder):
    recs, a = F.load_recording(folder)
    a = a[:, :3]
    tsf = recs.chip_tsf_us.to_numpy(np.int64)
    out = []
    for piece in F.segments(recs, F.qc_keep(recs)):
        tt = (tsf[piece] - tsf[piece[0]]) / 1e6
        s = 0.0
        while s + F.MOTION_WINDOW_S <= tt[-1]:
            sel = (tt >= s) & (tt < s + F.MOTION_WINDOW_S)
            if sel.sum() >= F.MIN_COVERAGE * F.MOTION_WINDOW_S / 0.0512:
                x = a[piece[sel]] / (a[piece[sel]].mean(axis=2, keepdims=True) + F.EPS)
                grid = s + np.arange(64) / F.FS
                g = np.stack([np.interp(grid, tt[sel], x.reshape(len(x), -1)[:, k]) for k in range(x.shape[1] * 208)], 1)
                d = g - g.mean(0)
                tc = np.arange(64) - 31.5
                d -= tc[:, None] * (tc @ d / (tc @ tc))
                u, sv, _ = np.linalg.svd(d, full_matrices=False)
                p = np.abs(np.fft.rfft(u[:, 0] * sv[0] * np.hanning(64))) ** 2
                out.append(p / p[1:].sum())
            s += F.MOTION_HOP_S
    return np.array(out)


freq = np.fft.rfftfreq(64, 1 / F.FS)
fig, ax = plt.subplots(figsize=(9, 4))
for f in dataset:
    p = pc1_psd(f)
    lab_ = label(f)["label"]
    ax.plot(freq[1:], np.median(p[:, 1:], 0), color=C_WAVE if lab_ == "wave" else C_NO, lw=1.5,
            label=f"{f.name[9:15]} {lab_} ({len(p)} cửa sổ)")
    NUM[f"pc1_band_fraction::{f.name}"] = {b: round(float(np.median(p[:, (freq >= lo) & (freq < hi)].sum(1))), 3)
                                           for b, (lo, hi) in F.BANDS.items()}
ax.axvspan(1, 4, color="k", alpha=0.06, label="dải vẫy tay giả định 1–4 Hz")
ax.set_xlabel("tần số (Hz)")
ax.set_ylabel("phần công suất PC1 (trung vị)")
ax.set_yscale("log")
ax.legend(fontsize=8)
save(fig, "5_pho_pc1.png")

# ---------------------------------------------------------------- 6. độ tách từng đặc trưng (AUC cửa sổ + khoảng hở theo lượt)
def auc(x, y):
    r = pd.Series(x).rank().to_numpy()
    pos = y == "wave"
    n1, n0 = pos.sum(), (~pos).sum()
    return (r[pos].sum() - n1 * (n1 + 1) / 2) / (n1 * n0)


rows = []
for w, cols, fam in [(amp, F.AMP_FEATURES, lambda k: F.AMP_CATALOG[k]["family"]),
                     (mot, F.MOTION_FEATURES, lambda k: "motion_core" if k in F.MOTION_CORE else "motion")]:
    med = w.groupby(["recording_id", "y"])[cols].median().reset_index()
    for k in cols:
        a_ = auc(w[k].to_numpy(), w.y.to_numpy())
        wv, nv = med[med.y == "wave"][k], med[med.y == "no_wave"][k]
        gap = max(wv.min() - nv.max(), nv.min() - wv.max()) / (w[k].std() + 1e-12)
        rows.append(dict(feature=k, family=fam(k), auc=a_, sep=abs(a_ - 0.5) * 2, rec_gap=gap))
sep = pd.DataFrame(rows).sort_values("sep", ascending=False)
sep.round(3).to_csv(ROOT / "docs/feature_separability.csv", index=False)
fam_col = {"level": "#6c757d", "shape": "#adb5bd", "dynamic": "#00798c", "spectral": "#30638e",
           "coherence": "#edae49", "motion": "#d1495b", "motion_core": "#8b1e3f"}
top = sep.head(20)[::-1]
fig, ax = plt.subplots(figsize=(9, 6))
ax.barh(top.feature, top.auc - 0.5, left=0.5, color=[fam_col[f] for f in top.family])
ax.axvline(0.5, color="k", lw=0.8)
ax.set_xlim(0, 1)
ax.set_xlabel("AUC theo cửa sổ (0.5 = không tách; >0.5: wave cao hơn)")
from matplotlib.patches import Patch
ax.legend(handles=[Patch(color=c, label=f) for f, c in fam_col.items()], fontsize=8, loc="lower left",
          title="họ đặc trưng", title_fontsize=8)
ax.set_title("20 đặc trưng tách wave/no_wave tốt nhất (4 lượt 11.1, cùng phiên S1/P1)", fontsize=10)
save(fig, "6_do_tach_dac_trung.png")
NUM["separability_by_family"] = sep.groupby("family").agg(n=("feature", "size"), median_sep=("sep", "median"),
                                                          full_recording_gap=("rec_gap", lambda s: int((s > 0).sum()))
                                                          ).round(3).to_dict(orient="index")

# ---------------------------------------------------------------- 7. motion_step_c0 theo lượt
fig, ax = plt.subplots(figsize=(8, 4))
recs_ = sorted(mot.recording_id.unique())
data = [mot[mot.recording_id == r].motion_step_c0 for r in recs_]
bp = ax.boxplot(data, tick_labels=[r[9:15] + "\n" + r.split("_")[2] for r in recs_], patch_artist=True)
for patch, r in zip(bp["boxes"], recs_):
    patch.set_facecolor(C_WAVE if "_wave" in r and "no_wave" not in r else C_NO)
ax.set_ylabel("motion_step_c0 (bước đổi hình dạng / 50 ms)")
ax.set_title("Mỗi cửa sổ 3.2 s, theo từng lượt thu (11.1)", fontsize=10)
save(fig, "7_motion_step_theo_luot.png")

# ---------------------------------------------------------------- 8. LORO các bộ đặc trưng
fam = lambda k: F.AMP_CATALOG[k]["family"]
loro = {}
for name_, w, cols in [("amp84", amp, F.AMP_FEATURES),
                       ("amp level+shape (tĩnh)", amp, [k for k in F.AMP_FEATURES if fam(k) in ("level", "shape")]),
                       ("amp dynamic+spectral", amp, [k for k in F.AMP_FEATURES if fam(k) in ("dynamic", "spectral")]),
                       ("motion 31", mot, F.MOTION_FEATURES), ("motion_core 9", mot, F.MOTION_CORE)]:
    ev = T.loro(w, cols)
    loro[name_] = dict(recordings_correct=f"{ev['recording_correct']}/{ev['recordings']}",
                       window_accuracy=round(ev["window_accuracy"], 3),
                       p_wave_per_recording={r[0][9:15] + "_" + r[1]: round(r[2], 2) for r in ev["per_recording"]})
NUM["loro"] = loro
NUM["dataset"] = {f.name: dict(label=label(f)["label"], motion_windows=int((mot.recording_id == f.name).sum()),
                               amp_windows=int((amp.recording_id == f.name).sum())) for f in dataset}
(ROOT / "docs/analysis_numbers.json").write_text(json.dumps(NUM, ensure_ascii=False, indent=1, default=str))
print(json.dumps(NUM, ensure_ascii=False, indent=1, default=str))
