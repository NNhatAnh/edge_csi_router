#!/bin/sh
# cfr_rxmeta_stream.sh -- 2026-10-01. Periodic CSI capture through the relay with the 0003 driver
# (CFR Rx metadata, driver_patch/0003-cfr-rx-phy-metadata.patch). Each CSI record arrives with
# cfr_enh_metadata filled from the PHY TLVs of its PPDU, and each kept PPDU also arrives as its
# own raw-TLV record (magic 0xCF4E7A01). Decode on the PC with cfr_relay_meta.py.
#
# Unlike cfr_stream.sh it deletes no existing /tmp/cfr_dump_* file: it takes only the relay file
# its own reader created. Host state (enable_cfr, the station's cfr_capture, the reader) is
# restored by a trap. Never calls `wifi`, never writes UCI or flash.
#
# Usage: STA=<mac> [STOCK=1] [PERIOD_MS=100] [SECS=20] [BW=0] [METHOD=0] [OUT=dir] sh cfr_rxmeta_stream.sh
set -u
STA=${STA:?set STA=<station mac>}
PERIOD_MS=${PERIOD_MS:-100}; SECS=${SECS:-20}; BW=${BW:-0}; METHOD=${METHOD:-0}
AP_IF=${AP_IF:-phy00.0-0}; HW=${HW:-ath12k_hw0}
P=/sys/module/ath12k/parameters
EN=/sys/kernel/debug/ieee80211/phy00/$HW/enable_cfr
REQ=/sys/kernel/debug/ieee80211/phy00/netdev:$AP_IF/stations/$STA/link-0/cfr_capture
OUT=${OUT:-/tmp/cfr_rxm_$(date +%Y%m%d-%H%M%S)}

# without the 0003+ driver (e.g. the kit's 0001+0002, for an A/B) it still captures; the
# metadata counters then read n/a
[ -e $P/cfr_rxmeta ] || echo "note: cfr_rxmeta absent (not the 0003+ driver): CSI only"
# STOCK=1 accepts cfr_dbr_only=N: the stock correlate path (WMI Tx event + DMA buffer), which
# r8-00065 starves after 54 records but WLAN.WBO.1.0-01618 feeds (status bit 31 set, 2026-10-05)
[ "${STOCK:-0}" = 1 ] || [ "$(cat $P/cfr_dbr_only)" = Y ] || { echo "cfr_dbr_only is not on (STOCK=1 to accept)"; exit 1; }
[ -f "$REQ" ] || { echo "no station $STA on $AP_IF"; exit 1; }
[ "$(cat "$EN")" = 0 ] || { echo "enable_cfr already 1: someone is capturing"; exit 1; }
pidof cfr_test_app >/dev/null && { echo "cfr_test_app already running"; exit 1; }
mkdir -p "$OUT"
RP=
cleanup() {
	echo 0 > "$REQ" 2>/dev/null
	echo 0 > "$EN" 2>/dev/null
	[ -n "$RP" ] && { kill -INT "$RP" 2>/dev/null; sleep 1; kill "$RP" 2>/dev/null; }
	echo "restored: enable_cfr=$(cat "$EN") reader=$(pidof cfr_test_app || echo none)"
	echo "saved $OUT"
}
trap cleanup EXIT
trap 'exit 130' INT TERM HUP

counters() { echo "kept=$(cat $P/cfr_rxmeta_kept 2>/dev/null || echo n/a) hit=$(cat $P/cfr_rxmeta_hit 2>/dev/null || echo n/a) miss=$(cat $P/cfr_rxmeta_miss 2>/dev/null || echo n/a)"; }
# /proc/stat "cpu" line: total jiffies, idle+iowait, irq+softirq
cpu() { awk '/^cpu /{print $2+$3+$4+$5+$6+$7+$8, $5+$6, $7+$8; exit}' /proc/stat; }
{
	echo "# board=$(cat /tmp/sysinfo/board_name) sta=$STA request=\"1 $BW $PERIOD_MS $METHOD\" secs=$SECS cfr_rxmeta=$(cat $P/cfr_rxmeta 2>/dev/null || echo absent)"
	echo "# ath12k srcversion=$(cat /sys/module/ath12k/srcversion 2>/dev/null) wifi8 srcversion=$(cat /sys/module/ath12k_wifi8/srcversion 2>/dev/null)"
	echo "# before: $(counters)"
	iw dev "$AP_IF" info | grep channel
	iw dev "$AP_IF" station get "$STA" | grep -m3 -E "inactive time|tx bitrate|last ack signal"
} > "$OUT/stream.log" 2>&1

ls /tmp/cfr_dump_phy00_*.bin 2>/dev/null > "$OUT/old_dumps.txt"
t0=$(cut -d' ' -f1 /proc/uptime)
echo 1 > "$EN"
cfr_test_app -i phy00 -c "$HW" > "$OUT/reader.log" 2>&1 & RP=$!
sleep 1
C0=$(cpu)
echo "1 $BW $PERIOD_MS $METHOD" > "$REQ"
sleep "$SECS"
echo 0 > "$REQ"
C1=$(cpu)
sleep 2
echo 0 > "$EN"
kill -INT "$RP" 2>/dev/null; sleep 1; kill "$RP" 2>/dev/null; RP=
for f in /tmp/cfr_dump_phy00_*.bin; do
	[ -e "$f" ] || continue
	grep -qxF "$f" "$OUT/old_dumps.txt" && continue     # someone else's file: leave it
	mv "$f" "$OUT/relay.bin"
done
echo "# after: $(counters)" >> "$OUT/stream.log"
echo "$C0 $C1" | awk '{t=$4-$1; printf "# cpu over the capture: busy %.2f%% irq+softirq %.2f%% (%d jiffies, all cores)\n", 100*(t-($5-$2))/t, 100*($6-$3)/t, t}' >> "$OUT/stream.log"
dmesg | awk -v t="$t0" '{s=$0; sub(/^\[ */,"",s); split(s,a,"]"); if (a[1]+0 > t) print}' \
	| grep -v -E "Checking arvif|DCS" > "$OUT/kernel.log"
echo "relay bytes: $(wc -c < "$OUT/relay.bin" 2>/dev/null || echo 0)"
grep "^# " "$OUT/stream.log" | tail -3
echo "kernel warnings: $(grep -ciE 'warn|error|fail|oops|bug' "$OUT/kernel.log")"
