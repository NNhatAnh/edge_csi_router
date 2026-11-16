#!/bin/sh
# Khởi động toàn diện hệ thống VNPT Edge CSI & Model V6 Native
set -eu

PORT="${1:-5243}"

echo "=========================================================="
echo "   KHỞI ĐỘNG HỆ THỐNG VNPT EDGE CSI & MODEL V6 NATIVE    "
echo "=========================================================="

# 1. Dọn dẹp tiến trình cũ, zombie và file lock tồn đọng
echo "[1/3] Dọn dẹp tiến trình cũ và giải phóng tài nguyên phần cứng..."
killall -9 wake-score-web wake_native cfr_test_app edge_csi_daemon 2>/dev/null || true
rm -f /tmp/cfr_spool.lock /tmp/wake_ready_* /tmp/edge_csi_dump.bin 2>/dev/null || true
echo 0 > /sys/kernel/debug/ieee80211/phy00/ath12k_hw0/enable_cfr 2>/dev/null || true
sleep 1

# 2. Động cơ Model V6 Native M5R
echo "[2/3] Chuẩn bị môi trường cho Model V6 Native (đã nhúng trực tiếp vào Daemon)..."
CURRENT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
chmod +x "$CURRENT_DIR/edge_csi_daemon" "$CURRENT_DIR/capture_wake.sh" "$CURRENT_DIR/wake_native" 2>/dev/null || true

# 3. Khởi động Edge CSI Daemon chính (Port 5243)
echo "[3/3] Khởi động Edge CSI Daemon trên cổng $PORT..."
cd "$CURRENT_DIR"
exec ./edge_csi_daemon --web-port "$PORT"
