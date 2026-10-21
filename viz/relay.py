"""Parse luồng relay CFR (như router/wifi_sensing.c parse_one / kit cfr_export.parse_relay) → bản ghi CSI phức."""
import struct

import numpy as np

START, END, TAG = 0xDEADBEAF, 0xBEAFDEAD, 0xC0DE00BA
NON_DC = np.tile(np.arange(53) != 26, 4)
MAX_REC = 16384


def parse(buf):
    """buf: bytes/bytearray. Trả (list[(tsf_us, h chain×208 complex64)], số byte đã dùng). Dò từng byte như kit."""
    out, i, n = [], 0, len(buf)
    while True:
        i = buf.find(struct.pack("<I", START), i)
        if i < 0 or i + 115 + 90 > n:
            return out, (n if i < 0 else i)
        h = i + 115
        if struct.unpack_from("<I", buf, h)[0] != TAG:
            i += 1
            continue
        hdr = buf[h + 7] * 4
        size = hdr + struct.unpack_from("<I", buf, h + 8)[0]
        if size > MAX_REC or hdr < 90:
            i += 1
            continue
        if h + size + 4 > n:
            return out, i  # chưa đủ dữ liệu: giữ từ đầu bản ghi
        if struct.unpack_from("<I", buf, h + size)[0] != END:
            i += 1
            continue
        chains = struct.unpack_from("<I", buf, h + 18)[0] & 0xFF
        if size - hdr >= chains * 212 * 4:
            iq = np.frombuffer(bytes(buf[h + hdr:h + hdr + chains * 212 * 4]), "<i2").astype(np.float32).reshape(chains, 212, 2)
            out.append((struct.unpack_from("<Q", buf, h + 82)[0], (iq[..., 0] + 1j * iq[..., 1])[:, NON_DC]))
        i = h + size + 4


if __name__ == "__main__":  # self-check: khớp số bản ghi + TSF với cfr_export của kit
    import sys
    import pandas as pd
    recs, _ = parse(open(sys.argv[1], "rb").read())
    ref = pd.read_csv(sys.argv[2] + "/records.csv", usecols=["chip_tsf_us"]).chip_tsf_us.to_numpy()
    got = np.array([r[0] for r in recs])
    assert len(got) == len(ref) and (got == ref).all(), (len(got), len(ref))
    print(f"PASS {len(got)} bản ghi, TSF khớp kit; chain={recs[0][1].shape[0]}")
