"""Feature cho cửa sổ CSI (Phase 3, ghi chú dự án).

`amplitude_features` chép NGUYÊN VĂN từ data80mhz_full_workflow_colab.py (84 feature biên độ + nuisance);
test_features.py so trực tiếp với hàm trong notebook. C extractor trên router phải bám đúng file này.

Khác notebook (dữ liệu kit tách CSI/metadata, iPhone trộn ACK 20/40/80 MHz):
- đọc export_csi/ (CSI) + export/records_meta.csv (wmi_*, agc_*) và ghép theo thứ tự + chip_tsf_us;
- chỉ giữ record wmi_capture_ok & wmi_bw == 2 (80 MHz) & không trùng (tsf, ppdu) như notebook, nhưng
  CHỈ cắt đoạn khi TSF không tăng hoặc gap > 160 ms; notebook cắt cả khi bỏ 1 record (và gap > 100 ms),
  với ~22 % record 40 MHz thì gần như không có 64 record liên tiếp. Cửa sổ 64 record ≈ 3.3–4 s.
shortcut: chưa có Hampel / phase / complex feature của notebook; thêm khi feature biên độ không đủ.
"""
import json
from pathlib import Path

import numpy as np
import pandas as pd

WINDOW = 64
MAX_GAP_US = 160000  # chu kỳ thực ~51.2 ms: cho phép bỏ ≤ 2 record liên tiếp (notebook: 100 ms)
EPS = 1e-09
NON_DC_MASK = np.tile(np.arange(53) != 26, 4)
# wmi_* chỉ có với driver 0006 (11.1); 4.1 (patch 0001-0005) thiếu → cột NaN, QC dùng rx_bw
META_COLUMNS = ["wmi_capture_ok", "wmi_bw", "agc_ch0", "agc_ch1", "agc_ch2", "wmi_cfo_raw",
                "wmi_rssi_dbm_ch0", "wmi_rssi_dbm_ch1", "wmi_gain_tbl_ch0", "rx_bw"]
AMP_CATALOG = {}


def amplitude_features(a, metadata):
    a = a.astype(np.float64)
    n = len(a)
    mean = a.mean(axis=0)
    rms = np.sqrt((a * a).mean(axis=(0, 2)))
    z = a / (mean[None] + EPS) - 1
    cv = a.std(axis=0) / (mean + EPS)
    envelope = np.sqrt((a * a).mean(axis=2))
    block_rms = np.sqrt((a.reshape(n, 3, 4, 52) ** 2).mean(axis=(0, 3)))
    share = block_rms**2 / (np.sum(block_rms**2, axis=1, keepdims=True) + EPS)
    f = {}

    def add(name, value, family, formula):
        f[name] = float(value)
        AMP_CATALOG[name] = {"family": family, "formula": formula}

    for c in range(3):
        add(f"level_rms_c{c}", rms[c], "level", "sqrt(mean_t,k A^2), raw amplitude units")
        for b in range(4):
            add(
                f"level_log_block_rms_c{c}_b{b}",
                np.log(block_rms[c, b] + EPS),
                "level",
                "ln RMS amplitude in native 53-slot block, excluding DC",
            )
            add(
                f"shape_block_power_share_c{c}_b{b}",
                share[c, b],
                "shape",
                "block mean A^2 / sum of four block mean A^2",
            )
        p = mean[c] / (mean[c].sum() + EPS)
        add(
            f"shape_tone_entropy_c{c}",
            -np.sum(p * np.log(p + EPS)) / np.log(len(p)),
            "shape",
            "entropy of mean amplitude across non-DC tones / ln(208)",
        )
        rough = np.mean(np.abs(np.diff(mean[c].reshape(4, 52), axis=1))) / (mean[c].mean() + EPS)
        add(
            f"shape_tone_roughness_c{c}",
            rough,
            "shape",
            "mean within-block adjacent-tone amplitude difference / tone mean",
        )
        coords = np.tile(np.linspace(-1, 1, 52), 4)
        add(
            f"shape_tone_slope_c{c}",
            np.mean((mean[c] / (mean[c].mean() + EPS) - 1) * coords),
            "shape",
            "normalized mean amplitude slope on native within-block index; no absolute frequency claim",
        )
        add(f"dynamic_cv_median_c{c}", np.median(cv[c]), "dynamic", "median_k std_t(A)/mean_t(A)")
        add(
            f"dynamic_cv_p90_c{c}",
            np.percentile(cv[c], 90),
            "dynamic",
            "90th percentile across tones of std_t(A)/mean_t(A)",
        )
        add(
            f"dynamic_relative_delta_c{c}",
            np.median(np.mean(np.abs(np.diff(a[:, c], axis=0)), axis=0) / (mean[c] + EPS)),
            "dynamic",
            "median_k mean_t abs(A[t]-A[t-1])/mean_t(A)",
        )
        norm = a[:, c] / (np.linalg.norm(a[:, c], axis=1, keepdims=True) + EPS)
        add(
            f"dynamic_shape_step_c{c}",
            np.mean(1 - np.sum(norm[1:] * norm[:-1], axis=1)),
            "dynamic",
            "mean consecutive cosine distance between tone-amplitude vectors",
        )
        add(
            f"dynamic_log_envelope_std_c{c}",
            np.std(np.log(envelope[:, c] + EPS)),
            "dynamic",
            "std_t ln(sqrt(mean_k A^2))",
        )
        for lag in [1, 8]:
            u, v = (z[:-lag, c], z[lag:, c])
            corr = np.sum(u * v, axis=0) / (
                np.sqrt(np.sum(u * u, axis=0) * np.sum(v * v, axis=0)) + EPS
            )
            add(
                f"dynamic_acf_lag{lag}_c{c}",
                np.median(corr),
                "dynamic",
                f"median_k normalized demeaned amplitude autocorrelation at {lag} record lag",
            )
        fft = np.fft.rfft(z[:, c] * np.hanning(n)[:, None], axis=0)
        power = np.mean(abs(fft) ** 2, axis=1)[1:]
        frac = power / (power.sum() + EPS)
        freq = np.arange(1, n // 2 + 1) / n
        for label, mask in [
            ("low", freq <= 0.0625),
            ("mid", (freq > 0.0625) & (freq <= 0.25)),
            ("high", freq > 0.25),
        ]:
            add(
                f"spectral_{label}_fraction_c{c}",
                frac[mask].sum(),
                "spectral",
                "fraction of relative-amplitude FFT power: low<=1/16, mid<=1/4, high<=1/2 cycles/record; not physical Doppler",
            )
        add(
            f"spectral_entropy_c{c}",
            -np.sum(frac * np.log(frac + EPS)) / np.log(len(frac)),
            "spectral",
            "normalized entropy of temporal relative-amplitude power spectrum",
        )
        add(
            f"spectral_centroid_c{c}",
            np.sum(frac * freq),
            "spectral",
            "temporal power centroid in cycles/record",
        )
        add(
            f"spectral_log_power_c{c}",
            np.log(power.sum() / (n * n) + EPS),
            "spectral",
            "ln total non-DC relative-amplitude Hann FFT power / N^2",
        )
    for c, d in [(0, 1), (0, 2), (1, 2)]:
        add(
            f"shape_chain_log_ratio_{c}{d}",
            np.log((rms[c] + EPS) / (rms[d] + EPS)),
            "shape",
            "ln ratio of raw chain RMS amplitudes",
        )
        u = envelope[:, c] - envelope[:, c].mean()
        v = envelope[:, d] - envelope[:, d].mean()
        add(
            f"coherence_envelope_corr_{c}{d}",
            np.sum(u * v) / (np.linalg.norm(u) * np.linalg.norm(v) + EPS),
            "coherence",
            "Pearson correlation between raw chain RMS envelopes",
        )
        add(
            f"coherence_log_ratio_std_{c}{d}",
            np.std(np.log((envelope[:, c] + EPS) / (envelope[:, d] + EPS))),
            "coherence",
            "std_t ln ratio of chain RMS envelopes",
        )
    for name, column in [
        ("agc_c0", "agc_ch0"),
        ("agc_c1", "agc_ch1"),
        ("agc_c2", "agc_ch2"),
        ("cfo_raw", "wmi_cfo_raw"),
        ("rssi_c0", "wmi_rssi_dbm_ch0"),
        ("rssi_c1", "wmi_rssi_dbm_ch1"),
    ]:
        add(
            "nuisance_" + name,
            pd.to_numeric(metadata[column], errors="coerce").median(),
            "nuisance",
            "diagnostic metadata median; excluded from sensing feature sets",
        )
    add(
        "nuisance_absolute_host_time",
        pd.to_numeric(metadata["kernel_ts_s"]).median(),
        "nuisance",
        "host timestamp diagnostic only, never sensing input",
    )
    add(
        "nuisance_dt_ms",
        np.median(np.diff(metadata["chip_tsf_us"].to_numpy())) / 1000,
        "nuisance",
        "median TSF sample spacing diagnostic",
    )
    return f


def is_true(series):
    return series.astype(str).str.lower().isin(["true", "1"]).to_numpy()


def segments(records, keep, max_gap_us=MAX_GAP_US):
    """Chỉ số record giữ lại, chia đoạn khi TSF không tăng hoặc gap > max_gap_us."""
    idx = np.flatnonzero(keep)
    if len(idx) == 0:
        return []
    ts = records["chip_tsf_us"].to_numpy(dtype=np.int64)[idx]
    breaks = (np.diff(ts) <= 0) | (np.diff(ts) > max_gap_us)
    return np.split(idx, np.flatnonzero(breaks) + 1)


def load_recording(folder, iq=False):
    """records (CSI header + metadata, 1 dòng/record) và biên độ a: n x chain x 208 (bỏ DC).
    Số chain đọc từ dữ liệu: 3 trên RDP499 (11.1), 5 trên RDP488 (4.1). iq=True: trả CSI phức (complex128) thay biên độ."""
    folder, want_iq = Path(folder), iq
    csi = pd.read_csv(folder / "export_csi/records.csv", low_memory=False)
    meta = pd.read_csv(folder / "export/records_meta.csv", low_memory=False)
    # decoder đôi khi không xuất metadata cho vài record (4.1: 3–9/1000, "malformed") → ghép theo TSF, GIỮ mọi
    # record CSI (metadata thiếu → NaN) như daemon C (chỉ đọc CSI); phy_ppdu_id lấy từ header CSI
    meta = meta.drop_duplicates("chip_tsf_us").set_index("chip_tsf_us").reindex(csi.chip_tsf_us).reset_index()
    meta = meta.reindex(columns=META_COLUMNS)  # cột thiếu → NaN
    records = pd.concat([csi[["record", "chip_tsf_us", "kernel_ts_s", "phy_ppdu_id", "packet_bw"]].reset_index(drop=True),
                         meta.reset_index(drop=True)], axis=1)
    ok = np.ones(len(csi), bool)
    iq = pd.read_csv(folder / "export_csi/csi_iq.csv", usecols=["record", "chain", "tone_index", "i", "q", "present"])
    per = len(iq) // max(len(ok), 1)
    assert per % 212 == 0 and len(iq) == len(ok) * per, f"Layout không phải chain x 212: {folder}"
    slots = np.arange(len(iq)) % per
    assert (iq.chain.to_numpy() == slots // 212).all() and (iq.tone_index.to_numpy() == slots % 212).all()
    assert is_true(iq.present).all(), f"Có I/Q missing: {folder}"
    if want_iq:
        a = (iq.i.to_numpy(np.float64) + 1j * iq.q.to_numpy(np.float64)).reshape(-1, per // 212, 212)[ok][:, :, NON_DC_MASK]
    else:
        a = np.hypot(iq.i.to_numpy(np.float32), iq.q.to_numpy(np.float32)).reshape(-1, per // 212, 212)[ok][:, :, NON_DC_MASK]
    # relay.bin không luôn theo thứ tự thời gian (4.1: vài trăm record cuối ghi trước) và có record hỏng với
    # kernel_ts vô lý → sắp theo TSF, bỏ record lệch > 1 giờ so với trung vị
    k = records.kernel_ts_s.to_numpy()
    order = np.argsort(records.chip_tsf_us.to_numpy(), kind="stable")
    order = order[np.abs(k[order] - np.median(k)) < 3600]
    return records.iloc[order].reset_index(drop=True), a[order]


def bw80(records):
    """Record có ACK 80 MHz. Ưu tiên wmi_bw (driver 0006); không có thì packet_bw của header CSI (4.1: = rx_bw).
    Trên 11.1 rx_bw KHÁC wmi_bw (0 cho ~70 % record dù wmi_bw = 2) → chỉ dùng rx_bw khi thiếu wmi_*."""
    if records.wmi_bw.notna().any():
        return records.wmi_bw.to_numpy() == 2
    return records.packet_bw.to_numpy() == 2  # = rx_bw trên 4.1, và là trường daemon C dùng


def qc_keep(records):
    duplicate = records.duplicated(["chip_tsf_us", "phy_ppdu_id"], keep="first").to_numpy()
    ok = is_true(records.wmi_capture_ok) if records.wmi_capture_ok.notna().any() else np.ones(len(records), bool)
    return ok & bw80(records) & ~duplicate


def recording_windows(folder):
    """Một dòng/cửa sổ 64 record: feature + nhãn/metadata từ label.json. Trả (DataFrame, qc dict).
    amp84 (notebook) cố định 3 chain → dùng 3 chain đầu kể cả trên 4.1 (5 chain)."""
    folder = Path(folder)
    label = json.loads((folder / "label.json").read_text())
    records, a = load_recording(folder)
    a = a[:, :3]
    keep = qc_keep(records)
    rows = []
    for piece in segments(records, keep):
        for start in range(0, len(piece) - WINDOW + 1, WINDOW):
            use = piece[start:start + WINDOW]
            rm = records.iloc[use]
            f = amplitude_features(a[use], rm)
            f["nuisance_gain_tbl_nonzero_fraction"] = float((pd.to_numeric(rm.wmi_gain_tbl_ch0) != 0).mean())
            rows.append(dict(recording_id=folder.name, window=len(rows), start_tsf_us=int(rm.chip_tsf_us.iloc[0]),
                             label=label.get("label"), session=label.get("session", ""),
                             position=label.get("position", ""), person=label.get("person", ""), **f))
    qc = dict(recording_id=folder.name, records=len(records), kept=int(keep.sum()),
              bw80_fraction=float(bw80(records).mean()), windows=len(rows))
    return pd.DataFrame(rows), qc


def _catalog():
    fixture = pd.DataFrame({**{f"agc_ch{c}": [50] * 64 for c in range(3)}, "wmi_cfo_raw": [0] * 64,
                            "wmi_rssi_dbm_ch0": [-50] * 64, "wmi_rssi_dbm_ch1": [-55] * 64,
                            "kernel_ts_s": np.arange(64) * 0.05, "chip_tsf_us": np.arange(64) * 50000})
    amplitude_features(np.ones((64, 3, 208)) * 100, fixture)
    return [k for k, v in AMP_CATALOG.items() if v["family"] != "nuisance"]


AMP_FEATURES = _catalog()
assert len(AMP_FEATURES) == 84, len(AMP_FEATURES)


# ---------------------------------------------------------------------------------------------------------
# Feature CHUYỂN ĐỘNG (ghi chú dự án "Quyết định về feature"): không dùng level/shape tĩnh.
# Mỗi record chuẩn hoá theo trung bình tone của chain (bỏ thang AGC), nội suy theo TSF lên lưới đều FS Hz,
# cửa sổ theo thời gian MOTION_WINDOW_S trượt MOTION_HOP_S. Tần số tính bằng Hz thật.
FS = 20.0
MOTION_WINDOW_S = 3.2
MOTION_HOP_S = 0.8
# cửa sổ thời gian (motion/inv) nội suy theo TSF nên chịu được gap dài hơn: 300 ms → 2–4× số cửa sổ trên link yếu
# 11.1→4.1 (2026-10-20: 14–38 → 49–60 cửa sổ/lượt 60 s). Daemon C: WIN_MAX_GAP_US — đổi cả hai.
TIME_MAX_GAP_US = 300000
MIN_COVERAGE = 0.7  # cần ≥ 70 % số record kỳ vọng (chu kỳ 51.2 ms) trong cửa sổ
BANDS = {"slow": (0.3, 1.0), "wave": (1.0, 4.0), "fast": (4.0, 10.0)}


def _band_features(power, freq, prefix, f):
    total = power[freq > 0].sum() + EPS
    f[f"{prefix}_log_power"] = float(np.log(total))
    for name, (lo, hi) in BANDS.items():
        f[f"{prefix}_{name}_fraction"] = float(power[(freq >= lo) & (freq < hi)].sum() / total)


def motion_features(a, t_s, t0=None):
    """a: n x chain x 208 biên độ (3 chain ở 11.1, 5 ở 4.1), t_s: thời điểm (s, tăng dần) từng record; t0: đầu cửa sổ (mặc định t_s[0]).
    Ngoài khoảng có record, np.interp giữ giá trị biên."""
    a = np.asarray(a, np.float64)
    nc = a.shape[1]
    a = a / (a.mean(axis=2, keepdims=True) + EPS)  # bỏ thang AGC/gain theo từng record, từng chain
    grid = (t_s[0] if t0 is None else t0) + np.arange(int(round(MOTION_WINDOW_S * FS))) / FS
    flat = a.reshape(len(a), -1)
    g = np.empty((len(grid), flat.shape[1]))
    for k in range(flat.shape[1]):
        g[:, k] = np.interp(grid, t_s, flat[:, k])
    g = g.reshape(len(grid), nc, 208)
    # khử xu hướng tuyến tính theo thời gian từng tone: drift chậm (người dịch chỗ, nhiệt) lấn át PCA/phổ
    tc = np.arange(len(grid)) - (len(grid) - 1) / 2
    d = g - g.mean(axis=0)
    d = d - tc[:, None, None] * (np.tensordot(tc, d, axes=(0, 0)) / np.sum(tc * tc))[None]
    freq = np.fft.rfftfreq(len(grid), 1 / FS)
    hann = np.hanning(len(grid))
    f = {}
    for c in range(nc):
        f[f"motion_std_c{c}"] = float(d[:, c].std(axis=0).mean())
        f[f"motion_step_c{c}"] = float(np.abs(np.diff(g[:, c], axis=0)).mean())
        u = g[:, c] / (np.linalg.norm(g[:, c], axis=1, keepdims=True) + EPS)
        f[f"motion_cos_step_c{c}"] = float(np.mean(1 - np.sum(u[1:] * u[:-1], axis=1)))
        power = (np.abs(np.fft.rfft(d[:, c] * hann[:, None], axis=0)) ** 2).mean(axis=1)
        _band_features(power, freq, f"motion_c{c}", f)
    # thành phần chính của biến thiên trên mọi chain: chuyển động cơ thể thường gom vào vài thành phần
    m = d.reshape(len(grid), -1)
    u, s, _ = np.linalg.svd(m, full_matrices=False)
    var = s ** 2
    f["motion_pc1_ratio"] = float(var[0] / (var.sum() + EPS))
    f["motion_pc3_ratio"] = float(var[:3].sum() / (var.sum() + EPS))
    pc1 = u[:, 0] * s[0]
    power = np.abs(np.fft.rfft(pc1 * hann)) ** 2
    _band_features(power, freq, "motion_pc1", f)
    f["motion_pc1_peak_hz"] = float(freq[1:][np.argmax(power[1:])])
    for lag_s in (0.25, 0.5, 1.0):
        lag = int(round(lag_s * FS))
        x, y = pc1[:-lag], pc1[lag:]
        f[f"motion_pc1_acf_{lag_s}s"] = float(np.sum(x * y) / (np.linalg.norm(x) * np.linalg.norm(y) + EPS))
    return f


def motion_windows(folder, chains=3):
    """Như recording_windows nhưng cửa sổ thời gian trượt và feature motion; dùng `chains` chain đầu
    (None = tất cả). Mọi lượt trong một lần train phải cùng số chain → tên feature giống nhau."""
    return _time_windows(folder, chains, motion_features, iq=False)


def _time_windows(folder, chains, fn, iq):
    folder = Path(folder)
    label = json.loads((folder / "label.json").read_text())
    records, a = load_recording(folder, iq=iq)
    assert chains is None or a.shape[1] >= chains, f"{folder.name} chỉ có {a.shape[1]} chain"
    a = a[:, :chains]
    tsf = records.chip_tsf_us.to_numpy(np.int64)
    expected = MOTION_WINDOW_S / 0.0512
    rows = []
    for piece in segments(records, qc_keep(records), TIME_MAX_GAP_US):
        t = (tsf[piece] - tsf[piece[0]]) / 1e6
        start = 0.0
        while start + MOTION_WINDOW_S <= t[-1] + 1e-9:
            sel = (t >= start) & (t < start + MOTION_WINDOW_S + 1e-9)
            if sel.sum() >= MIN_COVERAGE * expected:
                idx = piece[sel]
                f = fn(a[idx], t[sel], start)
                rows.append(dict(recording_id=folder.name, window=len(rows), start_tsf_us=int(tsf[idx[0]]),
                                 label=label.get("label"), session=label.get("session", ""),
                                 position=label.get("position", ""), person=label.get("person", ""),
                                 room=label.get("room") or "", board=label.get("board", "192.168.11.1"), **f))
            start += MOTION_HOP_S
    return pd.DataFrame(rows)


def motion_feature_names(chains=3):
    return list(motion_features(np.ones((64, chains, 208)), np.arange(64) * 0.05))


def motion_core_names(chains=3):
    """Bộ nhỏ cho khi ít lượt: năng lượng biến thiên, bước thay đổi, công suất — tách rõ nhất trên 4 lượt đầu."""
    return [f"{n}_c{c}" for c in range(chains) for n in ("motion_std", "motion_step")] + \
           [f"motion_c{c}_log_power" for c in range(chains)]


MOTION_FEATURES = motion_feature_names(3)
MOTION_CORE = motion_core_names(3)


# ---------------------------------------------------------------------------------------------------------
# Feature BẤT BIẾN MÔI TRƯỜNG ("inv", ghi chú dự án 2026-10-20): chỉ mô tả độ BIẾN THIÊN theo thời gian, không mô tả
# hình dạng/mức kênh tĩnh (vân tay phòng). Port 1-1 sang router/wifi_sensing.c (inv_features) — sửa cả hai.
#   inv_std_c / inv_step_c        biên độ chuẩn hoá theo record (như motion_std/step)
#   inv_tone_p90_c / _conc_c      độ nhạy subcarrier: phân vị 90 % độ biến thiên theo tone, và p90 / trung vị
#                                 (tay chỉ làm động một số tone — trung bình 208 tone làm loãng)
#   inv_ratio_{step,std,amp}_0k   CSI-ratio H0·conj(Hk): pha chung ngẫu nhiên (CFO/STO) triệt tiêu giữa 2 anten
#                                 cùng chip (FarSense, MobiCom 2019); bước pha, độ lệch pha vòng, std log tỉ số biên độ
# nb_<feature> = log(f / f_nền), nền = phân vị 20 % của f lúc phòng yên (lượt "calib", daemon: --calibrate).

def _interp_cols(grid, t_s, x):
    return np.stack([np.interp(grid, t_s, x[:, k]) for k in range(x.shape[1])], axis=1)


def _detrend(g):
    """g: grid x tone → bỏ trung bình + xu hướng tuyến tính theo thời gian từng tone (như motion_features)."""
    tc = np.arange(len(g)) - (len(g) - 1) / 2
    d = g - g.mean(axis=0)
    return d - tc[:, None] * (tc @ d / np.sum(tc * tc))[None]


def _detrended_tone_std(g):
    return _detrend(g).std(axis=0)


# Dải tần (2026-10-20): thở mạnh 0.2–0.6 Hz làm feature tổng năng lượng (std/step/ratio) vượt nền → báo nhầm "vẫy".
# Phổ 5 lượt: vẫy tăng năng lượng 1–4 Hz ~5–10×; pha CSI-ratio 76–81 % năng lượng < 0.7 Hz ở CẢ HAI lớp.
# → feature chỉ lấy dải vẫy, và tỉ số dải vẫy / dải thở. Cửa sổ 64 điểm @20 Hz: bin k = k·0.3125 Hz.
BREATH_BAND = (0.1, 0.7)   # bin 1–2
WAVE_BAND = (1.0, 4.0)     # bin 4–12
FAST_BAND = (4.0, 10.0)    # bin 13–31: tay giật nhanh có thành phần này, lồng ngực khi thở thì không
                           # (thở mạnh 171141: fast_fraction 0.05–0.06 vs vẫy 0.10–0.12, AUC 0.83)


def _band_powers(d):
    """d: grid x tone (đã khử xu hướng) → (P dải vẫy, P dải thở, P dải nhanh, P tổng bin ≥ 1), TB theo tone, Hann."""
    n = len(d)
    power = (np.abs(np.fft.rfft(d * np.hanning(n)[:, None], axis=0)) ** 2).mean(axis=1)
    freq = np.fft.rfftfreq(n, 1 / FS)
    band = lambda b: float(power[(freq >= b[0]) & (freq < b[1])].sum())
    return band(WAVE_BAND), band(BREATH_BAND), band(FAST_BAND), float(power[freq > 0].sum())


def invariant_features(h, t_s, t0=None, win_s=MOTION_WINDOW_S):
    """h: n x chain x 208 CSI phức, t_s: thời điểm (s) từng record, t0: đầu cửa sổ. win_s khác 3.2 s chỉ cho hiển thị
    (viz/): daemon + model dùng 3.2 s."""
    h = np.asarray(h, np.complex128)
    nc = h.shape[1]
    grid = (t_s[0] if t0 is None else t0) + np.arange(int(round(win_s * FS))) / FS
    amp = np.abs(h)
    f = {}
    for c in range(nc):
        a = amp[:, c] / (amp[:, c].mean(axis=1, keepdims=True) + EPS)
        g = _interp_cols(grid, t_s, a)
        d = _detrend(g)
        s = d.std(axis=0)
        p90, med = np.percentile(s, 90), np.percentile(s, 50)
        f[f"inv_std_c{c}"] = float(s.mean())
        f[f"inv_step_c{c}"] = float(np.abs(np.diff(g, axis=0)).mean())
        f[f"inv_tone_p90_c{c}"] = float(p90)
        f[f"inv_tone_conc_c{c}"] = float(p90 / (med + EPS))
        pw, pb, pf, pt = _band_powers(d)
        f[f"inv_hf_c{c}"] = pw
        f[f"inv_hfrac_c{c}"] = pw / (pb + EPS)
        f[f"inv_ffrac_c{c}"] = pf / (pt + EPS)
    for k in range(1, nc):
        q = h[:, 0] * np.conj(h[:, k])
        u = q / (np.abs(q) + EPS)
        ug = _interp_cols(grid, t_s, u.real) + 1j * _interp_cols(grid, t_s, u.imag)
        ug = ug / (np.abs(ug) + EPS)
        f[f"inv_ratio_step_0{k}"] = float(np.abs(np.angle(ug[1:] * np.conj(ug[:-1]))).mean())
        r = np.clip(np.abs(ug.mean(axis=0)), 1e-9, 1.0)
        f[f"inv_ratio_std_0{k}"] = float(np.sqrt(-2 * np.log(r)).mean())
        lr = np.log(amp[:, 0] + 1) - np.log(amp[:, k] + 1)
        f[f"inv_ratio_amp_0{k}"] = float(_interp_cols(grid, t_s, lr).std(axis=0).mean())
        ph = np.angle(ug * np.conj(ug.mean(axis=0)))  # pha lệch quanh hướng trung bình của tone (không unwrap)
        pw, pb, _, _ = _band_powers(_detrend(ph))
        f[f"inv_ratio_hf_0{k}"] = pw
        f[f"inv_ratio_hfrac_0{k}"] = pw / (pb + EPS)
    return f


def invariant_windows(folder, chains=3):
    return _time_windows(folder, chains, invariant_features, iq=True)


def invariant_names(chains=3):
    rng = np.random.default_rng(0)
    h = rng.normal(size=(64, chains, 208)) + 1j * rng.normal(size=(64, chains, 208)) + 3
    return list(invariant_features(h, np.arange(64) * 0.05))


def baseline_names(chains=3):
    return ["nb_" + n for n in invariant_names(chains)]


def ratio_names(chains=3):
    """Bộ "inv_ratio" (chuyển thiết bị tốt nhất, 2026-10-21): chỉ pha CSI-ratio H0·conj(Hk) — năng lượng dải vẫy (chuẩn
    hoá theo nền) + tỉ số dải vẫy/dải thở (không thang đo). Train 4.1 → test 11.1 AUC 0.84, ngược lại 0.76 (inv_hf: 0.69/0.70)."""
    return [f"nb_inv_ratio_hf_0{k}" for k in range(1, chains)] + [f"inv_ratio_hfrac_0{k}" for k in range(1, chains)]


def band_names(chains=3):
    """Bộ "inv_hf": năng lượng dải vẫy 1–4 Hz (tách yên) + tỉ lệ dải nhanh 4–10 Hz (tách thở mạnh) mỗi chain, chuẩn hoá
    theo nền. LORO 6 lượt (có 1 lượt thở mạnh): 4/6 vs inv_nb 1/6, inv_hf cũ (hf+hfrac+ratio) 1/6 (2026-10-20)."""
    return [f"nb_inv_{k}_c{c}" for c in range(chains) for k in ("hf", "ffrac")]


BASE_PCT = 20  # nền = phân vị 20 % (không phải trung vị): người cử động một lúc khi hiệu chỉnh không đẩy nền lên
               # (2026-10-20: nền trung vị lúc hiệu chỉnh 0.215 > mức vẫy 0.13–0.16 → model không bao giờ báo)


def apply_baseline(w, cols, calib=None):
    """Thêm cột nb_<c> = log(c / nền). Nền theo nhóm (board, room, session) = phân vị BASE_PCT các cửa sổ của lượt
    "calib" cùng nhóm; nhóm không có calib thì dùng cửa sổ no_wave của các lượt KHÁC trong nhóm
    (shortcut: no_wave có thể có người cử động → nền cao; thu lượt calib phòng yên mỗi phiên để bỏ fallback).
    Lượt không có nền bị bỏ. Trả (w có cột nb_*, bảng nguồn nền theo lượt)."""
    calib = pd.DataFrame(columns=w.columns) if calib is None else calib
    out, src = [], []
    key = ["board", "room", "session"]
    for rid, part in w.groupby("recording_id", sort=True):
        k = tuple(part[key].iloc[0])
        cal = calib[(calib[key] == pd.Series(k, index=key)).all(axis=1)] if len(calib) else calib
        how = "calib"
        if not len(cal):
            same = (w[key] == pd.Series(k, index=key)).all(axis=1)
            cal = w[same & (w.label != "wave") & (w.recording_id != rid)]
            how = "no_wave khác"
        if not len(cal):
            src.append(dict(recording_id=rid, baseline="KHÔNG CÓ (bỏ lượt)", windows=0))
            continue
        b = cal[cols].quantile(BASE_PCT / 100)  # = np.percentile tuyến tính, như daemon C
        part = part.copy()
        for c in cols:
            part["nb_" + c] = np.log((part[c] + EPS) / (b[c] + EPS))
        out.append(part)
        src.append(dict(recording_id=rid, baseline=how, windows=len(cal)))
    return (pd.concat(out, ignore_index=True) if out else w.iloc[:0]), pd.DataFrame(src)
