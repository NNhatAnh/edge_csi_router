#!/bin/sh
# Khởi động cùng lúc VNPT Edge CSI (cổng 5243) và Wake 8082 V7 nhận diện chữ V (cổng 8082)
# từ chính thư mục chứa file này. Dùng: sh start_all.sh [PORT_5243]
set -eu

PORT="${1:-5243}"
WAKE_PORT=8082
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SUPERVISOR_PID=/tmp/wake_8082_supervisor.pid
WAKE_LOG=/tmp/wake_8082.log
LAN_IP=$(ip -4 -o addr show br-lan 2>/dev/null | awk '{split($4,a,"/"); print a[1]; exit}')
LAN_IP=${LAN_IP:-192.168.5.1}

echo "=========================================================="
echo "   KHỞI ĐỘNG VNPT EDGE CSI ($PORT) & WAKE 8082 V7 CHỮ V   "
echo "   Thư mục: $DIR"
echo "=========================================================="

for f in edge_csi_daemon wake-score-web wake_native capture_wake.sh score.html models/M5R_logistic.txt web/index.html; do
    [ -e "$DIR/$f" ] || { echo "[!] Thiếu $DIR/$f" >&2; exit 1; }
done

# 1. Dừng các dịch vụ procd cũng dùng cổng 8082, nếu không chúng tự hồi sinh và tranh cổng.
echo "[1/4] Dừng dịch vụ 8082 cũ và tiến trình tồn đọng..."
for svc in wake_8082 wake-8082-v7-bundle wake-score; do
    [ -x "/etc/init.d/$svc" ] && "/etc/init.d/$svc" stop >/dev/null 2>&1 || true
done
if [ -f "$SUPERVISOR_PID" ]; then
    kill "$(cat "$SUPERVISOR_PID")" 2>/dev/null || true
    rm -f "$SUPERVISOR_PID"
fi
killall -9 wake-score-web wake_native cfr_test_app edge_csi_daemon 2>/dev/null || true
rm -rf /tmp/cfr_spool.lock 2>/dev/null || true
rm -f /tmp/wake_ready_* /tmp/edge_csi_dump.bin 2>/dev/null || true
echo 0 > /sys/kernel/debug/ieee80211/phy00/ath12k_hw0/enable_cfr 2>/dev/null || true
sleep 1

chmod +x "$DIR/edge_csi_daemon" "$DIR/wake-score-web" "$DIR/wake_native" "$DIR/capture_wake.sh" 2>/dev/null || true

# 2. Khởi động 8082 dưới một vòng giám sát nhỏ: tự chạy lại nếu wake-score-web thoát.
echo "[2/4] Khởi động Wake 8082 V7 (log: $WAKE_LOG)..."
(
    trap 'kill "$child" 2>/dev/null; exit 0' TERM INT HUP
    child=
    while :; do
        "$DIR/wake-score-web" "$DIR" "$WAKE_PORT" "$DIR/score.html" >>"$WAKE_LOG" 2>&1 &
        child=$!
        wait "$child" || true
        echo "$(date '+%F %T') wake-score-web thoát, chạy lại sau 2 s" >>"$WAKE_LOG"
        sleep 2
    done
) </dev/null &
echo $! > "$SUPERVISOR_PID"

i=0
while [ $i -lt 20 ]; do
    if netstat -tln 2>/dev/null | grep -q ":$WAKE_PORT "; then break; fi
    i=$((i+1)); busybox usleep 250000
done
if netstat -tln 2>/dev/null | grep -q ":$WAKE_PORT "; then
    echo "      [✓] 8082 đang nghe: http://$LAN_IP:$WAKE_PORT/"
else
    echo "      [!] 8082 chưa nghe sau 5 s. Xem $WAKE_LOG"
fi

# 3. Thông tin truy cập
echo "[3/4] Khởi động Edge CSI Daemon trên cổng $PORT..."
echo "=========================================================="
echo "   HỆ THỐNG ĐÃ SẴN SÀNG:"
echo "   - Dashboard chính:  http://$LAN_IP:$PORT"
echo "   - Cử chỉ chữ V:     tab 'Cử chỉ chữ V' (nhúng 8082, thiết bị lấy từ tab Tổng quan)"
echo "   - Web 8082 độc lập: http://$LAN_IP:$WAKE_PORT/"
echo "   - Dừng tất cả:      sh $DIR/stop_all.sh"
echo "=========================================================="

# 4. Edge CSI Daemon. Script này chờ daemon; Ctrl+C hoặc SIGTERM dừng cả 5243 và 8082.
echo "[4/4] Đang chạy. Ctrl+C để dừng cả hai cổng."
cd "$DIR"
./edge_csi_daemon --web-port "$PORT" &
EDGE_PID=$!
cleanup() {
    trap - EXIT INT TERM HUP
    kill "$EDGE_PID" 2>/dev/null || true
    kill "$(cat "$SUPERVISOR_PID" 2>/dev/null)" 2>/dev/null || true
    rm -f "$SUPERVISOR_PID"
    killall wake-score-web wake_native 2>/dev/null || true
    rm -rf /tmp/cfr_spool.lock 2>/dev/null || true
    echo 0 > /sys/kernel/debug/ieee80211/phy00/ath12k_hw0/enable_cfr 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT TERM HUP
wait "$EDGE_PID"
