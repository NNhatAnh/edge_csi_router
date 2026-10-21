#!/bin/sh
# wlan_fw_overlay.sh -- 2026-10-05. Run another QCN9625 WLAN firmware on the RDP499 from RAM.
#
# The driver reads ath12k/QCN9625/hw1.0/{amss,m3,board-2,...}.bin through symlinks into
# /lib/firmware/IPQ5210/WIFI_FW/qcn9625/, which is the flash squashfs (UBI volume wifi_fw).
# `mount` loop-mounts a release squashfs (e.g. WLAN.WBO.1.0/.../9625.wlanfw.eval/
# wifi_fw_qcn9625_squashfs.img) ON TOP of that directory; the flash volume is not touched and
# the caldata symlink (/lib/firmware/qcn9625/caldata_1.b5010) is outside it. Nothing is
# persistent: keep the image in /tmp, and a reboot returns the board to its flash firmware.
#
# The firmware is read only when the PCI driver probes, so a mount changes nothing until
# ath12k_wifi8 is reloaded (cfr_reset_ring.sh does that on this board, ~50 s, no reboot).
# Refuses the 488/489: they have their own persistent mechanism (/etc/init.d/fw-r8-mount),
# and a hot ath12k reload is forbidden on the 488.
#
# Usage: sh wlan_fw_overlay.sh mount <img> | umount | status
set -u
FW=/lib/firmware/IPQ5210/WIFI_FW
AMSS=$FW/qcn9625/amss.bin
B=$(cat /tmp/sysinfo/board_name)
case "$B" in *rdp499*) ;; *) echo "refused on $B (RDP499 only)"; exit 1 ;; esac

version() { grep -a -o 'QC_IMAGE_VERSION_STRING=[A-Za-z0-9._-]*' "$AMSS" 2>/dev/null | head -1; }
top_mount() { awk -v d="$FW" '$2 == d {s = $1} END {print s}' /proc/mounts; }
status() {
	echo "mounts on $FW (last = active):"
	awk -v d="$FW" '$2 == d {print "  " $1 " " $3}' /proc/mounts
	echo "active firmware: $(version)"
	echo "running firmware: $(dmesg | grep -o 'fw_build_id QC_IMAGE_VERSION_STRING=[A-Za-z0-9._-]*' | tail -1)"
}

case "${1:-status}" in
mount)
	IMG=${2:?usage: mount <squashfs img>}
	[ -f "$IMG" ] || { echo "no file $IMG"; exit 1; }
	[ "$(head -c 4 "$IMG")" = hsqs ] || { echo "$IMG is not a squashfs image"; exit 1; }
	case "$(top_mount)" in /dev/loop*) echo "an overlay is already mounted: umount first"; exit 1 ;; esac
	mount -t squashfs -o ro,loop "$IMG" "$FW" || exit 1
	if [ ! -f "$AMSS" ] || [ -z "$(version)" ]; then
		echo "image has no qcn9625/amss.bin with a version string: unmounting"
		umount "$FW"; exit 1
	fi
	status ;;
umount)
	case "$(top_mount)" in
	/dev/loop*) umount "$FW" || exit 1 ;;
	*) echo "no overlay mounted (top is $(top_mount))" ;;
	esac
	status ;;
status) status ;;
*) echo "usage: $0 mount <img> | umount | status"; exit 1 ;;
esac
