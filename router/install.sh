#!/bin/sh
# Cài daemon wifi_sensing lên router (chạy trên PC):  sh install.sh [192.168.4.1]
# Không ghi đè /etc/config/wifi_sensing đã có (giữ cấu hình người dùng). Gỡ: sh install.sh --remove [ip]
set -eu
[ "${1:-}" = --remove ] && { B=${2:-192.168.4.1}; ssh root@$B '/etc/init.d/wifi_sensing stop; /etc/init.d/wifi_sensing disable;
  rm -rf /root/wifi_sensing /etc/init.d/wifi_sensing /tmp/wifi_sensing.status; echo "đã gỡ (giữ /etc/config/wifi_sensing)"'; exit; }
B=${1:-192.168.4.1}
HERE=$(cd "$(dirname "$0")" && pwd)
[ -x "$HERE/build/wifi_sensing" ] || { echo "chưa build: make MODEL=../models/<ts>"; exit 1; }
SSH="ssh -o BatchMode=yes root@$B"
$SSH 'mkdir -p /root/wifi_sensing'
$SSH 'cat > /root/wifi_sensing/wifi_sensing.new && chmod 755 /root/wifi_sensing/wifi_sensing.new' < "$HERE/build/wifi_sensing"
$SSH 'cat > /root/wifi_sensing/model_report.json' < "$HERE/build/model_report.json"
$SSH 'cat > /root/wifi_sensing/show_listening.sh && chmod 755 /root/wifi_sensing/show_listening.sh' < "$HERE/files/root/wifi_sensing/show_listening.sh"
$SSH 'cat > /etc/init.d/wifi_sensing && chmod 755 /etc/init.d/wifi_sensing' < "$HERE/files/etc/init.d/wifi_sensing"
$SSH '[ -f /etc/config/wifi_sensing ] || cat > /etc/config/wifi_sensing' < "$HERE/files/etc/config/wifi_sensing"
$SSH '/etc/init.d/wifi_sensing stop 2>/dev/null; mv /root/wifi_sensing/wifi_sensing.new /root/wifi_sensing/wifi_sensing;
  /etc/init.d/wifi_sensing enable; /etc/init.d/wifi_sensing start; sleep 3; cat /tmp/wifi_sensing.status 2>/dev/null;
  logread -e wifi_sensing | tail -5'
