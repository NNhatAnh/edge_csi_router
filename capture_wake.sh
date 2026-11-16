#!/bin/sh
# OpenWrt terminal-only TX motion test. All inference runs on router.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
MODEL=M5R; STA=; IP=; BW=80; PING_MS=10; PERIOD_MS=10; STEP=40
PHY=${PHY:-phy00}; HW=${HW:-ath12k_hw0}; AP_IF=${AP_IF:-phy00.0-0}
usage() { echo "sh $0 --mac MAC --ip IPv4 [--model M1|M2|M3|M4|M5|M5R] [--bw 80] [--ping-ms 10] [--capture-ms 10] [--step-packets 40]"; }
while [ $# -gt 0 ]; do
 case "$1" in --help|-h) usage; exit 0;; esac
 [ $# -ge 2 ] || { usage; exit 1; }
 case "$1" in --model) MODEL=$2;; --mac) STA=$2;; --ip) IP=$2;; --bw) BW=$2;; --ping-ms) PING_MS=$2;; --capture-ms) PERIOD_MS=$2;; --step-packets) STEP=$2;; *) echo "Unknown option: $1" >&2; exit 1;; esac
 shift 2
done
STA=$(printf '%s' "$STA" | tr 'A-F' 'a-f')
printf '%s\n' "$STA" | grep -Eq '^([0-9a-f]{2}:){5}[0-9a-f]{2}$' || { echo 'Invalid MAC' >&2; exit 1; }
printf '%s\n' "$IP" | awk -F. 'NF!=4{exit 1}{for(i=1;i<=4;i++)if($i!~/^[0-9]+$/||$i>255)exit 1}' || { echo 'IPv4 required' >&2; exit 1; }
[ "$BW" = 80 ] || { echo 'These five models support BW80 only' >&2; exit 1; }
case "$STEP" in ''|*[!0-9]*) echo 'Invalid packet step'; exit 1;; esac
[ "$STEP" -ge 1 ] && [ "$STEP" -le 40 ] || { echo 'Packet step must be 1..40'; exit 1; }
for val in "$PING_MS" "$PERIOD_MS"; do case "$val" in ''|*[!0-9]*) echo 'Intervals must be integer ms'; exit 1;; esac; [ "$val" -le 60000 ] || exit 1; done
[ "$PING_MS" -ge 1 ] || { echo 'Ping interval must be at least 1 ms'; exit 1; }
[ "$PERIOD_MS" -ge 10 ] || { echo 'Capture interval must be at least 10 ms'; exit 1; }
[ $((PERIOD_MS % 10)) -eq 0 ] || { echo 'Capture interval must be multiple of 10 ms'; exit 1; }
case "$MODEL" in
 M1) ACTIVE_MODEL=$HERE/models/M1_logistic.txt;;
 M2) ACTIVE_MODEL=$HERE/models/M2_rf.txt;;
 M3) ACTIVE_MODEL=$HERE/models/M3_boost.txt;;
 M4) ACTIVE_MODEL=$HERE/models/M4_rf_all.txt;;
 M5) ACTIVE_MODEL=$HERE/models/M5_logistic.txt;;
 M5R) ACTIVE_MODEL=$HERE/models/M5R_logistic.txt;;
 *) echo 'Model must be M1, M2, M3, M4, M5 or M5R' >&2; exit 1;;
esac
[ -s "$ACTIVE_MODEL" ] || { echo "Missing model: $ACTIVE_MODEL" >&2; exit 1; }
FILES=$ACTIVE_MODEL
BIN=$HERE/wake_native
[ -x "$BIN" ] || { echo "Run chmod +x $BIN"; exit 1; }
EN=/sys/kernel/debug/ieee80211/$PHY/$HW/enable_cfr
REQ=/sys/kernel/debug/ieee80211/$PHY/netdev:$AP_IF/stations/$STA/link-0/cfr_capture
RELAY=/sys/kernel/debug/ieee80211/$PHY/$HW/cfr_capture0
P=/sys/module/ath12k/parameters
[ "$(id -u)" = 0 ] || { echo 'Bộ thu cần chạy bằng root' >&2; exit 1; }
iw dev "$AP_IF" station get "$STA" >/dev/null 2>&1 || {
 echo "Điện thoại $STA hiện không kết nối Wi-Fi 5 GHz của router. Kết nối lại rồi bấm Tìm thiết bị để chọn đúng MAC/IP." >&2
 exit 1
}
[ -e "$REQ" ] || { echo "Thiếu cfr_capture của thiết bị $STA trên $AP_IF. Thiết bị có thể vừa ngắt kết nối hoặc đổi MAC." >&2; exit 1; }
[ -w "$REQ" ] && [ -r "$RELAY" ] && [ -w "$EN" ] || {
 echo "Không truy cập được CFR trên $PHY/$HW. Kiểm tra driver và quyền debugfs." >&2
 exit 1
}
[ "$(cat "$P/cfr_dbr_only")" = Y ] && [ $(( $(cat "$P/cfr_rxmeta") & 3 )) -eq 3 ] || { echo 'Current CSI driver must provide DBR and WMI metadata'; exit 1; }
# Check radio before acquiring resources. BW in the request is not radio width.
iw dev "$AP_IF" info | grep -q 'width: 80 MHz' || { echo 'AP must already operate at 80 MHz; this script does not reconfigure Wi-Fi'; exit 1; }
iw dev "$AP_IF" info | grep -Eq '\(5[0-9][0-9][0-9] MHz\)' || { echo 'AP must operate in 5 GHz'; exit 1; }
INTERVAL=$(awk -v ms="$PING_MS" 'BEGIN{printf "%.3f",ms/1000}')
mkdir /tmp/cfr_spool.lock 2>/dev/null || { echo 'CSI capture lock exists; stop its owner first'; exit 1; }
RP=; PP=; ACTIVE=0; READY=/tmp/wake_ready_$$
cleanup() {
 trap - EXIT INT TERM HUP
 if [ "$ACTIVE" = 1 ]; then echo 0 > "$REQ" 2>/dev/null || true; echo 0 > "$EN" 2>/dev/null || true; fi
 for child in "$PP" "$RP"; do if [ -n "$child" ]; then kill "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi; done
 rm -f "$READY"
 rmdir /tmp/cfr_spool.lock 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT TERM HUP
[ "$(cat "$EN")" = 0 ] || { echo 'CFR is already active; stop current capture first'; exit 1; }
if pidof cfr_test_app cfr_spool cfr_file_spool wave_native_arm64 tx_motion_native wake_native >/dev/null 2>&1; then echo 'Another CSI reader is active'; exit 1; fi
"$BIN" --format "${WAKE_OUTPUT_FORMAT:-json}" --step-packets "$STEP" --model "$FILES" --sta "$STA" --relay "$RELAY" --ready "$READY" & RP=$!
i=0
while [ ! -f "$READY" ]; do
 kill -0 "$RP" 2>/dev/null || { echo 'Model/reader failed'; exit 1; }
 i=$((i+1)); [ "$i" -lt 200 ] || { echo 'Reader startup timeout'; exit 1; }
 busybox usleep 50000
done
ACTIVE=1
echo 1 > "$EN"
echo "1 2 $PERIOD_MS 0" > "$REQ"
"$BIN" --ping "$IP" --interval-ms "$PING_MS" & PP=$!
# Watch both children so a failed ping cannot silently leave capture running.
while kill -0 "$RP" 2>/dev/null; do
 kill -0 "$PP" 2>/dev/null || { echo 'ICMP sender exited; see error above' >&2; exit 1; }
 sleep 1
done
wait "$RP"
RP=
