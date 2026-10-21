#!/usr/bin/env python3
"""Estimate chain 2's RSSI, which neither the PHY TLVs nor the WMI event report, from the CSI.

Usage: cfr_chain_rssi_est.py <export_csi dir> <records_meta.csv> <out.csv>

<export_csi dir> is cfr_export.py's output for a relay file (records.csv, csi_iq.csv), and
records_meta.csv is cfr_relay_meta.py's for the SAME relay file (same record order; checked by
phy_ppdu_id).

The PHY reports per-chain RSSI (dB over the noise floor, primary 20 MHz) for chains 0 and 1 only.
Two models predict a chain's RSSI from another chain's and the CSI itself:
  A  rssi_c = rssi_0 + 10 log10(P_c / P_0) + offset_A           P = mean |H|^2 over present tones
  B  rssi_c = rssi_0 + 10 log10(P'_c / P'_0) - (agc_c - agc_0) + offset_B
                                                   P' = P (128 / amp_gain_ratio_c)^2, agc = LUT index
Both offsets are FITTED ON THIS DATA SET by predicting chain 1 from chain 0, where the truth is
known, and the residual of that fit is printed as the validation error. They are then applied
to chain 2. Chain 2 itself has no ground truth, so an estimate is only as good as the
assumption that chain 2 behaves like chain 1 relative to chain 0; the A/B disagreement per
record is written next to it as a warning sign.

est_rssi_ch2_db is MODEL B. Measured 2026-10-05, chain-1 validation |error| median:

| Data set | A | B |
|---|---|---|
| 11.1 ch 36, 375 + 182 records | 0.58 / 0.62 dB | 0.45 / 0.44 dB |
| 11.1 ch 161, 2209 records | 0.39 dB | 0.41 dB |
| 5.1 ch 48, 1003 records | **1.10 dB** | 0.49 dB |
| 10.1 ch 165 (20 MHz BSS), 1142 records | 0.71 dB | 0.71 dB |

B was never worse than A on chain 1. On chain 2 the two models disagree by 0.2-2.9 dB (median,
2.87 on 10.1), so chain 2's estimate is good to about +-3 dB, not to the chain-1 figure.
Standard library only.
"""
import csv
import math
import statistics as st
import sys
from collections import defaultdict


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    csi_dir, meta_csv, out_csv = sys.argv[1:]
    meta = list(csv.DictReader(open(meta_csv, encoding="utf-8")))
    recs = list(csv.DictReader(open(f"{csi_dir}/records.csv", encoding="utf-8")))
    if len(meta) != len(recs) or any(int(r["phy_ppdu_id"], 16) != int(m["phy_ppdu_id"])
                                     for r, m in zip(recs, meta)):
        raise SystemExit("records.csv and records_meta.csv are not from the same relay file")

    pw = defaultdict(lambda: [0.0, 0])
    for row in csv.DictReader(open(f"{csi_dir}/csi_iq.csv", encoding="utf-8")):
        if row["present"] == "1":
            k = (int(row["record"]), int(row["chain"]))
            pw[k][0] += float(row["amplitude"]) ** 2
            pw[k][1] += 1

    rows = []
    for i, (r, m) in enumerate(zip(recs, meta), 1):
        try:
            r0 = int(m["rssi_ch0_per20_db"].split()[0])
            r1 = int(m["rssi_ch1_per20_db"].split()[0])
            agc = [int(m[f"agc_ch{c}"]) for c in range(3)]
            g = [int(r[f"amp_gain_ratio_ch{c}"]) for c in range(3)]
            p = [pw[(i, c)][0] / pw[(i, c)][1] for c in range(3)]
        except (KeyError, ValueError, ZeroDivisionError, IndexError):
            continue
        if min(p) <= 0 or min(g) <= 0:
            continue
        a = [10 * math.log10(x) for x in p]
        b = [10 * math.log10(p[c] * (128 / g[c]) ** 2) - agc[c] for c in range(3)]
        rows.append(dict(record=i, phy_ppdu_id=m["phy_ppdu_id"], chip_tsf_us=m.get("chip_tsf_us"),
                         r0=r0, r1=r1, a=a, b=b, gain_tbl=m.get("wmi_gain_tbl_ch0", "")))
    if len(rows) < 10:
        raise SystemExit(f"only {len(rows)} usable records")

    fits = {}
    for name in ("a", "b"):
        res = [(x["r1"] - x["r0"]) - (x[name][1] - x[name][0]) for x in rows]
        off = st.median(res)
        err = sorted(abs(v - off) for v in res)
        fits[name] = off
        print(f"model {name.upper()}: offset {off:+.2f} dB, chain-1 validation |error| median "
              f"{st.median(err):.2f} dB, p90 {err[int(0.9 * len(err))]:.2f} dB, n={len(rows)}")

    with open(out_csv, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["record", "phy_ppdu_id", "chip_tsf_us", "rssi_ch0_db", "rssi_ch1_db",
                    "est_rssi_ch2_db_A", "est_rssi_ch2_db_B", "est_rssi_ch2_db", "ab_spread_db",
                    "gain_tbl"])
        spread = []
        for x in rows:
            ea = x["r0"] + (x["a"][2] - x["a"][0]) + fits["a"]
            eb = x["r0"] + (x["b"][2] - x["b"][0]) + fits["b"]
            spread.append(abs(ea - eb))
            w.writerow([x["record"], x["phy_ppdu_id"], x["chip_tsf_us"], x["r0"], x["r1"],
                        f"{ea:.1f}", f"{eb:.1f}", f"{eb:.1f}", f"{abs(ea - eb):.1f}",
                        x["gain_tbl"]])
    print(f"chain 2 A/B spread median {st.median(spread):.2f} dB, p90 "
          f"{sorted(spread)[int(0.9 * len(spread))]:.2f} dB -> {out_csv}")


if __name__ == "__main__":
    main()
