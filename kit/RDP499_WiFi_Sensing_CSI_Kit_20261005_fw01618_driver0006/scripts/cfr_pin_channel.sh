#!/bin/sh
# cfr_pin_channel.sh -- 2026-09-30. Hold the RDP499's 5 GHz radio on ONE channel for CSI work.
#
# The product config (docs/ipq5210_rdp499_build.md §15) runs QACS (channel=auto,
# qacs_enable=1) and DCS (dcs_enable=18), which moved the 5 GHz primary five times in one
# morning (48 -> 36 -> 44 -> 48 -> 36). CSI records taken across a move are not comparable.
#
#   pin [CH]  (default 36) backup /etc/config/wireless to /root/wireless.before_chanpin (once),
#             set radio0_band1 channel=CH, qacs_enable=0, dcs_enable=0, commit, `wifi reload`
#   unpin     restore that backup, `wifi reload`
#   status    show the UCI values and the live channel
#
# Stations drop and re-associate during the reload (~20-40 s). Wired LAN is not affected.
#   ssh root@192.168.5.1 'sh -s pin 36' < cfr_pin_channel.sh
MODE=${1:-status}; CH=${2:-36}; R=wireless.radio0_band1; BAK=/root/wireless.before_chanpin
[ "$(cat /tmp/sysinfo/board_name)" = "qcom,ipq5210-rdp499" ] || { echo "not the RDP499"; exit 1; }
show() {
    echo "uci: channel=$(uci -q get $R.channel) qacs_enable=$(uci -q get $R.qacs_enable) dcs_enable=$(uci -q get $R.dcs_enable) htmode=$(uci -q get $R.htmode)"
    iw dev | awk '/Interface/{i=$2} /channel/{print "live: " i ": " $0}' | sed 's/\t\+/ /g'
}
waitup() {
    i=0; while [ $i -lt 60 ]; do sleep 3; i=$((i+3)); [ "$(iw dev | grep -c ssid)" -ge 2 ] && break; done
    sleep 5; echo "ssids back after ~${i} s"
}
case "$MODE" in
status) show ;;
pin)
    case "$CH" in 36|40|44|48) ;; *) echo "channel $CH not tested (36/40/44/48 = the UHR80 block 36-48)"; exit 2 ;; esac
    [ -e $BAK ] || cp -a /etc/config/wireless $BAK
    uci set $R.channel=$CH; uci set $R.qacs_enable=0; uci set $R.dcs_enable=0; uci commit wireless
    echo "--- wifi reload"; wifi reload; waitup
    [ -x /etc/init.d/txpower-reapply ] && /etc/init.d/txpower-reapply start
    show ;;
unpin)
    [ -e $BAK ] || { echo "no backup $BAK"; exit 1; }
    cp -a $BAK /etc/config/wireless; echo "--- wifi reload"; wifi reload; waitup
    [ -x /etc/init.d/txpower-reapply ] && /etc/init.d/txpower-reapply start
    show ;;
*) echo "usage: $0 status|pin [36|40|44|48]|unpin"; exit 2 ;;
esac
