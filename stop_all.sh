#!/bin/sh
# Dừng toàn bộ hệ thống VNPT Edge CSI và giải phóng tài nguyên router
set -eu

echo "=========================================================="
echo "    DỪNG HỆ THỐNG VNPT EDGE CSI & GIẢI PHÓNG TÀI NGUYÊN   "
echo "=========================================================="

# 1. Dừng các dịch vụ OpenWrt procd cũ để tránh procd tự hồi sinh
for svc in edge_csi wake_8082 wake-8082-v7-bundle wake-score; do
    [ -x "/etc/init.d/$svc" ] && "/etc/init.d/$svc" stop 2>/dev/null || true
done

# 2. Dừng supervisor giám sát cổng 8082 nếu có
if [ -f /tmp/wake_8082_supervisor.pid ]; then
    kill "$(cat /tmp/wake_8082_supervisor.pid)" 2>/dev/null || true
    rm -f /tmp/wake_8082_supervisor.pid
fi

echo "[1/2] Dừng các tiến trình và xóa file lock..."
killall -9 edge_csi_daemon wake-score-web wake_native cfr_test_app 2>/dev/null || true
rm -rf /tmp/cfr_spool.lock 2>/dev/null || true
rm -f /tmp/wake_ready_* /tmp/edge_csi_dump.bin 2>/dev/null || true

echo "[2/2] Tắt CFR capture trên driver ath12k..."
echo 0 > /sys/kernel/debug/ieee80211/phy00/ath12k_hw0/enable_cfr 2>/dev/null || true

echo "[✓] Đã dừng toàn bộ dịch vụ và giải phóng tài nguyên router an toàn."
