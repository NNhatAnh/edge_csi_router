#!/bin/sh
# cfr_driver_swap.sh -- 2026-09-30. Run the PATCHED ath12k (driver_patch/, cfr_dbr_only) on the
# RDP499 without touching the flash: the two patched modules are bind-mounted over the stock
# files in /lib/modules and the ath12k stack is reloaded. A reboot drops the bind mounts, so the
# board always boots the stock driver (and the board panics+reboots on an oops: panic_on_oops=1,
# panic=3), i.e. nothing here can leave the board on a broken driver across a power cycle.
#
#   load      bind-mount /tmp/cfrdrv/{ath12k,ath12k_wifi8}.ko, reload with cfr_dbr_only=1
#   restore   remove the bind mounts, reload the stock modules (cfr_dbr_only disappears)
#   status    which modules are mounted/loaded, cfr_dbr_only, enable_cfr nodes, SSIDs
#
# Run detached (Wi-Fi goes down ~40-60 s):
#   ssh root@192.168.5.1 'setsid sh /tmp/cfr_driver_swap.sh load < /dev/null > /tmp/cfr_swap.log 2>&1 &'
# Refuses any board but the RDP499.
MODE=${1:-status}; K=/lib/modules/$(uname -r); D=${CFRDRV:-/tmp/cfrdrv}
[ "$(cat /tmp/sysinfo/board_name)" = "qcom,ipq5210-rdp499" ] || { echo "not the RDP499"; exit 1; }
now() { cut -d' ' -f1 /proc/uptime; }
# the parameters the boot uses: first line of /etc/modules.d/ath12k minus the module name
PARAMS=$(head -1 /etc/modules.d/ath12k | sed 's/^ath12k *//')

status() {
    echo "mounts: $(mount | grep -c "$K/ath12k") bind mount(s) over $K"
    mount | grep "$K/ath12k" | sed 's/^/  /'
    md5sum $K/ath12k.ko $K/ath12k_wifi8.ko | sed 's/^/  /'
    echo "cfr_dbr_only=$(cat /sys/module/ath12k/parameters/cfr_dbr_only 2>/dev/null || echo 'absent (stock module)')" \
         "cfr_enable_bmap=$(cat /sys/module/ath12k/parameters/cfr_enable_bmap)"
    echo "enable_cfr nodes: $(ls /sys/kernel/debug/ath12k/*/mac*/enable_cfr 2>/dev/null | wc -l)  ssids: $(iw dev | grep -c ssid)"
    lsmod | grep -E "^(ath12k|ath12k_wifi7|ath12k_wifi8|qca_wifi_nss_plugins) " | sed 's/^/  /'
}

# unload and reload the ath12k stack; $1 = extra ath12k parameters
reload() {
    t0=$(now)
    echo "--- wifi down"; wifi down; sleep 3
    for m in ath12k_wifi8 qca_wifi_nss_plugins ath12k_wifi7 ath12k; do
        rmmod $m || { echo "rmmod $m FAILED -- bringing the stack back"; recover; return 1; }
        echo "rmmod $m ok"
    done
    sleep 1
    echo "--- insmod ath12k $PARAMS $1"
    insmod $K/ath12k.ko $PARAMS $1 || { echo "insmod ath12k FAILED"; recover; return 1; }
    insmod $K/ath12k_wifi7.ko; insmod $K/qca-wifi-nss-plugins.ko; insmod $K/ath12k_wifi8.ko
    i=0; while [ $i -lt 90 ]; do sleep 2; i=$((i+2)); [ "$(ls /sys/kernel/debug/ath12k/*/mac*/enable_cfr 2>/dev/null | wc -l)" -ge 1 ] && break; done
    echo "CFR nodes after +${i}s"
    echo "--- wifi up"; wifi up
    i=0; while [ $i -lt 120 ]; do sleep 3; i=$((i+3)); [ "$(iw dev | grep -c ssid)" -ge 2 ] && break; done
    [ -x /etc/init.d/txpower-reapply ] && /etc/init.d/txpower-reapply start
    echo "ssids=$(iw dev | grep -c ssid) after wifi up +${i}s; elapsed $(awk -v a=$t0 -v b=$(now) 'BEGIN{print b-a}') s"
}
# best effort: load whatever modules are missing (stock or bind-mounted, whichever is in place)
recover() {
    lsmod | grep -q "^ath12k " || insmod $K/ath12k.ko $PARAMS
    lsmod | grep -q "^ath12k_wifi7 " || insmod $K/ath12k_wifi7.ko
    lsmod | grep -q "^qca_wifi_nss_plugins " || insmod $K/qca-wifi-nss-plugins.ko
    lsmod | grep -q "^ath12k_wifi8 " || insmod $K/ath12k_wifi8.ko
    sleep 10; wifi up
}

case "$MODE" in
status) status ;;
load)
    for m in ath12k ath12k_wifi8; do [ -s $D/$m.ko ] || { echo "missing $D/$m.ko"; exit 1; }; done
    for m in ath12k ath12k_wifi8; do
        mount | grep -q " $K/$m.ko " || mount --bind $D/$m.ko $K/$m.ko || { echo "bind $m failed"; exit 1; }
    done
    reload "cfr_dbr_only=1"; status ;;
restore)
    for m in ath12k ath12k_wifi8; do mount | grep -q " $K/$m.ko " && umount $K/$m.ko; done
    reload ""; status ;;
*) echo "usage: $0 status|load|restore"; exit 2 ;;
esac
echo "### swap_done mode=$MODE"
