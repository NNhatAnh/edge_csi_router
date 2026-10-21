#!/usr/bin/env python3
"""Re-run every field cross-check behind RDP499_CFR_READING_GUIDE.md on one or more decoded runs.

Usage: cfr_validate_fields.py <run dir> [<run dir> ...]

Each run dir holds export/records_meta.csv + export/wmievt_raw.json (cfr_relay_meta.py) and
export_csi/records.csv + export_csi/csi_iq.csv (cfr_export.py), from firmware 01618 + driver
0006, i.e. what cfr_capture_pull.sh leaves. For each run, and for all runs pooled, it prints:

  V1  CFO: WMI event raw == PHY TLV raw                  (-> the CFO unit applies to cfo_raw)
  V2  AGC: WMI agc_gain_index == PHY agc_ch*             (-> agc_ch* is the LUT index)
  V3  RSSI: WMI dBm - PHY dB-over-NF, chains 0/1         (constant -96 = a nominal NF, not measured)
  V4  chain 2 WMI RSSI value                             (-96 = 0 dB = not measured)
  V5  chip_tsf_us - wmi_ts_us                            (the two clocks are one TSF)
  V6  firmware counter continuity, failure causes, OK captures without CSI
  V7  gain table per packet, and equal on all chains
  V8  chain power vs the PHY's per-chain RSSI (chain 1 - chain 0): raw CSI, CSI x 128/ratio,
      CSI x 128/ratio - AGC index (model B), split by "both ratios 128" and by gain table
Standard library only. The numbers in the guide were produced by exactly these computations.
"""
import csv
import json
import math
import os
import statistics as st
import sys
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cfr_relay_meta  # noqa: E402


def load(run):
    meta = list(csv.DictReader(open(f"{run}/export/records_meta.csv", encoding="utf-8")))
    recs = list(csv.DictReader(open(f"{run}/export_csi/records.csv", encoding="utf-8")))
    if len(meta) != len(recs) or any(int(r["phy_ppdu_id"], 16) != int(m["phy_ppdu_id"])
                                     for r, m in zip(recs, meta)):
        raise SystemExit(f"{run}: records.csv and records_meta.csv do not line up")
    raw = json.load(open(f"{run}/export/wmievt_raw.json", encoding="utf-8"))
    events = [cfr_relay_meta.decode_wmi(bytes.fromhex(h)) for h in raw.values()]
    pw = defaultdict(lambda: [0.0, 0])
    for row in csv.DictReader(open(f"{run}/export_csi/csi_iq.csv", encoding="utf-8")):
        if row["present"] == "1":
            k = (int(row["record"]), int(row["chain"]))
            pw[k][0] += float(row["amplitude"]) ** 2
            pw[k][1] += 1
    rows = []
    for i, (r, m) in enumerate(zip(recs, meta), 1):
        if m.get("wmi_found") != "True":
            continue
        x = dict(m)
        x["g"] = [int(r[f"amp_gain_ratio_ch{c}"]) for c in range(3)]
        x["p"] = [pw[(i, c)][0] / pw[(i, c)][1] if pw[(i, c)][1] else 0.0 for c in range(3)]
        rows.append(x)
    return rows, events, len(meta)


def iqr(v):
    q = st.quantiles(v, n=4)
    return q[2] - q[0]


def check(name, rows, events, n_csi):
    def eq(a, b):
        pairs = [(r[a], r[b]) for r in rows if r.get(a) not in (None, "") and r.get(b) not in (None, "")]
        return sum(float(x) == float(y) for x, y in pairs), len(pairs)

    print(f"===== {name}: {n_csi} CSI records, {len(rows)} with their WMI event, {len(events)} WMI events")
    k, n = eq("wmi_cfo_raw", "cfo_raw")
    print(f"V1 CFO WMI == PHY              {k}/{n}")
    k = sum(all(r[f"wmi_agc_idx_ch{c}"] == r[f"agc_ch{c}"] for c in range(3)) for r in rows)
    print(f"V2 AGC index WMI == PHY        {k}/{len(rows)} (all three chains)")
    d = Counter()
    for r in rows:
        for c in range(2):
            phy = r[f"rssi_ch{c}_per20_db"].split()[0]
            if r.get(f"wmi_rssi_dbm_ch{c}") and phy not in ("--", ""):
                d[int(r[f"wmi_rssi_dbm_ch{c}"]) - int(phy)] += 1
    print(f"V3 WMI dBm - PHY dB/NF         {dict(d)}")
    print(f"V4 chain 2 WMI RSSI            {dict(Counter(r.get('wmi_rssi_dbm_ch2') for r in rows))}")
    dt = Counter(int(r["chip_tsf_us"]) - int(r["wmi_ts_us"]) for r in rows if r.get("chip_tsf_us"))
    print(f"V5 chip_tsf - wmi_ts (us)      {dict(dt.most_common(3))}")
    ok = sorted(e["wmi_counter"] for e in events if e.get("wmi_capture_ok"))
    gaps = sum(1 for a, b in zip(ok, ok[1:]) if b != a + 1)
    fail = Counter((e.get("wmi_tx_status"), e.get("wmi_ps_fail")) for e in events if e.get("wmi_capture_ok") is False)
    print(f"V6 counter gaps {gaps}; failed events (tx_status, ps_fail) {dict(fail)}; OK captures without CSI {len(ok) - n_csi}")
    same = sum(r["wmi_gain_tbl_ch0"] == r["wmi_gain_tbl_ch1"] == r["wmi_gain_tbl_ch2"] for r in rows)
    print(f"V7 gain table {dict(Counter(r['wmi_gain_tbl_ch0'] for r in rows))}, equal on all chains {same}/{len(rows)}")

    res = defaultdict(list)
    for r in rows:
        if min(r["p"][:2]) <= 0:
            continue
        dr = int(r["rssi_ch1_per20_db"].split()[0]) - int(r["rssi_ch0_per20_db"].split()[0])
        raw = 10 * math.log10(r["p"][1] / r["p"][0])
        nrm = raw + 20 * math.log10(128 / r["g"][1]) - 20 * math.log10(128 / r["g"][0])
        b = nrm - (int(r["agc_ch1"]) - int(r["agc_ch0"]))
        grp = "both ratios 128" if r["g"][:2] == [128, 128] else "a ratio != 128"
        for model, v in (("raw CSI", raw), ("x128/ratio", nrm), ("model B", b)):
            res[(model, grp)].append(dr - v)
            res[(model, "all")].append(dr - v)
        res[("model B", f"gain table {r['wmi_gain_tbl_ch0']}")].append(dr - b)
    print("V8 (RSSI_1 - RSSI_0) - (chain power difference): median / IQR / n")
    for (model, grp), v in sorted(res.items()):
        if len(v) >= 4:
            print(f"     {model:11s} {grp:16s} {st.median(v):+6.2f} / {iqr(v):5.2f} dB / {len(v)}")


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    pool_rows, pool_events, pool_n = [], [], 0
    for run in sys.argv[1:]:
        rows, events, n = load(run)
        check(os.path.basename(os.path.normpath(run)), rows, events, n)
        pool_rows += rows
        pool_events += events
        pool_n += n
    if len(sys.argv) > 2:
        # pooled: V6 counters are per run, so only the record-level checks are meaningful here
        check("ALL RUNS POOLED (V6 not meaningful)", pool_rows, pool_events, pool_n)


if __name__ == "__main__":
    main()
