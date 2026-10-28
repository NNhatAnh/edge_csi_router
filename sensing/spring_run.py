"""Run the SPRING+ Python port on existing RDP499 captures, one ACK block at a time.

No router access, capture, UI writes or physical-position output. All RF assumptions
are explicit CLI inputs, and absolute_aoa_valid remains false in this experiment.
"""
import argparse
from dataclasses import asdict
import json
from pathlib import Path
import time

import numpy as np
import pandas as pd

import features as F
from spring_aoa import C, SOURCE_COMMIT, Config, Estimator, first_path_aoa


def load_capture(folder):
    """Strict existing CSV layout, including refusal of uncorrelated/other-size rows."""
    folder = Path(folder)
    label = json.loads((folder / "label.json").read_text())
    if label.get("exit_code") != 0 or label.get("board") != "192.168.11.1":
        raise ValueError("Expected a successful RDP499 capture")
    csi = pd.read_csv(folder / "export_csi/records.csv", low_memory=False)
    meta = pd.read_csv(folder / "export/records_meta.csv", low_memory=False)
    meta_keys = ["chip_tsf_us", "phy_ppdu_id"]
    if meta.duplicated(meta_keys).any() or not csi.record.is_unique:
        raise ValueError("Ambiguous CSI/metadata keys")
    original_count = len(csi)
    csi["phy_ppdu_id"] = csi.phy_ppdu_id.map(lambda x: int(str(x), 0) if str(x).startswith("0x") else int(x))
    shape_ok = csi.num_chains.eq(3) & csi.tones_per_chain.eq(212) & csi.samples_present.eq(636) & csi.samples_missing.eq(0)
    current = abs(csi.kernel_ts_s - csi.kernel_ts_s.median()) < 3600
    csi = csi.loc[shape_ok & current].copy()
    csi = csi.join(meta.set_index(meta_keys), on=meta_keys, rsuffix="_meta", validate="many_to_one")
    ok = (csi.wmi_peer_mac.str.lower().eq(label["sta"].lower()).to_numpy()
          & F.is_true(csi.wmi_found) & F.is_true(csi.wmi_capture_ok) & csi.wmi_bw.eq(2).to_numpy()
          & csi[[f"wmi_gain_tbl_ch{i}" for i in range(3)]].eq(0).all(axis=1).to_numpy()
          & ~csi.duplicated(meta_keys).to_numpy())
    selected = csi.loc[ok].sort_values("record")
    if selected.empty:
        raise ValueError("No packets satisfy MAC/WMI/BW80/all-chain gain0 QC")
    iq = pd.read_csv(folder / "export_csi/csi_iq.csv", usecols=["record", "chain", "tone_index", "i", "q", "present"])
    iq = iq.loc[iq.record.isin(selected.record)].sort_values(["record", "chain", "tone_index"])
    checks = [F.is_true(iq.present).all(),
              np.array_equal(iq.record.to_numpy(), np.repeat(selected.record.to_numpy(), 636)),
              np.array_equal(iq.chain.to_numpy(), np.tile(np.repeat(np.arange(3), 212), len(selected))),
              np.array_equal(iq.tone_index.to_numpy(), np.tile(np.arange(212), 3*len(selected)))]
    if not all(checks):
        raise ValueError("Incomplete or reordered CSI samples")
    h = (iq.i.to_numpy(float) + 1j*iq.q.to_numpy(float)).reshape(-1, 3, 212)
    if not np.isfinite(h).all():
        raise ValueError("Non-finite I/Q")
    order = np.argsort(selected.chip_tsf_us.to_numpy(), kind="stable")
    selected, h = selected.iloc[order].reset_index(drop=True), h[order]
    frequencies = selected.wmi_center_freq1.unique()
    if len(frequencies) != 1 or not np.isfinite(frequencies[0]) or frequencies[0] <= 1000:
        raise ValueError("Mixed/invalid center frequency")
    return selected, h, dict(raw_records=original_count, qc_packets=len(h), center_frequency_hz=float(frequencies[0])*1e6,
                            peer_mac=label["sta"], distance_reported=label.get("distance"), exclude_from_gesture_training=label.get("exclude"))


def run_recording(folder, out, spacing, tone_spacing):
    records, h, qc = load_capture(folder)
    folder, out = Path(folder), Path(out) / Path(folder).name
    out.mkdir(parents=True, exist_ok=True)
    indices = tuple(np.r_[np.arange(-26, 0), np.arange(1, 27)].tolist())
    config = Config(qc["center_frequency_hz"], spacing, indices, tone_spacing_hz=tone_spacing)
    rows, blocks = [], []
    start = time.monotonic()
    for block in range(4):
        estimator = Estimator(config, 3)
        keep = np.arange(53) != 26
        paths, failed = [], []
        first_spectrum = None
        for packet, channel in enumerate(h[:, :, block*53:(block+1)*53][:, :, keep]):
            try:
                result = estimator.estimate_packet(channel)
            except (ValueError, ArithmeticError, np.linalg.LinAlgError) as error:
                failed.append(dict(packet=packet, record=int(records.record.iloc[packet]), error=str(error)))
                continue
            if first_spectrum is None:
                first_spectrum = result["spectrum"]
            for path, (delay, angle, correlation) in enumerate(result["paths"]):
                rows.append(dict(packet=packet, record=int(records.record.iloc[packet]),
                                 chip_tsf_us=int(records.chip_tsf_us.iloc[packet]), block=block, path=path,
                                 relative_delay_ns=delay, model_angle_deg=angle, correlation=correlation,
                                 mpm_paths=result["mpm_paths"], detected_paths=result["detected_paths"],
                                 smoothing_index=result["smoothing_index"], k=result["k"], j=result["j"]))
            paths.extend(result["paths"])
        cluster = first_path_aoa(np.asarray(paths).reshape(-1, 3), config)
        blocks.append(dict(block=block, processed_packets=len(h)-len(failed), failed_packets=failed,
                           numerical_cluster=cluster, absolute_aoa_valid=False))
        if first_spectrum is not None:
            np.savez_compressed(out / f"block_{block}_first_spectrum.npz", spectrum=first_spectrum,
                                delay_ns=estimator.delays*1e9, angle_deg=estimator.angles)
        print(json.dumps(dict(run=folder.name, block=block, packets=len(h)-len(failed),
                              cluster=cluster["angle_deg"], seconds=round(time.monotonic()-start,2))), flush=True)
    pd.DataFrame(rows).to_csv(out / "paths.csv", index=False)
    summary = dict(method="spring_plus_block_experimental", source_commit=SOURCE_COMMIT,
                   recording=folder.name, qc=qc, config=asdict(config), blocks=blocks,
                   absolute_aoa_valid=False, calibration_id=None,
                   assumptions=dict(geometry="assumed ULA, chain order 0/1/2", spacing_m=spacing,
                                    iq_convention="assumed I+jQ", phase_calibration="identity, not measured",
                                    tone_mapping="local block slots -26..26 except DC; frequency spacing/sign assumed",
                                    block_coherence="not assumed; no cross-block delay steering",
                                    far_field_at_reported_distance="not verified"),
                   elapsed_seconds=time.monotonic()-start)
    (out / "summary.json").write_text(json.dumps(summary, indent=2, ensure_ascii=False, allow_nan=False))
    return summary


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("recordings", nargs="+", type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--spacing-m", required=True, type=float, help="Explicit assumed ULA spacing, not an inferred board measurement")
    ap.add_argument("--tone-spacing-hz", required=True, type=float)
    ap.add_argument("--assume-uncalibrated-ula", required=True, action="store_true",
                    help="Acknowledge unknown geometry/calibration; outputs cannot be physical AoA")
    args = ap.parse_args()
    summaries = [run_recording(f, args.out, args.spacing_m, args.tone_spacing_hz) for f in args.recordings]
    (args.out / "results.json").write_text(json.dumps(summaries, indent=2, ensure_ascii=False, allow_nan=False))


if __name__ == "__main__":
    main()
