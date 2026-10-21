#!/bin/sh
# cfr_driver_install.sh -- 2026-09-30. Make the PATCHED ath12k (driver_patch/, cfr_dbr_only) the
# board's driver across reboots, or put the stock one back. Writes to the overlay (flash).
#
#   install    stock ath12k.ko / ath12k_wifi8.ko -> /root/ath12k-stock/ (once, md5-checked);
#              /tmp/cfrdrv/*.ko -> /lib/modules/<kver>/ (md5-checked);
#              S04 gets "cfr_dbr_only=1" next to "cfr_enable_bmap=7" (backup
#              /root/ath12k_dyn_dbg_enable.sh.pre-dbronly). Takes effect at the next boot (or run
#              cfr_driver_swap.sh load for the running system).
#   uninstall  stock modules back from /root/ath12k-stock/, the cfr_dbr_only line removed.
#              (the pair that was on flash before each install is in /root/ath12k-before-*/)
#   status     md5 of the installed modules and the S04 lines.
# A failure of the patched driver leaves the board booting with Ethernet and SSH; only Wi-Fi is
# affected, and "uninstall" + reboot returns to stock. RDP499 only.
MODE=${1:-status}; K=/lib/modules/$(uname -r); D=${CFRDRV:-/tmp/cfrdrv}; ST=/root/ath12k-stock
S04=/etc/init.d/ath12k_dyn_dbg_enable.sh; B04=/root/ath12k_dyn_dbg_enable.sh.pre-dbronly
# the pair to install: the kit's 0001+0002 by default; another build by md5, e.g. 0005:
#   PATCHED_A=26641585f7d8aee50885a2673b2281e4 PATCHED_W=72c817c116e5bbb769b68b34c24f916b
PATCHED_A=${PATCHED_A:-6c340d7724f02fb89a3b07f650d49808}; PATCHED_W=${PATCHED_W:-7aaf143ce4306d4dd900b62cdf1bf5ce}
[ "$(cat /tmp/sysinfo/board_name)" = "qcom,ipq5210-rdp499" ] || { echo "not the RDP499"; exit 1; }
m5() { md5sum "$1" 2>/dev/null | cut -c1-32; }
unbind() { for m in ath12k ath12k_wifi8; do mount | grep -q " $K/$m.ko " && umount $K/$m.ko; done; }
status() {
    echo "installed: ath12k.ko $(m5 $K/ath12k.ko)  ath12k_wifi8.ko $(m5 $K/ath12k_wifi8.ko)"
    [ "$(m5 $K/ath12k.ko)" = $PATCHED_A ] && [ "$(m5 $K/ath12k_wifi8.ko)" = $PATCHED_W ] && echo "  = PATCHED" || echo "  = not the patched pair"
    echo "stock copies: $(ls $ST 2>/dev/null | tr '\n' ' ')"
    echo "bind mounts: $(mount | grep -c "$K/ath12k")"
    grep -n "cfr_enable_bmap\|cfr_dbr_only" $S04
    echo "live cfr_dbr_only=$(cat /sys/module/ath12k/parameters/cfr_dbr_only 2>/dev/null || echo absent)"
}
case "$MODE" in
status) status ;;
install)
    [ "$(m5 $D/ath12k.ko)" = $PATCHED_A ] && [ "$(m5 $D/ath12k_wifi8.ko)" = $PATCHED_W ] || { echo "missing or wrong $D/*.ko"; exit 1; }
    unbind
    mkdir -p $ST
    for m in ath12k ath12k_wifi8; do
        if [ ! -s $ST/$m.ko ]; then
            case "$(m5 $K/$m.ko)" in $PATCHED_A|$PATCHED_W) echo "$K/$m.ko is already patched and no stock copy exists: stop"; exit 1 ;; esac
            cp -a $K/$m.ko $ST/$m.ko && [ "$(m5 $ST/$m.ko)" = "$(m5 $K/$m.ko)" ] || { echo "stock backup of $m failed"; exit 1; }
        fi
    done
    md5sum $ST/*.ko > $ST/MD5SUMS
    # whatever is on flash now (stock, the kit pair, someone's own build) is kept before it is
    # replaced: /root/ath12k-before-<time>/, unless it already is the pair being installed
    if [ "$(m5 $K/ath12k.ko)" != $PATCHED_A ] || [ "$(m5 $K/ath12k_wifi8.ko)" != $PATCHED_W ]; then
        PB=/root/ath12k-before-$(date +%Y%m%d-%H%M%S); mkdir -p $PB
        for m in ath12k ath12k_wifi8; do cp -a $K/$m.ko $PB/$m.ko && [ "$(m5 $PB/$m.ko)" = "$(m5 $K/$m.ko)" ] || { echo "backup of $m to $PB failed"; exit 1; }; done
        md5sum $PB/*.ko > $PB/MD5SUMS; echo "previous pair kept in $PB: $(cut -c1-8 $PB/MD5SUMS | tr '\n' ' ')"
    fi
    for m in ath12k ath12k_wifi8; do cat $D/$m.ko > $K/$m.ko.new && mv $K/$m.ko.new $K/$m.ko; done
    sync
    [ -e $B04 ] || cp -a $S04 $B04
    grep -q cfr_dbr_only $S04 || awk '{ print }
        /module_params="\$module_params cfr_enable_bmap=7"/ { print "\tmodule_params=\"$module_params cfr_dbr_only=1\"" }' $B04 > /tmp/s04.new
    [ -s /tmp/s04.new ] && { grep -q cfr_dbr_only=1 /tmp/s04.new && cat /tmp/s04.new > $S04; rm -f /tmp/s04.new; }
    $S04 boot; sync
    echo "modules.d: $(head -1 /etc/modules.d/ath12k)"
    status ;;
uninstall)
    [ -s $ST/ath12k.ko ] && [ -s $ST/ath12k_wifi8.ko ] || { echo "no stock copies in $ST"; exit 1; }
    unbind
    for m in ath12k ath12k_wifi8; do cat $ST/$m.ko > $K/$m.ko.new && mv $K/$m.ko.new $K/$m.ko; done
    if [ -e $B04 ]; then cat $B04 > $S04; else sed -i '/cfr_dbr_only=1/d' $S04; fi
    $S04 boot; sync
    echo "modules.d: $(head -1 /etc/modules.d/ath12k)"
    status ;;
*) echo "usage: $0 status|install|uninstall"; exit 2 ;;
esac
echo "### install_done mode=$MODE"
