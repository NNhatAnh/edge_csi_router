#!/usr/bin/env python3
"""Quality report for one decoded RDP499 capture: the checks of RDP499_CFR_READING_GUIDE.md §7.

Usage: cfr_quality_report.py <run dir>

<run dir> holds relay.bin plus the outputs of
  cfr_export.py     relay.bin <run>/export_csi
  cfr_relay_meta.py relay.bin <run>/export
(cfr_capture_pull.sh produces exactly that). Prints PASS / WARN / FAIL per check and exits 1 if
any check FAILs. Standard library only.
"""
import csv
import json
import os
import statistics as st
import sys
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cfr_relay_meta  # noqa: E402  (decode_wmi, same directory)

worst = 0


def report(level, name, msg):
    global worst
    worst = max(worst, {"PASS": 0, "WARN": 1, "FAIL": 2}[level])
    print(f"{level:4s}  {name:28s} {msg}")


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    run = sys.argv[1]
    meta = list(csv.DictReader(open(f"{run}/export/records_meta.csv", encoding="utf-8")))
    recs = list(csv.DictReader(open(f"{run}/export_csi/records.csv", encoding="utf-8")))
    n = len(meta)
    if n == 0:
        report("FAIL", "CSI records", "none: is the station awake (guide §16.6)?")
        sys.exit(1)

    # 1. every CSI record decoded completely and joined to its PHY metadata
    missing = sum(int(r.get("samples_missing") or 0) for r in recs)
    report("PASS" if missing == 0 else "FAIL", "samples", f"{missing} missing over {n} records")
    rx = sum(r["rxmeta_found"] == "True" for r in meta)
    report("PASS" if rx == n else "WARN", "PHY metadata matched", f"{rx}/{n}")

    # 2. one station
    peers = Counter(r["peer_mac"] for r in meta)
    report("PASS" if len(peers) == 1 else "WARN", "stations", ", ".join(f"{k} x{v}" for k, v in peers.items()))

    # 3. firmware event (01618 + driver 0006 only)
    wmi = sum(r.get("wmi_found") == "True" for r in meta)
    if wmi == 0:
        report("WARN", "firmware event", "none: r8-00065 or a driver older than 0006 (no wmi_* fields)")
    else:
        report("PASS" if wmi == n else "WARN", "firmware event matched", f"{wmi}/{n}")
        raw = json.load(open(f"{run}/export/wmievt_raw.json", encoding="utf-8"))
        ev = [cfr_relay_meta.decode_wmi(bytes.fromhex(h)) for h in raw.values()]
        ok = sorted(e["wmi_counter"] for e in ev if e.get("wmi_capture_ok"))
        fail = Counter(e.get("wmi_tx_status") for e in ev if e.get("wmi_capture_ok") is False)
        gaps = sum(1 for a, b in zip(ok, ok[1:]) if b != a + 1)
        report("PASS" if gaps == 0 else "WARN", "firmware counter gaps", f"{gaps} (captures lost inside the firmware)")
        lost = len(ok) - n
        report("PASS" if lost <= max(1, n // 500) else "WARN", "OK captures without CSI",
               f"{lost} of {len(ok)} (DMA buffer never arrived)")
        report("PASS", "failed captures", f"{sum(fail.values())} (tx_status {dict(fail)}: 1 = the station did not ACK)")
        gt = Counter(r.get("wmi_gain_tbl_ch0") for r in meta if r.get("wmi_found") == "True")
        report("PASS" if gt.get("0", 0) == wmi else "WARN", "gain table",
               f"{dict(gt)} (non-zero = RF gain reduced; drop those for cross-chain phase work)")

    # 4. time axis
    ts = sorted(int(r["chip_tsf_us"]) for r in meta if r.get("chip_tsf_us"))
    if len(ts) > 2:
        dt = [(b - a) / 1000 for a, b in zip(ts, ts[1:])]
        med = st.median(dt)
        big = sum(1 for d in dt if d > 2.5 * med)
        report("PASS" if big <= len(dt) // 50 else "WARN", "record interval",
               f"median {med:.1f} ms, p99 {sorted(dt)[int(0.99 * len(dt))]:.1f} ms, max {max(dt):.1f} ms, "
               f"{big} gaps > 2.5x median; rate {(len(ts) - 1) / ((ts[-1] - ts[0]) / 1e6):.2f}/s")

    # 5. what this run tells about the radio
    def med(col):
        v = [float(r[col]) for r in meta if r.get(col) not in (None, "")]
        return f"{st.median(v):.1f}" if v else "--"
    cfo = [float(r["cfo_raw"]) * cfr_relay_meta.CFO_HZ_PER_LSB for r in meta if r.get("cfo_raw") not in (None, "")]
    print(f"info  rssi_comb {med('rssi_comb_db')} dB/NF, agc idx ch0/1/2 {med('agc_ch0')}/{med('agc_ch1')}/"
          f"{med('agc_ch2')}, cfo {st.median(cfo) if cfo else float('nan'):.0f} Hz, "
          f"channel {Counter(r['center_freq1'] for r in meta).most_common(1)[0][0]} MHz")
    sys.exit(1 if worst == 2 else 0)


if __name__ == "__main__":
    main()
