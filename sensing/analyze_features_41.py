"""Phân tích feature motion trên dữ liệu 4.1 (5 chain, 20 ms) → docs/features_4-1/{*.png, *.csv, README.md}.

    .venv/bin/python sensing/analyze_features_41.py

Dùng mọi lượt 4.1 không exclude, chu kỳ 20 ms, nhãn wave/no_wave. Chỉ mô tả feature (không train model).
Mỗi chain xét riêng (ghi chú dự án: không gộp/so sánh chéo anten). Chạy lại khi có thêm lượt.
"""
import json, sys, warnings
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from sklearn.metrics import roc_auc_score

sys.path.insert(0, str(Path(__file__).parent))
import features as F

warnings.filterwarnings("ignore")
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs/features_4-1"
OUT.mkdir(parents=True, exist_ok=True)
C_WAVE, C_NO = "#d1495b", "#00798c"
NCH = 5
PER_CHAIN = ["std", "step", "cos_step", "log_power", "slow_fraction", "wave_fraction", "fast_fraction"]


def col(kind, c):
    return f"motion_{kind}_c{c}" if kind in ("std", "step", "cos_step") else f"motion_c{c}_{kind}"


def md_table(df):
    return "\n".join(["| " + " | ".join(map(str, df.columns)) + " |", "|" + "---|" * len(df.columns)]
                     + ["| " + " | ".join(map(str, r)) + " |" for r in df.itertuples(index=False)])


def save(fig, name):
    fig.tight_layout()
    fig.savefig(OUT / name, dpi=110)
    plt.close(fig)


runs = []
for f in sorted((ROOT / "data/raw").glob("*_4-1_*/label.json")):
    l = json.loads(f.read_text())
    if not l.get("exclude") and l.get("period_ms") == 20 and l.get("label") in ("wave", "no_wave"):
        runs.append(f.parent)
assert {json.loads((r / "label.json").read_text())["label"] for r in runs} == {"wave", "no_wave"}, "cần cả 2 lớp"

W, info, amp = [], [], {}
for r in runs:
    rec, a = F.load_recording(r)
    t = rec.chip_tsf_us.to_numpy() / 1e6
    g = np.diff(t) * 1000
    w = F.motion_windows(r, chains=NCH)
    w["t_s"] = (w.start_tsf_us / 1e6 - t[0]) if "start_tsf_us" in w else np.arange(len(w)) * F.MOTION_HOP_S
    W.append(w)
    lab = w.label.iloc[0] if len(w) else json.loads((r / "label.json").read_text())["label"]
    info.append(dict(run=r.name[9:15] + ("W" if lab == "wave" else "N"), recording_id=r.name, label=lab,
                     records=len(rec), rate_per_s=round(len(rec) / (t[-1] - t[0]), 1),
                     gaps_gt160ms=int((g > 160).sum()), windows=len(w),
                     **{f"amp_c{c}": round(float(np.median(a[:, c].mean(1))), 0) for c in range(a.shape[1])}))
    amp[r.name] = (t - t[0], a)
W = pd.concat(W, ignore_index=True)
info = pd.DataFrame(info)
short = dict(zip(info.recording_id, info.run))
W["run"] = W.recording_id.map(short)
y = (W.label == "wave").astype(int).to_numpy()
order = list(info.sort_values(["label", "run"]).run)  # no_wave trước, wave sau
info.to_csv(OUT / "luot.csv", index=False)

# ---- bảng AUC + tách theo lượt cho mọi feature
names = F.motion_feature_names(NCH)
rows = []
for n in names:
    x = W[n].to_numpy()
    if np.nanstd(x) == 0:
        continue
    auc = roc_auc_score(y, np.nan_to_num(x))
    med = W.groupby("run")[n].median()
    wv, nw = med[[r for r in med.index if r.endswith("W")]], med[[r for r in med.index if r.endswith("N")]]
    sep = "wave>no" if wv.min() > nw.max() else "wave<no" if wv.max() < nw.min() else "chồng"
    rows.append(dict(feature=n, auc=round(auc, 3), auc_sep=round(max(auc, 1 - auc), 3),
                     direction="wave cao hơn" if auc >= 0.5 else "wave thấp hơn", run_separation=sep,
                     wave_run_medians=" ".join(f"{v:.3g}" for v in wv), no_wave_run_medians=" ".join(f"{v:.3g}" for v in nw),
                     wave_min_over_max=round(float(wv.max() / wv.min()), 2) if (wv > 0).all() else np.nan))
A = pd.DataFrame(rows).sort_values("auc_sep", ascending=False)
A.to_csv(OUT / "feature_auc.csv", index=False)
W.to_csv(OUT / "windows.csv", index=False)

# 1. tổng quan lượt
fig, ax = plt.subplots(1, 3, figsize=(13, 3.6))
cols_ = [C_WAVE if l == "wave" else C_NO for l in info.label]
for a_, k, tt in zip(ax, ["rate_per_s", "gaps_gt160ms", "windows"], ["Bản ghi/s", "Gap > 160 ms", "Số cửa sổ 3.2 s"]):
    a_.bar(info.run, info[k], color=cols_); a_.set_title(tt); a_.tick_params(axis="x", rotation=45)
save(fig, "1_tong_quan_luot.png")

# 2. xếp hạng AUC mọi feature
fig, ax = plt.subplots(figsize=(10, 11))
Ar = A.iloc[::-1]
c_ = ["#2a9d8f" if s != "chồng" else "#adb5bd" for s in Ar.run_separation]
ax.barh(Ar.feature, Ar.auc_sep, color=c_)
ax.axvline(0.5, color="k", lw=0.8, ls="--")
ax.set_xlim(0.4, 1); ax.set_xlabel("AUC tách wave/no_wave (cửa sổ, max(AUC, 1−AUC))")
ax.set_title("Xếp hạng feature — xanh: trung vị theo lượt tách hoàn toàn 2 lớp; xám: chồng")
ax.tick_params(axis="y", labelsize=7)
save(fig, "2_xep_hang_auc.png")

# 3. heatmap chain × loại feature
M = np.array([[A.set_index("feature").auc.get(col(k, c), np.nan) for k in PER_CHAIN] for c in range(NCH)])
fig, ax = plt.subplots(figsize=(9, 3.8))
im = ax.imshow(M, cmap="RdBu_r", vmin=0, vmax=1)
ax.set_xticks(range(len(PER_CHAIN)), PER_CHAIN, rotation=30); ax.set_yticks(range(NCH), [f"chain {c}" for c in range(NCH)])
for i in range(NCH):
    for j in range(len(PER_CHAIN)):
        ax.text(j, i, f"{M[i, j]:.2f}", ha="center", va="center", fontsize=8)
fig.colorbar(im, label="AUC (>0.5: wave cao hơn)")
ax.set_title("AUC theo chain × loại feature (mỗi chain xét riêng)")
save(fig, "3_auc_chain_x_feature.png")

# 4. box theo lượt: std/step/log_power × 5 chain
fig, ax = plt.subplots(3, NCH, figsize=(16, 9), sharey="row")
for i, k in enumerate(["std", "step", "log_power"]):
    for c in range(NCH):
        data = [W[W.run == r][col(k, c)].dropna() for r in order]
        b = ax[i, c].boxplot(data, tick_labels=order, patch_artist=True, widths=0.6)
        for p, r in zip(b["boxes"], order):
            p.set_facecolor(C_WAVE if r.endswith("W") else C_NO); p.set_alpha(0.6)
        ax[i, c].set_title(col(k, c), fontsize=9); ax[i, c].tick_params(axis="x", rotation=45, labelsize=7)
fig.suptitle("Feature lõi (motion_core) theo từng lượt — đỏ: wave, xanh: no_wave")
save(fig, "4_box_theo_luot.png")

# 5. diễn biến theo thời gian
fig, ax = plt.subplots(len(order), 1, figsize=(12, 2.1 * len(order)), sharex=True, sharey=True)
for a_, r in zip(np.atleast_1d(ax), order):
    s = W[W.run == r]
    for c in range(NCH):
        a_.plot(s.t_s, s[col("std", c)], ".-", lw=0.8, ms=3, label=f"c{c}")
    a_.set_ylabel(r, color=C_WAVE if r.endswith("W") else C_NO)
np.atleast_1d(ax)[0].legend(ncol=NCH, fontsize=7, loc="upper right")
np.atleast_1d(ax)[-1].set_xlabel("thời gian trong lượt (s)")
fig.suptitle("motion_std theo cửa sổ (mỗi chấm = 1 cửa sổ 3.2 s); khoảng trống = mất CSI")
save(fig, "5_dien_bien_thoi_gian.png")

# 6. scatter std vs log_power theo chain
fig, ax = plt.subplots(1, NCH, figsize=(17, 3.6))
for c in range(NCH):
    for lab, cc in (("no_wave", C_NO), ("wave", C_WAVE)):
        s = W[W.label == lab]
        ax[c].scatter(s[col("std", c)], s[col("log_power", c)], s=10, c=cc, alpha=0.6, label=lab)
    ax[c].set_title(f"chain {c}"); ax[c].set_xlabel("std"); ax[0].set_ylabel("log_power")
ax[0].legend(fontsize=7)
save(fig, "6_scatter_std_logpower.png")

# 7. feature phổ/PCA (pc1)
pcs = [n for n in names if n.startswith("motion_pc")]
fig, ax = plt.subplots(2, (len(pcs) + 1) // 2, figsize=(16, 6.5))
for a_, n in zip(ax.flat, pcs):
    data = [W[W.run == r][n].dropna() for r in order]
    b = a_.boxplot(data, tick_labels=order, patch_artist=True, widths=0.6)
    for p, r in zip(b["boxes"], order):
        p.set_facecolor(C_WAVE if r.endswith("W") else C_NO); p.set_alpha(0.6)
    a_.set_title(f"{n}  AUC {A.set_index('feature').auc.get(n, np.nan):.2f}", fontsize=8)
    a_.tick_params(axis="x", rotation=45, labelsize=7)
for a_ in ax.flat[len(pcs):]:
    a_.axis("off")
fig.suptitle("Feature PCA/phổ (gộp 5 chain qua PCA) theo lượt")
save(fig, "7_pca_pho.png")

# 8. biên độ chuẩn hoá (tone × thời gian), 1 lượt mỗi lớp, mọi chain, 15 s đầu
pick = [info[info.label == "no_wave"].sort_values("windows").recording_id.iloc[-1],
        info[info.label == "wave"].sort_values("windows").recording_id.iloc[-1]]
fig, ax = plt.subplots(NCH, 2, figsize=(14, 2.2 * NCH), sharex=True)
for j, rid in enumerate(pick):
    t, a = amp[rid]
    m = t < 15
    for c in range(NCH):
        z = a[m, c] / a[m, c].mean(1, keepdims=True)
        ax[c, j].imshow(z.T, aspect="auto", origin="lower", extent=[t[m][0], t[m][-1], 0, z.shape[1]],
                        cmap="viridis", vmin=0.3, vmax=1.7)
        ax[c, j].set_ylabel(f"c{c} tone")
    ax[0, j].set_title(f"{short[rid]} ({'wave' if short[rid].endswith('W') else 'no_wave'}) — biên độ / TB tone của record")
    ax[-1, j].set_xlabel("s (thứ tự record, không nội suy)")
save(fig, "8_bien_do_chuan_hoa.png")

# 9. tương quan feature lõi giữa các chain
core = [col(k, c) for k in ("std", "step", "log_power") for c in range(NCH)]
Cm = W[core].corr().to_numpy()
fig, ax = plt.subplots(figsize=(8.5, 7.5))
im = ax.imshow(Cm, cmap="RdBu_r", vmin=-1, vmax=1)
ax.set_xticks(range(len(core)), core, rotation=90, fontsize=7); ax.set_yticks(range(len(core)), core, fontsize=7)
fig.colorbar(im)
ax.set_title("Tương quan feature lõi (cửa sổ) — cao = các chain thấy cùng chuyển động")
save(fig, "9_tuong_quan.png")

# ---- README
top = A.head(12)
md = ["# Phân tích feature — router 4.1 (5 chain, chu kỳ 20 ms)", "",
      "Sinh bởi `.venv/bin/python sensing/analyze_features_41.py`. Station = router 11.1 (STA). Chạy lại khi có thêm lượt.",
      "", "**Nhận xét (viết tay, không bị ghi đè khi chạy lại): [NHAN_XET.md](NHAN_XET.md)**",
      "", "## Lượt dùng", "", md_table(info), "",
      f"Tổng {len(W)} cửa sổ 3.2 s (trượt 0.8 s): wave {int(y.sum())}, no_wave {int((1 - y).sum())}.", "",
      "## Đọc nhanh", "",
      "- **AUC** (cửa sổ): 0.5 = không tách, 1 (hoặc 0) = tách hoàn toàn. Các cửa sổ cùng lượt không độc lập → AUC lạc quan.",
      "- **run_separation**: trung vị theo lượt — `wave>no` nghĩa là MỌI lượt wave cao hơn MỌI lượt no_wave (điều kiện "
      "cần để ngưỡng chung hoạt động trên lượt chưa thấy).",
      "- **wave_min_over_max**: tỉ lệ giữa lượt wave mạnh nhất / yếu nhất — lớn = cường độ vẫy khác nhau nhiều giữa các lượt.",
      "", "## 12 feature tách tốt nhất", "",
      md_table(top[["feature", "auc", "direction", "run_separation", "wave_run_medians", "no_wave_run_medians",
           "wave_min_over_max"]]), "",
      "Bảng đầy đủ: `feature_auc.csv`; mọi cửa sổ: `windows.csv`; lượt: `luot.csv`.", "",
      "## Hình", ""]
for p, d in [("1_tong_quan_luot.png", "Tốc độ thu, số gap > 160 ms, số cửa sổ mỗi lượt"),
             ("2_xep_hang_auc.png", "Xếp hạng mọi feature theo AUC"),
             ("3_auc_chain_x_feature.png", "AUC theo chain × loại feature"),
             ("4_box_theo_luot.png", "motion_core theo lượt, từng chain"),
             ("5_dien_bien_thoi_gian.png", "motion_std theo thời gian trong lượt"),
             ("6_scatter_std_logpower.png", "std vs log_power theo chain"),
             ("7_pca_pho.png", "Feature PCA / phổ"),
             ("8_bien_do_chuan_hoa.png", "Biên độ CSI chuẩn hoá (tone × thời gian), 15 s đầu"),
             ("9_tuong_quan.png", "Tương quan giữa feature lõi các chain")]:
    md += [f"### {d}", "", f"![{d}]({p})", ""]
(OUT / "README.md").write_text("\n".join(md))
print(info.to_string(index=False))
print(A.head(15)[["feature", "auc", "run_separation", "wave_min_over_max"]].to_string(index=False))
print("→", OUT)
