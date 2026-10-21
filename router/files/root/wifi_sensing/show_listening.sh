#!/bin/sh
# show_listening.sh — "cách A" (ghi chú dự án, router/IPC_TRIGGER.md): khi vẫy tay, phát chuỗi event ubus
# kws.listening start → tick (mỗi PING_MS) → stop, giống router_audio khi nghe "hey igate", để router_display
# hiện trạng thái "đang nghe". CHỈ HIỂN THỊ: mic không mở, không có STT (việc đó cần router_audio nối
# wifi_sensing.gesture vào listen.on_manual_trigger()).
# Định dạng theo router_audio_src/ubus_pub.c + kws_listen.cpp: start bắt buộc có "score" (panel từ chối nếu
# thiếu), stop có "reason". Gọi bởi daemon qua UCI action với WS_P; tham số môi trường:
#   WS_LISTEN_MS (5000)  WS_PING_MS (500)  WS_SOURCE (wifi-sensing)
P=${WS_P:-1.0}; LISTEN=${WS_LISTEN_MS:-5000}; PING=${WS_PING_MS:-500}; SRC=${WS_SOURCE:-wifi-sensing}
LOCK=/tmp/wifi_sensing.listen.lock
mkdir "$LOCK" 2>/dev/null || exit 0   # đang có phiên hiển thị: bỏ qua, như router_audio không lồng phiên
trap 'rmdir "$LOCK"' EXIT
S=$(( 900000 + $(date +%s) % 100000 ))  # session riêng, xa dãy 1,2,3… của router_audio
send() { ubus send kws.listening "$1"; }
send "{\"state\":\"start\",\"session\":$S,\"seq\":0,\"elapsed_ms\":0,\"remaining_ms\":$LISTEN,\"score\":$P,\"source\":\"$SRC\"}"
seq=1; t=0
while [ $(( t + PING )) -lt "$LISTEN" ]; do
	usleep $(( PING * 1000 ))
	t=$(( t + PING ))
	send "{\"state\":\"tick\",\"session\":$S,\"seq\":$seq,\"elapsed_ms\":$t,\"remaining_ms\":$(( LISTEN - t )),\"source\":\"$SRC\",\"level\":0}"
	seq=$(( seq + 1 ))
done
usleep $(( (LISTEN - t) * 1000 ))
send "{\"state\":\"stop\",\"session\":$S,\"seq\":$seq,\"elapsed_ms\":$LISTEN,\"remaining_ms\":0,\"reason\":\"timeout\",\"source\":\"$SRC\"}"
