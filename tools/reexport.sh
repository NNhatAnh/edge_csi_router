#!/bin/sh
# Tạo lại export_csi/csi_iq.csv (+ raw/) từ relay.bin cho mọi lượt thiếu (gói gửi đi bỏ các file này cho nhẹ).
#   sh tools/reexport.sh
set -eu
cd "$(dirname "$0")/.."
EXPORT=$(ls kit/RDP499_*/scripts/cfr_export.py)
for d in data/raw/*/; do
	[ -f "$d/relay.bin" ] && [ ! -f "$d/export_csi/csi_iq.csv" ] || continue
	echo "== $d"
	python3 "$EXPORT" "$d/relay.bin" "$d/export_csi" | tail -1
done
