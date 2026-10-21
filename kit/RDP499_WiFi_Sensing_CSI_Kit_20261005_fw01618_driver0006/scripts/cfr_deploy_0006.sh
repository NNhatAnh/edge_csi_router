#!/bin/sh
# cfr_deploy_0006.sh -- 2026-10-05. Make driver 0006 the flash driver of an RDP499, make WLAN
# firmware WLAN.WBO.1.0-01618 its boot firmware (wlan_fw_overlay, the flash volume untouched),
# and reboot into both. Expects in /tmp:
#   cfrdrv/{ath12k,ath12k_wifi8}.ko   the 0006 pair
#   fw_01618.img                      9625.wlanfw.eval/wifi_fw_qcn9625_squashfs.img of 01618
#   cfr_driver_install.sh, wlan_fw_overlay.init
# Refuses, before writing anything, when:
#   - the board is not an RDP499, a module or image md5 is wrong, overlay has < 14 MB free;
#   - someone is capturing (enable_cfr = 1 on a radio, or a cfr_test_app runs): FORCE=1 overrides;
#   - S04 has no cfr_enable_bmap=7 line (the installer only adds cfr_dbr_only next to it).
# The pair on flash before the install is kept in /root/ath12k-before-<time>/ by the installer.
# Rollback: rm /root/wlan_fw/ENABLED (firmware), cfr_driver_install.sh uninstall (driver), reboot.
# REBOOT=0 installs without rebooting.
set -u
A=e7e8ed4a4a5c5cbd02ebb5bbfc9729f4; W=72c817c116e5bbb769b68b34c24f916b
FWMD5=1d01b51fa091c6ca4df2726773106b9e; FWVER=WLAN.WBO.1.0-01618-QCAHKSWPL_SILICONZ-1
K=/lib/modules/$(uname -r); S04=/etc/init.d/ath12k_dyn_dbg_enable.sh; FD=/root/wlan_fw
m5() { md5sum "$1" 2>/dev/null | cut -c1-32; }
fail() { echo "REFUSED: $*"; exit 1; }

[ "$(cat /tmp/sysinfo/board_name)" = "qcom,ipq5210-rdp499" ] || fail "not an RDP499"
[ "$(m5 /tmp/cfrdrv/ath12k.ko)" = $A ] && [ "$(m5 /tmp/cfrdrv/ath12k_wifi8.ko)" = $W ] || fail "/tmp/cfrdrv/*.ko missing or wrong md5"
[ "$(m5 /tmp/fw_01618.img)" = $FWMD5 ] || fail "/tmp/fw_01618.img missing or wrong md5"
[ -s /tmp/cfr_driver_install.sh ] && [ -s /tmp/wlan_fw_overlay.init ] || fail "installer or init script missing in /tmp"
free_kb=$(df -k /overlay | awk 'NR==2{print $4}')
[ "${free_kb:-0}" -ge 14336 ] || fail "overlay has only ${free_kb} KB free"
grep -q 'cfr_enable_bmap=7' $S04 || fail "S04 has no cfr_enable_bmap=7 (run cfr_enable_any.sh persist 7 first)"
busy=""
for e in /sys/kernel/debug/ieee80211/phy*/ath12k_hw*/enable_cfr; do
	[ -e "$e" ] && [ "$(cat "$e")" = 1 ] && busy="$busy $e=1"
done
pidof cfr_test_app >/dev/null && busy="$busy cfr_test_app(pid $(pidof cfr_test_app))"
if [ -n "$busy" ] && [ "${FORCE:-0}" != 1 ]; then fail "someone is capturing:$busy"; fi

echo "board $(cat /tmp/sysinfo/board_name) $(ip -4 addr show br-lan | awk '/inet /{print $2; exit}') up $(cut -d. -f1 /proc/uptime) s"
echo "before: ath12k.ko $(m5 $K/ath12k.ko | cut -c1-8) ath12k_wifi8.ko $(m5 $K/ath12k_wifi8.ko | cut -c1-8), overlay free ${free_kb} KB"

# 1. driver
PATCHED_A=$A PATCHED_W=$W sh /tmp/cfr_driver_install.sh install || fail "driver installer failed"
[ "$(m5 $K/ath12k.ko)" = $A ] && [ "$(m5 $K/ath12k_wifi8.ko)" = $W ] || fail "flash modules do not match after install"

# 2. firmware: image + md5 + expected version in /root/wlan_fw, the init script enabled at S05
mkdir -p $FD
cat /tmp/fw_01618.img > $FD/fw.img.new && mv $FD/fw.img.new $FD/fw.img
echo "$FWMD5  fw.img" > $FD/MD5SUMS
echo "$FWVER" > $FD/VERSION
( cd $FD && md5sum -c MD5SUMS ) || fail "fw.img md5 after copy"
rm -f $FD/PENDING $FD/DISABLED_AFTER_FAILURE
touch $FD/ENABLED
cat /tmp/wlan_fw_overlay.init > /etc/init.d/wlan_fw_overlay.new && mv /etc/init.d/wlan_fw_overlay.new /etc/init.d/wlan_fw_overlay
chmod 755 /etc/init.d/wlan_fw_overlay
/etc/init.d/wlan_fw_overlay enable
[ -e /etc/rc.d/S05wlan_fw_overlay ] || fail "S05wlan_fw_overlay link missing"
sync
echo "firmware: $FD/{fw.img,MD5SUMS,VERSION,ENABLED}, /etc/rc.d/S05wlan_fw_overlay"
if [ "${REBOOT:-1}" = 1 ]; then
	echo "### deploy_done: rebooting into 0006 + $FWVER"
	(sleep 2; reboot) > /dev/null 2>&1 &
else
	echo "### deploy_done: installed, both load at the next boot"
fi
