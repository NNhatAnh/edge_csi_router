"""So sánh tín hiệu CSI giữa các nguồn: router 11.1 (RDP499, iPhone), router 4.1 (RDP488, station = 11.1) và lượt
ngoài (vd. PHONGHOP2, RDP499) → docs/so_sanh_nguon/{*.png, luot.csv, README.md}.

    .venv/bin/python sensing/compare_sources.py [thư mục lượt ngoài ...]

Lượt ngoài phải có relay.bin đã giải mã bằng kit (export_csi/, export/) + label.json (board, label, period_ms).
Mỗi chain xét riêng, chỉ so 3 chain đầu (4.1 có 5). Feature: features.invariant_features (cửa sổ 3.2 s, lưới 20 Hz).
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

warnings.filterwarnings("ignore")
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs/so_sanh_nguon"
OUT.mkdir(parents=True, exist_ok=True)
SRC_COLOR = {"11.1": "#e09f3e", "4.1": "#00798c", "ngoài": "#6a4c93"}
KEY = ["inv_std_c0", "inv_hf_c0", "inv_ffrac_c0", "inv_ratio_hfrac_01"]
SPEC_WIN = 6.4  # cửa sổ phổ dài hơn để phân giải dải thở


def pick_runs():
    runs = []
    for f in sorted((ROOT / "data/raw").glob("*/label.json")):
        l = json.loads(f.read_text())
        if l.get("exclude") or l.get("label") not in ("wave", "no_wave") or l.get("bw") != 2:
            continue
        if l.get("board") == "192.168.11.1":
            runs.append(("11.1", f.parent))
        elif l.get("board") == "192.168.4.1" and l.get("period_ms") == 20:
            runs.append(("4.1", f.parent))
    return runs + [("ngoài", Path(p)) for p in sys.argv[1:]]


def spectrum(rec, h):
    """Phổ TB (3 chain, 208 tone) của biên độ chuẩn hoá, cửa sổ SPEC_WIN, như analyze_spectrum_41."""
    n = int(SPEC_WIN * F.FS)
    tsf = rec.chip_tsf_us.to_numpy(np.int64)
    amp, out = np.abs(h[:, :3]), []
    tc = np.arange(n) - (n - 1) / 2
    for piece in F.segments(rec, F.qc_keep(rec), F.TIME_MAX_GAP_US):
        t = (tsf[piece] - tsf[piece[0]]) / 1e6
        s = 0.0
        while s + SPEC_WIN <= t[-1]:
            sel = (t >= s) & (t < s + SPEC_WIN)
            if sel.sum() >= 0.5 * SPEC_WIN / 0.0512:
                g = s + np.arange(n) / F.FS
                ps = []
                for c in range(3):
                    a = amp[piece[sel], c] / amp[piece[sel], c].mean(1, keepdims=True)
                    x = F._interp_cols(g, t[sel], a)
                    x = x - x.mean(0)
                    x = x - np.outer(tc, tc @ x / np.sum(tc * tc))
                    ps.append((np.abs(np.fft.rfft(x * np.hanning(n)[:, None], axis=0)) ** 2).mean(1))
                out.append(np.mean(ps, 0))
            s += 1.6
    return np.array(out)


rows, W, SPEC = [], [], {}
for src, d in pick_runs():
    lab = json.loads((d / "label.json").read_text())
    rec, h = F.load_recording(d, iq=True)
    t = rec.chip_tsf_us.to_numpy(np.int64) / 1e6
    g = np.diff(t) * 1000
    keep = F.qc_keep(rec)
    meta = pd.read_csv(d / "export/records_meta.csv", usecols=lambda c: c in ("chip_tsf_us", "rssi_comb_db"))
    w = F.invariant_windows(d)
    w["src"], w["run"] = src, d.name
    W.append(w)
    note = (lab.get("note") or "")
    kind = "thở mạnh" if d.name.startswith("20261020-171141") or "tho manh" in note else lab["label"]
    SPEC[d.name] = (src, kind, spectrum(rec, h))
    rows.append(dict(
        _dir=d.name, nguồn=src, lượt=d.name[:15], nhãn=kind, chain=h.shape[1], chu_kỳ_ms=lab.get("period_ms"),
        bản_ghi=len(rec), giây=round(t[-1] - t[0]), bản_ghi_s=round(len(rec) / (t[-1] - t[0]), 1),
        gap_160ms=int((g > 160).sum()), bw80=f"{F.bw80(rec).mean():.0%}", giữ_QC=f"{keep.mean():.0%}", cửa_sổ=len(w),
        rssi_comb=meta.rssi_comb_db.median() if "rssi_comb_db" in meta else np.nan,
        **{f"|H|_c{c}": round(float(np.median(np.abs(h[keep, c]).mean(1))), 0) for c in range(3)},
        **{k: round(float(w[k].median()), 3) if len(w) else np.nan for k in KEY}))
    print(f"{src:5s} {d.name[:24]:24s} {kind:8s} {len(rec)} rec, {len(w)} cửa sổ")
T = pd.DataFrame(rows)
W = pd.concat(W, ignore_index=True)
T.drop(columns=["_dir"]).to_csv(OUT / "luot.csv", index=False)
order = list(T.sort_values(["nguồn", "nhãn"]).apply(lambda r: f"{r.nguồn} {r.lượt[9:]} {r.nhãn}", axis=1))
T["tên"] = T.apply(lambda r: f"{r.nguồn} {r.lượt[9:]} {r.nhãn}", axis=1)

# 1. chất lượng link
fig, ax = plt.subplots(1, 4, figsize=(17, 4.2))
Ts = T.set_index("tên").loc[order]
for a_, k, tt in zip(ax, ["bản_ghi_s", "gap_160ms", "cửa_sổ", "rssi_comb"],
                     ["Bản ghi CSI / s", "Gap > 160 ms / lượt", "Cửa sổ 3.2 s / lượt", "rssi_comb (dB trên nền nhiễu)"]):
    a_.barh(order, Ts[k], color=[SRC_COLOR[s] for s in Ts.nguồn]); a_.set_title(tt); a_.invert_yaxis()
    a_.tick_params(axis="y", labelsize=7)
    if a_ is not ax[0]:
        a_.set_yticklabels([])
fig.suptitle("Chất lượng link theo lượt — cam: 11.1, xanh: 4.1, tím: lượt ngoài")
fig.tight_layout(); fig.savefig(OUT / "1_chat_luong_link.png", dpi=110); plt.close(fig)

# 2. feature chính theo lượt (log)
W["tên"] = W.run.map(dict(zip(T._dir, T.tên)))
fig, ax = plt.subplots(1, 4, figsize=(18, 5.5))
for a_, k in zip(ax, KEY):
    data = [W[W.tên == n][k].dropna() for n in order]
    b = a_.boxplot(data, vert=False, tick_labels=order, patch_artist=True, widths=0.6)
    for p, n in zip(b["boxes"], order):
        p.set_facecolor(SRC_COLOR[n.split()[0]]); p.set_alpha(0.55 if "no_wave" in n or "thở" in n else 0.95)
    a_.set_xscale("log"); a_.set_title(k); a_.invert_yaxis(); a_.tick_params(axis="y", labelsize=7)
    if a_ is not ax[0]:
        a_.set_yticklabels([])
fig.suptitle("Feature đang dùng theo lượt (đậm: wave, nhạt: no_wave / thở mạnh)")
fig.tight_layout(); fig.savefig(OUT / "2_feature_theo_luot.png", dpi=110); plt.close(fig)

# 3. phổ
f = np.fft.rfftfreq(int(SPEC_WIN * F.FS), 1 / F.FS)
fig, ax = plt.subplots(1, 3, figsize=(17, 4.5), sharey=True)
for a_, src in zip(ax, ["11.1", "4.1", "ngoài"]):
    for name, (s, kind, P) in SPEC.items():
        if s != src or not len(P):
            continue
        st = dict(color="#d1495b", ls="-") if kind == "wave" else dict(color="#6a4c93", ls=":") if kind == "thở mạnh" else dict(color="#00798c", ls="--")
        a_.semilogy(f[1:], P.mean(0)[1:], label=f"{name[9:15]} {kind}", **st)
    a_.axvspan(0.1, 0.7, color="grey", alpha=0.15); a_.axvspan(1, 4, color="orange", alpha=0.1)
    a_.set_title(f"nguồn {src}"); a_.set_xlabel("Hz"); a_.set_xlim(0, 10); a_.legend(fontsize=7)
ax[0].set_ylabel("công suất biến thiên (TB cửa sổ 6.4 s)")
fig.suptitle("Phổ biên độ chuẩn hoá — xám: dải thở, cam: dải vẫy 1–4 Hz")
fig.tight_layout(); fig.savefig(OUT / "3_pho.png", dpi=110); plt.close(fig)

# 4. diễn biến theo thời gian các lượt wave
waves = [n for n in order if n.endswith("wave") and "no_wave" not in n]
fig, ax = plt.subplots(len(waves), 1, figsize=(12, 1.7 * len(waves)), sharex=True)
for a_, n in zip(np.atleast_1d(ax), waves):
    s = W[W.tên == n]
    t0 = s.start_tsf_us.min()
    a_.plot((s.start_tsf_us - t0) / 1e6, s.inv_hf_c0, ".-", color=SRC_COLOR[n.split()[0]], lw=0.8, ms=3)
    a_.set_yscale("log"); a_.set_ylabel(n, fontsize=7, rotation=0, ha="right")
np.atleast_1d(ax)[-1].set_xlabel("s trong lượt")
fig.suptitle("inv_hf_c0 (năng lượng 1–4 Hz) theo cửa sổ — các lượt wave")
fig.tight_layout(); fig.savefig(OUT / "4_wave_theo_thoi_gian.png", dpi=110); plt.close(fig)


def md(df):
    return "\n".join(["| " + " | ".join(map(str, df.columns)) + " |", "|" + "---|" * len(df.columns)]
                     + ["| " + " | ".join(map(str, r)) + " |" for r in df.itertuples(index=False)])


(OUT / "README.md").write_text("\n".join([
    "# So sánh tín hiệu: 11.1 vs 4.1 vs lượt ngoài", "",
    "Sinh bởi `.venv/bin/python sensing/compare_sources.py <lượt ngoài>`. Nhận xét viết tay: [NHAN_XET.md](NHAN_XET.md).", "",
    md(T.drop(columns=["tên", "_dir"])), "",
    "![link](1_chat_luong_link.png)", "", "![feature](2_feature_theo_luot.png)", "", "![phổ](3_pho.png)", "",
    "![wave](4_wave_theo_thoi_gian.png)", ""]))
print(T.drop(columns=["tên", "_dir"]).to_string(index=False))
print("→", OUT)
