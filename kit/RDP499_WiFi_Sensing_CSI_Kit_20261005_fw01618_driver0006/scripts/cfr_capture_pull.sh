#!/bin/sh
# cfr_capture_pull.sh -- 2026-10-05. One command on the PC (Git Bash, Linux, macOS): capture CSI on
# an RDP499, copy the run back, decode it, and print the quality report.
#
#   sh cfr_capture_pull.sh <board ip> <station mac> [period ms] [seconds] [out dir]
#   sh cfr_capture_pull.sh 192.168.11.1 50:bb:b5:fa:e1:06 50 60
#
# Steps:
#   1. uploads cfr_rxmeta_stream.sh to /tmp on the board;
#   2. runs it: periodic capture through the relay; enable_cfr and the station's request are
#      restored on exit; no `wifi`, no UCI, no flash write;
#   3. copies the run directory back over ssh (no scp needed);
#   4. runs cfr_export.py, cfr_relay_meta.py, cfr_chain_rssi_est.py and cfr_quality_report.py.
#
# Needs ssh key access to root@<board> and python3 (or python) on the PC. The station must be
# associated and awake: a phone playing a video, or a laptop pinging the board over Wi-Fi
# (RDP499_CSI_CAPTURE_GUIDE.md §16.6). The script refuses if someone else is capturing.
set -eu
B=${1:?board ip}; STA=${2:?station mac, lower case}; PERIOD=${3:-50}; SECS=${4:-20}
HERE=$(cd "$(dirname "$0")" && pwd)
STAMP=$(date +%Y%m%d-%H%M%S)
OUT=${5:-$HERE/../runs/$B-$STAMP}
# the first interpreter that actually runs: on Windows "python3" is often the Microsoft Store stub
PY=""
for c in python3 python py; do
	command -v "$c" >/dev/null 2>&1 && "$c" -c "import sys; assert sys.version_info >= (3, 6)" >/dev/null 2>&1 && { PY=$c; break; }
done
[ -n "$PY" ] || { echo "no working python 3 found (python3 / python / py)"; exit 1; }
SSH="ssh -o BatchMode=yes -o ConnectTimeout=8 root@$B"

mkdir -p "$OUT"
echo "== board $B, station $STA, ${PERIOD} ms for ${SECS} s -> $OUT"
$SSH 'cat > /tmp/cfr_rxmeta_stream.sh' < "$HERE/cfr_rxmeta_stream.sh"
$SSH "STA=$STA PERIOD_MS=$PERIOD SECS=$SECS OUT=/tmp/cfr_rxm_$STAMP sh /tmp/cfr_rxmeta_stream.sh"
$SSH "cd /tmp/cfr_rxm_$STAMP && tar cf - ." | tar xf - -C "$OUT"
[ -s "$OUT/relay.bin" ] || { echo "no relay.bin came back: see $OUT/stream.log"; exit 1; }

echo "== decode"
"$PY" "$HERE/cfr_export.py" "$OUT/relay.bin" "$OUT/export_csi" | tail -1
"$PY" "$HERE/cfr_relay_meta.py" "$OUT/relay.bin" "$OUT/export" | head -2
"$PY" "$HERE/cfr_chain_rssi_est.py" "$OUT/export_csi" "$OUT/export/records_meta.csv" \
	"$OUT/export/chain_rssi_est.csv" || echo "(chain 2 estimate skipped)"
echo "== quality"
"$PY" "$HERE/cfr_quality_report.py" "$OUT"
echo "== done: $OUT"
echo "   CSI: export_csi/csi_iq.csv   metadata: export/records_meta.csv   chain 2: export/chain_rssi_est.csv"
echo "   on the board the run stays in /tmp/cfr_rxm_$STAMP (RAM) until reboot"
