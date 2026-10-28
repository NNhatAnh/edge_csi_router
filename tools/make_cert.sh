#!/bin/sh
# Chứng chỉ HTTPS cho trang vẽ trên iPhone (iOS chỉ cho đọc gyro trên HTTPS): CA tự tạo + cert server có SAN IP của PC.
# Cài data/certs/public/ca.crt lên iPhone 1 lần (Hồ sơ → Cài đặt; Cài đặt chung → Giới thiệu → Cài đặt tin cậy chứng nhận → bật).
#   sh tools/make_cert.sh [192.168.11.178 [192.168.5.240 ...]]   (IP đầu = CN; mọi IP vào SAN)
set -e
IP=${1:-192.168.11.178}
SAN=$(for i in "${@:-$IP}"; do printf "IP:%s," "$i"; done); SAN=${SAN%,}
D=$(dirname "$0")/../data/certs
mkdir -p "$D/public"; cd "$D"
[ -f ca.key ] || openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out public/ca.crt -days 3650 -subj "/CN=VNPT WiFi Sensing local CA"
openssl req -newkey rsa:2048 -nodes -keyout server.key -out server.csr -subj "/CN=$IP"
printf "subjectAltName=%s\nextendedKeyUsage=serverAuth\nbasicConstraints=CA:FALSE\n" "$SAN" > ext.cnf
openssl x509 -req -in server.csr -CA public/ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 800 -sha256 -extfile ext.cnf  # iOS: ≤ 825 ngày
rm -f server.csr ext.cnf
echo "OK: $D/server.{crt,key}, CA để cài lên iPhone: $D/public/ca.crt"
