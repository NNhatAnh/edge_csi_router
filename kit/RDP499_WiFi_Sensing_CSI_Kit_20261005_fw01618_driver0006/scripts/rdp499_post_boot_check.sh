#!/bin/sh
# rdp499_post_boot_check.sh -- 2026-10-05. Read-only check of an RDP499 after cfr_deploy_0006.sh
# or any reboot: firmware overlay state and boot log, the firmware the radio actually runs,
# driver md5 and CFR parameters, radios, kernel health. Changes nothing.
#   ssh root@<board> 'sh -s' < rdp499_post_boot_check.sh
# Expect: "running: WLAN.WBO.1.0-01618-...", ath12k.ko e7e8ed4a..., 0 bind mounts,
# cfr_dbr_only=Y cfr_rxmeta=11, oops 0. Wi-Fi needs 2-3 min after boot for both SSIDs.
P=/sys/module/ath12k/parameters
echo "board=$(cat /tmp/sysinfo/board_name) boot_id=$(cat /proc/sys/kernel/random/boot_id) up=$(cut -d. -f1 /proc/uptime)s"
echo "--- deploy log"; tail -6 /root/cfr_deploy_0006.log 2>/dev/null
echo "--- reset reason"; dmesg | grep -i reset_reason | head -2
echo "--- firmware overlay"; /etc/init.d/wlan_fw_overlay status
ls -la /etc/rc.d/ | grep -E "wlan_fw|wifi_fw|ath12k_dyn" | awk '{print $NF, $(NF-2)}'
echo "--- running firmware"; dmesg | grep -E "fw_build_id|board_id 0x|BDF download" | sed 's/^\[ *[0-9.]*\] //' | tail -4
dmesg | grep -E "aoa_for_rcc|wlan_fw_overlay" | sed 's/^\[ *[0-9.]*\] //'
echo "--- driver"; md5sum /lib/modules/$(uname -r)/ath12k.ko /lib/modules/$(uname -r)/ath12k_wifi8.ko
echo "bind mounts over modules: $(mount | grep -c '/lib/modules/.*/ath12k')"
for p in cfr_enable_bmap cfr_dbr_only cfr_rxmeta cfr_wmievt_seen debug_mask; do printf "%s=%s " $p "$(cat $P/$p 2>/dev/null)"; done; echo
echo "enable_cfr nodes: $(ls /sys/kernel/debug/ath12k/*/mac*/enable_cfr 2>/dev/null | wc -l)"
echo "--- radios"; iw dev | grep -E "Interface|ssid|channel|txpower"
echo "--- health"; echo "oops/call trace: $(dmesg | grep -ciE 'oops|call trace|kernel BUG|firmware crashed|SSR')"
dmesg | grep -iE "ath12k.*(error|fail)" | sed 's/^\[ *[0-9.]*\] //' | sort | uniq -c | sort -rn | head -6
df -k /overlay | tail -1; grep -E "MemAvailable" /proc/meminfo
