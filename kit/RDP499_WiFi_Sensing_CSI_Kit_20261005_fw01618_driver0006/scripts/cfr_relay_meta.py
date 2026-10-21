#!/usr/bin/env python3
"""Decode a relay file written with the 0003 driver (CFR Rx metadata) into one CSV row per CSI
record, with its per-packet metadata.

Usage: cfr_relay_meta.py <relay.bin> <out dir>

The relay holds two kinds of record (driver_patch/0003-cfr-rx-phy-metadata.patch, cfr.c):
  CSI      u32 0xDEADBEAF, ath12k_csi_cfr_header (22 B) + cfr_enh_metadata (93 B, packed),
           the DMA buffer (wifi8 header 0xC0DE00BA + payload), u32 0xBEAFDEAD
  RX META  u32 0xCF4E7A01, u16 ppdu id, u8 pdev, u8 n, u64 host ns, u32 bytes,
           n x {u16 tag, u16 len, data padded to 4}, u32 0xBEAFDEAD
  WMI EVT  (driver 0006) u32 0xCF4E7A02, u16 ppdu id, u8 pdev, u8 version, u64 host ns,
           u32 bytes, WMI_PEER_CFR_CAPTURE_EVENT's TLVs as the firmware sent them, u32 0xBEAFDEAD
A CSI record is matched to its RX META record and to its WMI EVT record by the DMA header's
phy_ppdu_id. The header's metadata is filled by the driver when the PPDU had already been seen
(cfr_rxmeta_hit); the raw records are written either way, so this script decodes the fields
from them and also reports what the header carried.

WMI EVT fields follow the firmware's own wmi_unified.h (WLAN.WBO.1.0-01618,
qca-wifi-fw-src-component-cmn): wmi_peer_cfr_capture_event_fixed_param (tag 0x317) and
wmi_peer_cfr_capture_event_phase_fixed_param (tag 0x33b). r8-00065 sends the first one with
every Rx field 0 and never the second; 01618 fills both (phase stays 0xFFFF = invalid).

Writes <out>/records_meta.csv and <out>/rxmeta_raw.json. Standard library only. CSI samples
themselves: cfr_export.py on the same relay file.
"""
import csv
import json
import os
import struct
import sys

CSI_START, END, RXMETA, WIFI8_TAG = 0xDEADBEAF, 0xBEAFDEAD, 0xCF4E7A01, 0xC0DE00BA
WMIEVT = 0xCF4E7A02
TAG_WMI_FIXED, TAG_WMI_PHASE = 0x317, 0x33b
# cfo_measurement: bit 0 valid, bits 14:1 signed raw. One LSB = 1 / (2^13 * 2 * 800 ns) Hz,
# the reading of the firmware header's ppm formula that is dimensionally consistent with its
# stated +-40 ppm range (README section 4.11)
CFO_HZ_PER_LSB = 1.0 / (2 ** 13 * 2 * 800e-9)
LEGACY_MCS_MBPS = {0: 48, 1: 24, 2: 12, 3: 6, 4: 54, 5: 36, 6: 18, 7: 9}
GI_US = {0: 0.8, 1: 0.4, 2: 1.6, 3: 3.2}
HDR_LEN, META_LEN = 22, 93
TAG_NAMES = {132: "rssi_legacy", 135: "pkt_end", 139: "l_sig_a", 157: "other_rx_info",
             158: "cmn_user_info", 456: "location", 643: "rssi_legacy_part2"}


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def parse_meta_enh(b, o):
    m = {}
    m["status"], m["tx_pkt_bw"], m["phy_mode"] = b[o], b[o + 1], b[o + 2]
    m["center_freq1"], m["center_freq2"] = struct.unpack_from("<HH", b, o + 3)
    m["num_mu_users"] = b[o + 7]
    m["peer_mac"] = b[o + 8:o + 14].hex(":")
    p = o + 14
    cr = struct.unpack_from("<8I", b, p)
    m["hdr_chain_rssi"] = [f"0x{x:08x}" for x in cr[:3]]
    # 0005: chain_rssi[7] = 0xC0000000 | combined RSSI (dB over NF); anything else = not filled
    m["hdr_rssi_comb"] = (cr[7] & 0xFF) if (cr[7] >> 24) == 0xC0 else None
    p += 32
    m["hdr_chain_phase"] = list(struct.unpack_from("<8H", b, p)[:3])
    p += 16
    m["hdr_cfo"] = struct.unpack_from("<i", b, p)[0]
    p += 4
    m["hdr_agc"] = list(b[p:p + 3])
    p += 8
    m["hdr_rx_start_ts"] = u32(b, p)
    m["hdr_mcs_rate"], m["hdr_gi_type"] = struct.unpack_from("<HH", b, p + 4)
    # then u8 beamformed, u8 agc_gain_tbl_index[8], u16 puncture_bitmap (0005 fills it from
    # the vdev's channel when the record had no WMI event)
    m["hdr_puncture_bitmap"] = f"0x{u16(b, p + 8 + 1 + 8):04x}"
    return m


def decode_tlvs(tlv):
    """Field positions: RDP499_CSI_METADATA_GUIDE.md section 5."""
    f = {}
    r = tlv.get(132)
    if r and len(r) >= 38:
        f["rssi_comb_db"] = r[37]
        f["rx_bw"] = (u16(r, 0) >> 5) & 7
    k = tlv.get(135)
    if k and len(k) >= 56:
        f["rx_end_ticks"] = struct.unpack_from("<Q", k, 4)[0]
        f["rx_start_ticks"] = struct.unpack_from("<Q", k, 12)[0]
        f["airtime_us"] = round((f["rx_end_ticks"] - f["rx_start_ticks"]) / 960.0, 2)
        for c, off in enumerate((24, 40)):
            row = k[off:off + 16]
            # column 0 = primary 20 MHz (verified at ch 36 and ch 48), then ext20, ext40 low,
            # ext40 high; signed dB over the noise floor, 0x80 = invalid
            f[f"rssi_ch{c}_per20_db"] = " ".join(
                "--" if x == 0x80 else str(x - 256 if x > 127 else x) for x in row[:4])
        f["pkt_end_cfo_s32"] = struct.unpack_from("<i", k, 104)[0] if len(k) >= 108 else None
    loc = tlv.get(456)
    if loc and len(loc) >= 38:
        f["chain_mask"] = f"0x{loc[2]:02x}"
        f["capture_counter"] = loc[10]
        f["cfo_raw"] = struct.unpack_from("<h", loc, 16)[0]
        f["agc_ch0"], f["agc_ch1"], f["agc_ch2"] = loc[32], loc[34], loc[36]
    s = tlv.get(139)
    if s and len(s) >= 2:
        w = u16(s, 0)
        f["lsig_rate_code"] = w & 0xF
        f["lsig_length"] = (w >> 5) & 0xFFF
    return f


def s8(x):
    return x - 256 if x > 127 else x


def decode_wmi(raw):
    """The event's TLVs: header u32 = tag << 16 | len (bytes), value padded to 4."""
    f, p = {}, 0
    while p + 4 <= len(raw):
        hdr = u32(raw, p)
        tag, ln = hdr >> 16, hdr & 0xFFFF
        v = raw[p + 4:p + 4 + ln]
        p += 4 + ((ln + 3) & ~3)
        if tag == TAG_WMI_FIXED and len(v) >= 104:
            f["wmi_method"] = u32(v, 0)
            f["wmi_peer_mac"] = v[8:14].hex(":")
            f["wmi_prim20_mhz"] = u32(v, 16)
            f["wmi_bw"] = u32(v, 20)
            f["wmi_center_freq1"] = u32(v, 28)
            f["wmi_sts"] = u32(v, 36)
            st = u32(v, 48)
            f["wmi_status"] = f"0x{st:08x}"
            f["wmi_capture_ok"] = bool(st >> 31 & 1)
            f["wmi_ps_fail"] = bool(st >> 30 & 1)
            f["wmi_tx_status"] = st & 3
            f["wmi_ts_us"] = u32(v, 52)
            f["wmi_counter"] = u32(v, 56)
            for c in range(3):
                w = u32(v, 60 + 4 * c)
                bands = [s8(w >> (8 * k) & 0xFF) for k in range(4)]
                # pri20, sec20, sec40, sec80 in dBm; -128 = invalid
                f[f"wmi_rssi_dbm_ch{c}"] = bands[0] if bands[0] != -128 else None
                f[f"wmi_rssi_dbm_ch{c}_bands"] = " ".join(
                    "--" if x == -128 else str(x) for x in bands)
            cfo = u32(v, 92)
            if cfo & 1:
                raw14 = (cfo >> 1) & 0x3FFF
                raw14 = raw14 - 0x4000 if raw14 & 0x2000 else raw14
                f["wmi_cfo_raw"] = raw14
                f["wmi_cfo_hz"] = round(raw14 * CFO_HZ_PER_LSB, 1)
                if f["wmi_prim20_mhz"]:
                    f["wmi_cfo_ppm"] = round(f["wmi_cfo_hz"] / f["wmi_prim20_mhz"], 3)
            f["wmi_rx_start_480mhz"] = u32(v, 96)
            f["wmi_rx_ts_reset"] = u32(v, 100)
            # r8-00065 sends these words too, all 0 (mcs 0 would read as 48 Mb/s)
            if len(v) >= 108 and f["wmi_capture_ok"]:
                mg = u32(v, 104)
                f["wmi_mcs"] = mg & 0xFF
                f["wmi_rate_mbps"] = LEGACY_MCS_MBPS.get(mg & 0xFF)
                f["wmi_gi_us"] = GI_US.get(mg >> 8 & 0xFF)
            if len(v) >= 112:
                sc = u32(v, 108)
                f["wmi_seq_ctrl_valid"] = bool(st >> 29 & 1)
                f["wmi_seq_num"], f["wmi_frag_num"] = sc >> 4 & 0xFFF, sc & 0xF
        elif tag == TAG_WMI_PHASE and len(v) >= 40:
            for c in range(3):
                ph = u32(v, 4 * c) & 0xFFFF
                # 0-1023 maps 0-360 degrees; 0xFFFF = invalid chain
                f[f"wmi_phase_ch{c}_deg"] = None if ph == 0xFFFF else round(ph * 360 / 1024, 2)
                f[f"wmi_agc_idx_ch{c}"] = v[32 + c]
                if len(v) >= 48:
                    # 0 default gain table, 1 low RF gain, 2 very low RF gain
                    f[f"wmi_gain_tbl_ch{c}"] = v[40 + c]
    return f


def parse(path):
    b = open(path, "rb").read()
    csi, meta, i, bad = [], {}, 0, 0
    wmi = {}
    while i + 4 <= len(b):
        magic = u32(b, i)
        if magic == CSI_START and i + HDR_LEN + META_LEN + 16 <= len(b):
            j = i + HDR_LEN + META_LEN
            if u32(b, j) != WIFI8_TAG:
                i += 4; bad += 1
                continue
            n = b[j + 7] * 4 + u32(b, j + 8)
            if j + n + 4 > len(b) or u32(b, j + n) != END:
                i += 4; bad += 1
                continue
            rec = {"host_ts": struct.unpack_from("<Q", b, i + 14)[0] / 1e9,
                   "meta_version": b[i + 8], "chip_type": b[i + 9]}
            rec.update(parse_meta_enh(b, i + HDR_LEN))
            rec["phy_ppdu_id"] = u16(b, j + 16)
            rec["chip_tsf_us"] = struct.unpack_from("<Q", b, j + 82)[0] if n >= 90 else None
            csi.append(rec)
            i = j + n + 4
        elif magic == RXMETA and i + 20 <= len(b):
            ppdu, pdev, ntlv = u16(b, i + 4), b[i + 6], b[i + 7]
            ts = struct.unpack_from("<Q", b, i + 8)[0] / 1e9
            nbytes = u32(b, i + 16)
            p, end = i + 20, i + 20 + nbytes
            if end + 4 > len(b) or u32(b, end) != END:
                i += 4; bad += 1
                continue
            tlv = {}
            for _ in range(ntlv):
                tag, ln = struct.unpack_from("<HH", b, p)
                tlv[tag] = b[p + 4:p + 4 + ln]
                p += 4 + ((ln + 3) & ~3)
            meta.setdefault(ppdu, []).append({"pdev": pdev, "host_ts": ts, "tlv": tlv})
            i = end + 4
        elif magic == WMIEVT and i + 20 <= len(b):
            ppdu = u16(b, i + 4)
            ts = struct.unpack_from("<Q", b, i + 8)[0] / 1e9
            nbytes = u32(b, i + 16)
            end = i + 20 + nbytes
            if end + 4 > len(b) or u32(b, end) != END:
                i += 4; bad += 1
                continue
            raw = b[i + 20:end]
            wmi.setdefault(ppdu, []).append({"host_ts": ts, "raw": raw, "f": decode_wmi(raw)})
            i = end + 4
        else:
            i += 4
    return csi, meta, wmi, bad


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    csi, meta, wmi, bad = parse(src)
    rows, used, wused = [], set(), set()
    for r in csi:
        cands = meta.get(r["phy_ppdu_id"], [])
        m = min(cands, key=lambda x: abs(x["host_ts"] - r["host_ts"])) if cands else None
        row = dict(r)
        row["rxmeta_found"] = m is not None
        if m:
            used.add((r["phy_ppdu_id"], m["host_ts"]))
            row["rxmeta_minus_csi_ms"] = round((m["host_ts"] - r["host_ts"]) * 1e3, 3)
            row.update(decode_tlvs(m["tlv"]))
            row["header_filled"] = r["hdr_agc"] != [0, 0, 0]
        # a ppdu id wraps at 65536: take the event closest in host time, and within 1 s
        wc = [x for x in wmi.get(r["phy_ppdu_id"], []) if abs(x["host_ts"] - r["host_ts"]) < 1.0]
        e = min(wc, key=lambda x: abs(x["host_ts"] - r["host_ts"])) if wc else None
        row["wmi_found"] = e is not None
        if e:
            wused.add((r["phy_ppdu_id"], e["host_ts"]))
            row["wmi_minus_csi_ms"] = round((e["host_ts"] - r["host_ts"]) * 1e3, 3)
            row.update(e["f"])
        rows.append(row)
    keys = []
    for r in rows:
        keys += [k for k in r if k not in keys]
    with open(os.path.join(out, "records_meta.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        w.writerows(rows)
    raw = {f"{ppdu}@{m['host_ts']:.6f}": {TAG_NAMES.get(t, str(t)): v.hex() for t, v in m["tlv"].items()}
           for ppdu, lst in meta.items() for m in lst}
    json.dump(raw, open(os.path.join(out, "rxmeta_raw.json"), "w"), indent=1)
    wraw = {f"{ppdu}@{e['host_ts']:.6f}": e["raw"].hex() for ppdu, lst in wmi.items() for e in lst}
    json.dump(wraw, open(os.path.join(out, "wmievt_raw.json"), "w"), indent=1)

    n_meta = sum(len(v) for v in meta.values())
    n_wmi = sum(len(v) for v in wmi.values())
    found = sum(1 for r in rows if r["rxmeta_found"])
    wfound = sum(1 for r in rows if r["wmi_found"])
    filled = sum(1 for r in rows if r.get("header_filled"))
    print(f"CSI records {len(csi)}, rx-meta records {n_meta}, matched {found}, "
          f"header filled by the driver {filled}, rx-meta without CSI {n_meta - len(used)}, "
          f"malformed {bad}")
    print(f"WMI event records {n_wmi}, matched to CSI {wfound}, WMI events without CSI "
          f"{n_wmi - len(wused)}")
    for r in rows[:5]:
        print(json.dumps({k: r.get(k) for k in (
            "phy_ppdu_id", "peer_mac", "center_freq1", "status", "rssi_comb_db",
            "rssi_ch0_per20_db", "rssi_ch1_per20_db", "agc_ch0", "agc_ch1", "agc_ch2",
            "cfo_raw", "airtime_us", "lsig_rate_code", "lsig_length", "rxmeta_minus_csi_ms",
            "header_filled", "hdr_agc", "hdr_cfo", "hdr_chain_rssi", "hdr_rssi_comb",
            "hdr_puncture_bitmap")}))


if __name__ == "__main__":
    main()
