#!/bin/sh
# cfr_reset_ring.sh -- 2026-09-30. Give the CFR DMA ring its buffers back WITHOUT a reboot.
#
# Why: the stock driver never releases a held DBR buffer, because the Tx metadata event always
# arrives with status 0 and ath12k_cfr_free_pending_dbr_events() runs only after a success
# (../router_cfr_probe/results/src/cfr.c:102-183). So each boot gives a finite number of
# records (54 measured three times on the RDP499, README.md §4.4), then Tx events keep arriving with no
# payload. No debugfs node resets the ring. Re-probing the PCI driver does: the device is torn
# down, the firmware reboots, and ath12k_cfr_init() allocates a fresh ring (cfr_enable_bmap is
# a parameter of ath12k.ko, which stays loaded, so the value survives).
#
# Cost: Wi-Fi is down for the duration (~30-60 s); stations re-associate. Not a board reboot,
# and no firmware assert (simulate_fw_crash rebooted the 488 via ssr_failsafe_mode=1).
# Only for a single-SoC board whose WLAN PCI driver has no users (lsmod "ath12k_wifi8 ... 0").
# Refuses the 488 and the 489 (multi-SoC MLO hardware groups: not tested there).
#
#   ssh root@<ip> 'cat > /tmp/cfr_reset_ring.sh && (trap "" HUP; exec sh /tmp/cfr_reset_ring.sh) < /dev/null > /tmp/cfr_reset.log 2>&1 &'
#   (busybox here has no nohup; an ignored SIGHUP survives exec)
#   ssh root@<ip> 'cat /tmp/cfr_reset.log'
B=$(cat /tmp/sysinfo/board_name)
case "$B" in *rdp488*|*rdp489*) echo "refused on $B (multi-SoC hw group, not tested)"; exit 1 ;; esac
now() { cut -d' ' -f1 /proc/uptime; }
users=$(lsmod | awk '$1=="ath12k_wifi8"{print $3}')
[ "$users" = 0 ] || { echo "ath12k_wifi8 has users ($users), refusing"; exit 1; }
[ "$(cat /sys/module/ath12k/parameters/cfr_enable_bmap)" != 0 ] || echo "WARNING: cfr_enable_bmap=0, the new ring will not be CFR-enabled"
t0=$(now); echo "board=$B t0=$t0 ssids_before=$(iw dev | grep -c ssid)"
echo "--- wifi down"; wifi down; sleep 3
echo "--- rmmod ath12k_wifi8"; rmmod ath12k_wifi8 || { echo "rmmod failed, bringing wifi back"; wifi up; exit 1; }
sleep 2
echo "--- modprobe ath12k_wifi8"; modprobe ath12k_wifi8 || insmod /lib/modules/$(uname -r)/ath12k_wifi8.ko
i=0
while [ $i -lt 90 ]; do
    sleep 2; i=$((i+2))
    n=$(ls /sys/kernel/debug/ath12k/*/mac*/enable_cfr 2>/dev/null | wc -l)
    [ "$n" -ge 1 ] && [ -d /sys/kernel/debug/ieee80211/phy00 ] && break
done
echo "t=+${i}s enable_cfr_nodes=$n"
dmesg | awk -v t="$t0" '{ s=$0; sub(/^\[ */,"",s); split(s,a,"]"); if (a[1]+0 > t) print }' \
    | grep -iE 'Enabling CFR|fw_version|fw_build|firmware|error|fail' | grep -v 'Checking arvif' | head -15
echo "--- wifi up"; wifi up
i=0
while [ $i -lt 120 ]; do
    sleep 3; i=$((i+3))
    k=$(iw dev | grep -c ssid)
    [ "$k" -ge 2 ] && break
done
echo "t=+${i}s after wifi up: ssids=$(iw dev | grep -c ssid)"
# txpower-reapply is a boot-only S99 script (journal §15): re-apply the UCI txpower now
[ -x /etc/init.d/txpower-reapply ] && /etc/init.d/txpower-reapply start
iw dev | grep -E 'Interface|ssid|channel'
echo "### reset_done elapsed=$(awk -v a="$t0" -v b="$(now)" 'BEGIN{print b-a}') s"
