"""Export a CSI series collected by cfr_collect_any.sh into files a colleague can open.

Input : either the router's /tmp/cfr_collect/all.log (kernel lines per request, with the
        "data_from_buf_rel:" BIT(25) hex dumps of the QCN9625 wifi8 CFR DMA buffer), or a
        relay file written by cfr_test_app with the patched driver (driver_patch/,
        cfr_dbr_only=1): [ath12k_csi_cfr_header][DMA header + payload][0xBEAFDEAD] per
        record, complete (no 90-byte cut). The type is detected from the first 4 bytes.
Output: <outdir>/
          dataset.json    board, firmware, station, channel, layout, known gaps
          records.csv     one row per CSI record (header fields + host context)
          csi_iq.csv      one row per (record, chain, tone): I, Q, |h|, phase
          raw/rec_NNN.bin the buffer exactly as dumped (header + payload, driver-truncated)

Self-contained (standard library only). The header decode follows
../../router_cfr_probe/scripts/cfr_decode_dump.py and ../../router_cfr_probe/results/src/cfr.h
(struct ath12k_cfir_wifi8_common_hdr / _cc_hdr); the 488 copy is referenced, not imported,
so this directory can be handed over on its own.

Usage: python cfr_export.py <all.log> [<more.log> ...] <outdir>
       (several logs = the batches of one cfr_campaign.sh run; column `batch` says which)
"""
import csv
import json
import math
import os
import re
import struct
import sys

HEX = re.compile(r": ([0-9a-f]{8}): ((?:[0-9a-f]{2} ?)+)")
TS = re.compile(r"^\[ *([0-9.]+)\]")
REQ = re.compile(r"^=== REQ (\d+) t=([0-9.]+)(.*)$")
PEER = re.compile(r"^=== PEER (\d+) t=([0-9.]+)(.*)$")
KV = re.compile(r'(\w+)=("[^"]*"|\S*)')
TONES_PER_BLOCK = 53     # one 20 MHz sub-band, tones -26..+26, index 26 = DC (measured, 488 + 499)


def bits(v, lo, n):
    return (v >> lo) & ((1 << n) - 1)


RELAY_START, RELAY_END, WIFI8_TAG = 0xDEADBEAF, 0xBEAFDEAD, 0xC0DE00BA


def parse_relay(path):
    """Yield ('buf', host_real_ts_s, dma_bytes) for every record of a cfr_test_app relay file.

    struct ath12k_csi_cfr_header (cfr.h): u32 start magic, u32 vendor, u8 meta version,
    u8 chip type, u32 meta len, u64 host_real_ts (ns) at byte 14 (byte 10 is the u32 meta len;
    read at 10 before 2026-10-01, which gave a garbage time), then cfr_enh_metadata
    (empty with cfr_dbr_only). The DMA buffer starts at the wifi8 tag; its length is
    header_size*4 + payload_size, and the end magic must follow it.
    """
    b = open(path, "rb").read()
    i, bad = 0, 0
    while True:
        i = b.find(struct.pack("<I", RELAY_START), i)
        if i < 0 or i + 26 > len(b):
            break
        j = b.find(struct.pack("<I", WIFI8_TAG), i + 22, i + 400)
        if j < 0:
            i += 4; bad += 1
            continue
        hdr_bytes = b[j + 7] * 4
        n = hdr_bytes + struct.unpack_from("<I", b, j + 8)[0]
        if struct.unpack_from("<I", b, j + n)[0:1] != (RELAY_END,) if j + n + 4 <= len(b) else True:
            i += 4; bad += 1
            continue
        ts = struct.unpack_from("<Q", b, i + 14)[0] / 1e9
        yield ("buf", ts, b[j:j + n])
        i = j + n + 4
    if bad:
        print(f"{path}: {bad} malformed record(s) skipped", file=sys.stderr)


def parse(path):
    """Yield ('meta', dict) / ('req', dict) / ('tx', ts, bytes) / ('buf', ts, bytes)."""
    with open(path, "rb") as f:
        # a 0003-driver relay may start with an Rx-metadata record (0xCF4E7A01), a 0006-driver
        # relay with a WMI event record (0xCF4E7A02); parse_relay skips both,
        # cfr_relay_meta.py decodes them
        if f.read(4) in (struct.pack("<I", RELAY_START), struct.pack("<I", 0xCF4E7A01),
                         struct.pack("<I", 0xCF4E7A02)):
            yield ("meta", {"source_format": "relay"})
            yield from parse_relay(path)
            return
    block, kind, ts0 = None, None, None
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.rstrip("\n")
        m = HEX.search(line)
        if block is not None and m:
            off = int(m.group(1), 16)
            if off == len(block):
                block += bytes.fromhex(m.group(2).replace(" ", "")[:32])
                continue
        if block is not None:                      # block ended (non-hex line or offset reset)
            yield (kind, ts0, bytes(block))
            block = None
        if line.startswith("# "):
            yield ("meta", dict((k, v.strip('"')) for k, v in KV.findall(line)))
        elif line.startswith("=== PEER"):
            r = PEER.match(line)
            if r:
                d = dict((k, v.strip('"')) for k, v in KV.findall(r.group(3)))
                d.update(peer_req=int(r.group(1)), peer_uptime=float(r.group(2)))
                yield ("peer", d)
        elif line.startswith("=== REQ"):
            r = REQ.match(line)
            if r:
                d = dict((k, v.strip('"')) for k, v in KV.findall(r.group(3)))
                d.update(req=int(r.group(1)), req_uptime=float(r.group(2)))
                yield ("req", d)
        elif "data_from_buf_rel:" in line or "cfr_dump:" in line:
            t = TS.match(line)
            kind = "buf" if "data_from_buf_rel:" in line else "tx"
            ts0, block = (float(t.group(1)) if t else None), bytearray()
    if block is not None:
        yield (kind, ts0, bytes(block))


def decode_header(raw):
    tag, w1, payload_size, w3 = struct.unpack_from("<IIII", raw, 0)
    ppdu = struct.unpack_from("<H", raw, 16)[0]
    w5 = struct.unpack_from("<I", raw, 18)[0]
    pre, phy = struct.unpack_from("<HH", raw, 22)
    snd, xbar = struct.unpack_from("<HI", raw, 26)
    c0, c1 = struct.unpack_from("<HH", raw, 40)
    agr = struct.unpack_from("<I", raw, 44)[0]
    agr4 = raw[48]
    rescale_pri80, rescale_sec80 = raw[50], raw[51]
    cgim = struct.unpack_from("<H", raw, 52)[0]
    hdr_words = bits(w1, 24, 8)
    # freeze TLV v5 (struct macrx_freeze_capture_channel_v5, cfr.h) starts at byte 64:
    # 70..75 packet TA, 76..81 packet RA (= the AP's own MAC in every verified dump),
    # 82..89 the chip's 64-bit TSF in microseconds (verified 2026-09-30: consecutive records
    # differ by the same ~390 ms as the host clock, to a few ms)
    mac = raw[76:82].hex(":") if hdr_words * 4 >= 82 and len(raw) >= 82 else None
    tsf = struct.unpack_from("<Q", raw, 82)[0] if hdr_words * 4 >= 90 and len(raw) >= 90 else None
    bf = raw[49]
    punct, nltf, offs = struct.unpack_from("<HHH", raw, 54)
    fz = {}
    if hdr_words * 4 >= 92 and len(raw) >= 92:
        f0, fpeer, fppdu = struct.unpack_from("<HHH", raw, 64)
        u = struct.unpack_from("<H", raw, 90)[0]
        fz = dict(freeze_bit=f0 & 1, freeze_capture_reason=(f0 >> 1) & 7, freeze_packet_type=(f0 >> 4) & 3,
                  freeze_packet_sub_type=(f0 >> 6) & 0xF, freeze_sw_peer_id_valid=(f0 >> 15) & 1,
                  freeze_sw_peer_id=fpeer, freeze_phy_ppdu_id=f"0x{fppdu:04x}",
                  freeze_ta_mac=raw[70:76].hex(":"), freeze_user_index=u & 0x3F, freeze_directed=(u >> 6) & 1)
    return dict(
        header_tag=f"0x{tag:08x}", tag_ok=(tag == 0xC0DE00BA), chip_id=bits(w1, 0, 8),
        header_type=bits(w1, 8, 8), header_version=bits(w1, 16, 8),
        header_size_words=hdr_words, header_bytes=hdr_words * 4, payload_size=payload_size,
        sw_peer_id_valid=bits(w3, 15, 1), sw_peer_id=bits(w3, 16, 16), phy_ppdu_id=f"0x{ppdu:04x}",
        num_chains=bits(w5, 0, 8), nss=bits(w5, 8, 8), channel_bw=bits(w5, 16, 8),
        packet_bw=bits(w5, 24, 8), preamble_type=bits(pre, 0, 8), ltf_type=bits(pre, 8, 4),
        gi_type=bits(pre, 12, 4), phy_mode=bits(phy, 0, 2), rf_chain_mask=f"0x{bits(phy, 2, 8):02x}",
        sounding_dialog_token=bits(snd, 0, 8), pri20_location=bits(snd, 8, 8), xbar_config=f"0x{xbar:08x}",
        capture_type=bits(c0, 0, 4), cc_format=bits(c0, 4, 2), cir_fmt=bits(c0, 6, 3),
        freeze_data_incl=bits(c0, 11, 1), freeze_tlv_version=bits(c0, 12, 4),
        mu_rx_num_users=bits(c1, 0, 8), decimation_factor=bits(c1, 8, 4),
        amplitude_gain_ratio_0_3=f"0x{agr:08x}",
        # one byte per chain (cfr.h: "amplitude gain ratio for chains 0-3, one byte each");
        # 0x80 on chains 0/2 and 0x12-0x14 on the weak chain 1 of the RDP499 -- meaning not documented
        **{f"amp_gain_ratio_ch{c}": (agr >> (8 * c)) & 0xFF for c in range(4)}, amp_gain_ratio_ch4=agr4,
        rescale_amt_shift_pri80=rescale_pri80, rescale_amt_shift_sec80=rescale_sec80,
        cgim_status=cgim & 1, cgim_filter=(cgim >> 1) & 1, leg_cfr_mode=(cgim >> 6) & 3,
        aoa_cal_gdp_incl=bits(c0, 9, 1), mu_rx_data_incl=bits(c0, 10, 1),
        azbf_mode=bf & 0xF, azbf_node=bf >> 4,
        demf_turbo_mode=(cgim >> 3) & 1, demf_pbs_en=(cgim >> 4) & 3, tx_or_rx_based_cfr=(cgim >> 2) & 1,
        puncture_pattern=f"0x{punct:04x}", total_num_ltfs=nltf, freeze_tlv_offset_u16=offs & 0x1F,
        per_user_info_offset_u16=(offs >> 5) & 0x1F, aoa_cal_gdp_offset_u16=(offs >> 10) & 0x3F,
        freeze_tlv_mac=mac, chip_tsf_us=tsf, **fz)


def decode_tx(raw):
    """WMI_PEER_CFR_CAPTURE_EVENT fixed param as the BIT(25) cfr_dump shows it (116 B)."""
    g = lambda o: struct.unpack_from("<I", raw, o)[0] if len(raw) >= o + 4 else None
    return dict(tx_peer_mac=raw[12:18].hex(":") if len(raw) >= 18 else None,
                tx_prim20_mhz=g(20), tx_center_mhz=g(32), tx_status_word=g(52),
                tx_nonzero_bytes_after_40=sum(1 for b in raw[40:] if b), tx_event_hex=raw.hex())


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    srcs, out = sys.argv[1:-1], sys.argv[-1]
    os.makedirs(os.path.join(out, "raw"), exist_ok=True)
    meta, req, last_tx, recs, pdev = {}, {}, None, [], {}
    iq_rows = []
    events = ((bi, ev) for bi, src in enumerate(srcs, 1) for ev in parse(src))
    for batch, ev in events:
        if ev[0] == "meta":
            if "pdev_runtime_nf_chain" in ev[1]:
                if not pdev:                       # first "# pdev_*" line of this log = the start snapshot
                    pdev = {k: v for k, v in ev[1].items() if k.startswith("pdev_")}
            else:
                meta.update(ev[1])
                pdev = {} if "board" in ev[1] else pdev   # a new log starts: forget the old snapshot
        elif ev[0] == "peer":
            # firmware per-peer stats read right AFTER request N: attach to that request's record
            if recs and recs[-1].get("req") == ev[1]["peer_req"] and recs[-1].get("batch") == batch:
                recs[-1].update(ev[1])
        elif ev[0] == "req":
            req, last_tx = ev[1], None
        elif ev[0] == "tx":
            last_tx = (ev[1], ev[2])
        elif ev[0] == "buf":
            ts, raw = ev[1], ev[2]
            if len(raw) < 64:
                continue
            h = decode_header(raw)
            n = len(recs) + 1
            nch = max(h["num_chains"], 1)
            pay = raw[h["header_bytes"]:h["header_bytes"] + h["payload_size"]]
            total = h["payload_size"] // 4
            tones = total // nch
            present = len(pay) // 4
            row = dict(record=n, batch=batch, kernel_ts_s=ts, **{k: req.get(k) for k in
                       ("req", "req_uptime", "inactive", "ack_signal", "signal", "tx", "rx")})
            row.update(pdev)
            row.update(h)
            row.update(tones_per_chain=tones, samples_expected=total, samples_present=present,
                       samples_missing=total - present, dump_bytes=len(raw))
            if last_tx:
                row.update(tx_event_ts_s=last_tx[0], **decode_tx(last_tx[1]))
            recs.append(row)
            with open(os.path.join(out, "raw", f"rec_{n:03d}.bin"), "wb") as f:
                f.write(raw)
            for k in range(total):
                c, t = divmod(k, tones)
                blk, tb = divmod(t, TONES_PER_BLOCK)
                if k < present:
                    i, q = struct.unpack_from("<hh", pay, 4 * k)
                    iq_rows.append((n, c, t, blk, tb - TONES_PER_BLOCK // 2, i, q,
                                    f"{math.hypot(i, q):.1f}", f"{math.atan2(q, i):.5f}", 1))
                else:
                    iq_rows.append((n, c, t, blk, tb - TONES_PER_BLOCK // 2, "", "", "", "", 0))
    if not recs:
        raise SystemExit("no data_from_buf_rel: block in " + ", ".join(srcs))
    keys = []
    for r in recs:
        keys += [k for k in r if k not in keys]
    with open(os.path.join(out, "records.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        w.writerows(recs)
    with open(os.path.join(out, "csi_iq.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["record", "chain", "tone_index", "block_20mhz", "tone_in_block", "i", "q",
                    "amplitude", "phase_rad", "present"])
        w.writerows(iq_rows)
    with open(os.path.join(out, "dataset.json"), "w") as f:
        json.dump(dict(source=[os.path.basename(x) for x in srcs], capture=meta, records=len(recs),
                       requests=max((r.get("req") or 0) for r in recs),
                       layout=dict(
                           sample="int16 I, int16 Q, little-endian (I-first is assumed, not documented)",
                           order="chain-major: chain 0 tones 0..N-1, chain 1 ..., N = payload_size/4/num_chains",
                           tones="4 blocks of 53 per chain = four 20 MHz sub-bands of the capture window; "
                                 "tone_in_block -26..+26, 0 = DC (always 0 in the loud block)",
                           block_to_frequency="UNRESOLVED -- do not map block_20mhz to a channel",
                           header="struct ath12k_cfir_wifi8_common_hdr + _cc_hdr + freeze TLV, "
                                  "header_size counted in 32-bit words"),
                       known_gaps=[
                           "driver dump is 90 B short (reads header_size as bytes): last 23 samples of "
                           "the last chain are missing, present=0 in csi_iq.csv",
                           "no RSSI / AGC gain / noise floor from the chip: the Tx metadata event arrives "
                           "empty (status 0); ack_signal (last ACK RSSI of the station, dBm) and inactive "
                           "in records.csv are host-side iw readings taken just before each request",
                           "chip_tsf_us is the chip's 64-bit TSF (microseconds) of the captured PPDU; "
                           "kernel_ts_s is only the host print time"]),
                  f, indent=2)
    miss = sum(r["samples_missing"] for r in recs)
    print(f"{len(recs)} records -> {out} (records.csv, csi_iq.csv {len(iq_rows)} rows, raw/, dataset.json); "
          f"{miss} samples missing in total (driver truncation)")


if __name__ == "__main__":
    main()
