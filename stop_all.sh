#!/bin/sh
# Dừng toàn bộ hệ thống VNPT Edge CSI và giải phóng tài nguyên router
set -eu

echo "=========================================================="
echo "    DỪNG HỆ THỐNG VNPT EDGE CSI & GIẢI PHÓNG TÀI NGUYÊN   "
echo "=========================================================="

echo "[1/2] Dừng các tiến trình và xóa file lock..."
killall -9 edge_csi_daemon wake_native cfr_test_app 2>/dev/null || true
rm -f /tmp/cfr_spool.lock /tmp/wake_ready_* /tmp/edge_csi_dump.bin 2>/dev/null || true

echo "[2/2] Tắt CFR capture trên driver ath12k..."
echo 0 > /sys/kernel/debug/ieee80211/phy00/ath12k_hw0/enable_cfr 2>/dev/null || true

echo "[✓] Đã dừng toàn bộ dịch vụ và giải phóng tài nguyên router an toàn."
