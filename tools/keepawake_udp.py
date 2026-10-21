"""Giữ station thức khi thu CSI: bắn UDP đều tới IP của nó (port 9 = discard).
Phone bỏ qua ping và video tải theo đợt; luồng downlink đều buộc nó nhận khung liên tục.
Đo 2026-10-20 với iPhone: 1200 B mỗi 5 ms (~1.9 Mbit/s) -> 17.4 CSI/s ở chu kỳ 50 ms.
    python3 tools/keepawake_udp.py <station ip> [giây]"""
import socket, sys, time

ip = sys.argv[1]
end = time.time() + (float(sys.argv[2]) if len(sys.argv) > 2 else 1e9)
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
while time.time() < end:
    s.sendto(b"\0" * 1200, (ip, 9))
    time.sleep(0.005)
