# Edge CSI Router

Backend C++17 cho Qualcomm ath12k Wi-Fi 8 CFR relay, kèm web dashboard chạy trực tiếp
trên router. Backend giải mã dữ liệu CFR nhận được; nó không tự bật cấu hình capture
trên driver và cũng không tự chạy `cfr_test_app`.

## Kiến trúc

```text
Qualcomm CFR driver
        │  CFR capture do capture workflow / cfr_test_app tạo
        ▼
Relay nhị phân
        ├── File relay trong /tmp (watcher mặc định)
        └── TCP relay ingest :10000 (nguồn ngoài tùy chọn)
                    │
                    ▼
        CSIParser: framing → I/Q → amplitude/phase/motion
                    │
                    ▼
   WebServer: HTTP :8080, REST-like JSON và SSE /events
                    │
                    ▼
    Trình duyệt: dashboard 6 tab, vẽ biểu đồ bằng Canvas
```

Các phần chính:

- `src/main.cpp`: đọc tham số chạy, phục vụ replay và theo dõi relay file.
- `src/csi_parser.cpp`: ráp bản tin relay, kiểm tra kích thước/header và giải mã I/Q.
- `src/web_server.cpp`: HTTP dashboard/API, Server-Sent Events (SSE) và TCP ingest.
- `web/`: HTML/CSS/JavaScript cho dashboard; biểu đồ được vẽ trong trình duyệt, không
  cần thư viện chart/CDN bên ngoài.
- `tests/csi_parser_test.cpp`: kiểm thử parser bằng dữ liệu relay.

## Thuật toán và định dạng dữ liệu

### Luồng đầu vào

Parser xử lý một luồng byte liên tục, không phụ thuộc ranh giới giữa các lần `read()`:

1. Tìm magic relay `0xDEADBEAF`; phần relay header cố định dài 22 byte.
2. Đọc kích thước metadata little-endian tại header và bỏ qua phần metadata nếu có.
3. Kiểm tra CFR magic `0xC0DE00BA`; kích thước CFR header được mã hóa theo đơn vị
   word 32-bit. Parser kiểm tra giới hạn kích thước, số chain và payload trước khi
   truy cập dữ liệu.
4. Đọc payload chain-major; mỗi tone gồm `int16` I và `int16` Q little-endian.
   Kết thúc record là `0xBEAFDEAD`.
5. Nếu record chưa đủ do TCP/file mới có một phần dữ liệu, parser giữ lại byte đang
   chờ. Nếu framing không hợp lệ, parser tìm lại magic relay để đồng bộ.

Với mỗi tone, backend tính:

```text
amplitude = sqrt(I * I + Q * Q)
phase     = atan2(Q, I)                  // radian
```

#### Tính motion score

1. Ghép amplitude của mọi tone trong mọi antenna chain thành vector frame hiện tại.
   Frame đầu chỉ lưu vector làm tham chiếu; chưa có score.
2. Chuẩn hóa từng vector bằng trung bình amplitude của chính frame đó:
   `normalized[i] = amplitude[i] / (mean(amplitude) + 1e-6)`.
   Việc này giảm ảnh hưởng thay đổi scale tổng thể (ví dụ gain) trong khi vẫn giữ hình
   dạng tương đối giữa các tone.
3. Tính tương quan Pearson giữa vector hiện tại và vector frame trước:
   `r = covariance(current, previous) / sqrt(variance(current) * variance(previous))`.
   Giá trị `r` được chặn trong `[-1, 1]`. Nếu cả hai vector gần như hằng, score là 0
   khi hình dạng chuẩn hóa giống nhau, nếu không thì score tức thời là 1.
4. Đổi tương quan thành score thay đổi `instant_score = 1 - r`: score thấp nghĩa là
   hình dạng amplitude hai frame giống nhau; score cao nghĩa là phân bố amplitude
   giữa các tone thay đổi. Score lý thuyết nằm trong `[0, 2]`.
5. Làm mượt bằng median của tối đa 30 `instant_score` mới nhất. Chỉ khi đủ 30 mẫu,
   `motion_ready` mới thành `true` và baseline mới được xét.

#### Tự hiệu chuẩn chỉ khi ổn định

Không dùng mù quáng 50 score đầu. Sau khi `motion_ready`:

1. Xét score median hiện tại như một ứng viên baseline nếu score không vượt `0.1`.
   Score lớn hơn `0.1` được xem là đang biến động: xóa chuỗi ứng viên ổn định và chờ
   score thấp trở lại.
2. Nếu score thay đổi so với ứng viên trước đó hơn `0.02` theo trị tuyệt đối, coi là
   bước tăng/giảm đột ngột. Chuỗi ứng viên bị khởi động lại từ score mới; các ứng viên
   trước bước thay đổi không được đưa vào baseline.
3. Khi có đủ 50 ứng viên liên tiếp, tính độ lệch chuẩn tổng thể của 50 score. Nếu
   `std > 0.01`, không hiệu chuẩn; hủy chuỗi và bắt đầu lại từ score hiện tại. Nếu
   `std <= 0.01`, 50 mẫu đó được chấp nhận là nền ổn định.
4. Tính trung bình `mu` và độ lệch chuẩn nền `sigma` trên 50 mẫu được chấp nhận. Để
   tránh ngưỡng bằng 0 khi nền quá yên, dùng `sigma_floor = max(sigma, 0.001)`, sau đó:

   ```text
   threshold_high = mu + 5 * sigma_floor
   threshold_low  = mu + 3 * sigma_floor
   ```

   Chỉ sau bước này `motion_calibrated` thành `true`. Nếu tín hiệu tiếp tục biến động,
   detector vẫn chạy và hiển thị score nhưng không tuyên bố baseline/hiện diện đã hiệu
   chuẩn; số `motion_calibration_frames` là số ứng viên ổn định hiện tại trên 50.

Các giới hạn `0.1`, `0.02` và `0.01` áp dụng cho thang score tương quan đã làm mượt,
không phải dBm. Chúng là điều kiện vận hành để loại chuyển động/nhiễu rõ rệt khỏi
baseline, không phải ngưỡng vật lý phổ quát; môi trường vô tuyến khác nhau có thể cần
tinh chỉnh sau khi đo dữ liệu thực. Vì baseline được dùng làm trạng thái “yên tĩnh”,
khi khởi động hoặc hiệu chuẩn lại nên để môi trường không có chuyển động trong lúc
dashboard báo đang chờ ổn định.

#### Phát hiện và hiệu chuẩn lại

- Khi score `> threshold_high`, bật `presence_detected` ngay và xóa bộ đếm score thấp.
- Khi score `< threshold_low`, tăng bộ đếm; sau 15 score thấp liên tiếp thì tắt
  `presence_detected`.
- Khi score nằm giữa hai ngưỡng, giữ nguyên trạng thái hysteresis nhưng xóa bộ đếm
  thấp liên tiếp. Cách này tránh tắt do một dao động ngắn quanh ngưỡng.
- Nút **Hiệu chuẩn lại** gửi `POST /api/calibrate`. Server xóa vector tham chiếu,
  cửa sổ 30 score, ứng viên nền, hai ngưỡng và trạng thái hiện diện; capture/driver
  không bị dừng hay thay đổi. Sau đó hệ thống phải tích lũy lại cửa sổ score và chờ
  đủ 50 score ổn định mới có baseline mới. API trả `{"status":"calibrating",...}` và
  SSE phát trạng thái đang hiệu chuẩn ngay khi có frame gần nhất.

Dashboard hiển thị riêng giai đoạn tích lũy cửa sổ, số ứng viên ổn định, trạng thái
hiệu chuẩn và hai ngưỡng. `motion_calibration_stable` cho biết score gần nhất đang
được chấp nhận vào chuỗi ổn định hoặc baseline đã hiệu chuẩn xong; nó không khẳng
định detector đã calibrated (trường đó là `motion_calibrated`).

### Đọc file relay vòng

Mặc định daemon quét `/tmp` mỗi 50 ms để tìm file `.bin` mới nhất có tên bắt đầu bằng
`cfr_dump_phy00_`. Nếu nhận diện được tiến trình `cfr_test_app` đang mở file đó, watcher
đọc vị trí ghi từ `/proc/<pid>/fdinfo` để theo file ring/preallocated, kể cả khi kích
thước file không tăng. Nếu không có writer đang mở, watcher xử lý file theo kiểu append.

Watcher chỉ đọc relay file. Nó không thay đổi `enable_cfr`, station capture settings,
tham số driver, cũng không khởi chạy/dừng `cfr_test_app`. Phải bật và chạy capture
workflow riêng theo hướng dẫn của capture kit. Nếu producer dừng hoặc file không có
record mới, dashboard sẽ giữ frame cuối cùng hoặc ở trạng thái chờ; `has_data: true`
chỉ xác nhận đã nhận ít nhất một frame, không đảm bảo frame đó đang được cập nhật.

### Trình tự chạy từ lúc khởi động đến dashboard

1. `main.cpp` đọc tham số, khởi động HTTP/SSE và TCP ingest; sau đó chạy watcher file
   mặc định (trừ khi chọn replay).
2. Watcher lấy byte mới từ relay file, TCP handler lấy byte từ socket, còn chế độ replay
   đọc tuần tự file capture. Mỗi nguồn có `CSIParser` riêng để giữ buffer framing.
3. `CSIParser` tìm ranh giới record trong stream, giữ phần record chưa đầy đủ qua lần
   đọc tiếp theo, xác thực độ dài và giải mã timestamp, PPDU, metadata, I/Q, amplitude
   và phase. Record lỗi không được đưa vào dashboard.
4. Frame giải mã được gửi đến `WebServer::publish`. Tất cả nguồn dùng chung một
   `MotionDetector` có khóa bảo vệ; vì thế score/baseline không bị nhân đôi khi đổi
   nguồn và recalibrate sẽ reset detector chung.
5. `MotionDetector` so sánh vector amplitude với frame trước, cập nhật cửa sổ score,
   hiệu chuẩn ổn định hoặc đánh giá hysteresis; server lưu frame mới nhất và phát sự
   kiện `csi` qua SSE.
6. Trình duyệt kết nối `/events`, cập nhật metric/trạng thái/biểu đồ; `/latest_csi.json`
   trả snapshot cùng các trường hiệu chuẩn. Nếu producer dừng, HTTP vẫn có thể hoạt
   động nhưng frame cuối không đại diện dữ liệu realtime.

### Đầu ra và các tab

- `GET /health.json`: tình trạng server và việc đã nhận frame hay chưa.
- `GET /latest_csi.json`: frame CSI mới nhất dạng JSON.
- `GET /events`: SSE stream; mỗi frame phát ra dưới dạng sự kiện `csi`.
- `POST /api/calibrate`: xóa baseline và khởi động lại quá trình hiệu chuẩn ổn định.
- `motion_score`, trạng thái hiệu chuẩn, ngưỡng cao/thấp và `presence_detected` được
  đưa trong JSON frame/SSE. Dashboard hiển thị score, hai ngưỡng và trạng thái
  hiệu chuẩn/chuyển động.
- Dashboard có các tab Tổng quan, Biên độ CSI, Pha CSI, Chòm sao I/Q, So sánh antenna
  và Thông tin gói.

## Triển khai lên router qua SCP

Ví dụ dưới đây dùng router `192.168.10.1`, user `root` và PowerShell trên Windows.
Thay IP nếu router của bạn khác. Trên firmware này không có `sftp-server`, vì vậy dùng
`scp -O` (SCP protocol kiểu cũ qua SSH), không dùng SCP mặc định vốn thử SFTP trước.

### 1. Build trước khi upload

Chạy các lệnh build ARM64 ở trên. Sau khi build thành công, executable duy nhất cần đưa
lên router là `build/edge_csi_daemon`; giao diện cần có trong thư mục `web/` cùng
cấp với executable khi chạy.

### 2. Copy file vào thư mục staging trên router

```powershell
ssh root@192.168.10.1 "mkdir -p /tmp/edge_csi_deploy/web"
scp -O .\build\edge_csi_daemon root@192.168.10.1:/tmp/edge_csi_deploy/edge_csi_daemon.new
scp -O .\build\web\index.html .\build\web\script.js .\build\web\style.css root@192.168.10.1:/tmp/edge_csi_deploy/web/
```

Staging trước giúp kiểm tra file đã chuyển xong rồi mới thay executable đang dùng.

### 3. Backup, thay file và khởi động

SSH vào router:

```powershell
ssh root@192.168.10.1
```

Sau đó chạy trên router. Nếu daemon đang hoạt động, lấy PID bằng `pidof` và gửi
`SIGTERM` cho đúng PID trước khi thay file. Các lệnh backup bên dưới giữ lại bản hiện
tại với timestamp để có thể rollback:

```sh
cd /root
pidof edge_csi_daemon
kill -TERM <PID>
stamp="$(date +%Y%m%d%H%M%S)"
[ ! -e ./edge_csi_daemon ] || cp -p ./edge_csi_daemon "./edge_csi_daemon.previous.$stamp"
[ ! -d ./web ] || cp -a ./web "./web.previous.$stamp"
mkdir -p ./web
cp /tmp/edge_csi_deploy/edge_csi_daemon.new ./edge_csi_daemon
chmod 755 ./edge_csi_daemon
cp /tmp/edge_csi_deploy/web/* ./web/
./edge_csi_daemon >/tmp/edge_csi_daemon.log 2>&1 </dev/null &
echo $!
```

Trên BusyBox `ash` của router, chạy nền bằng `&` và redirect như trên; firmware này
không cài `nohup`. Daemon mặc định dùng HTTP/SSE cổng `8080`, TCP ingest cổng `10000`,
phục vụ asset từ `./web` và theo dõi `/tmp/cfr_dump_phy00_*.bin`. Để đổi tham số, xem
`./edge_csi_daemon --help`.

### 4. Kiểm tra sau triển khai

Chạy trên router:

```sh
pidof edge_csi_daemon
netstat -lntp | grep -E ':(8080|10000) '
wget -qO- http://127.0.0.1:8080/health.json
wget -qO- http://127.0.0.1:8080/latest_csi.json
```

Mở `http://192.168.10.1:8080/` trong trình duyệt. Để xác nhận **stream đang LIVE**,
không chỉ xác nhận backend chạy: bảo đảm `cfr_test_app`/capture workflow đang tạo relay
record, kết nối `/events` hoặc đọc `/latest_csi.json` nhiều lần và kiểm tra
`chip_tsf_us`/`phy_ppdu_id` thay đổi. Nếu capture producer chưa chạy, HTTP có thể vẫn
`status: ok` nhưng `has_data` là `false`, hoặc API tiếp tục trả frame cũ.

### Rollback

Nếu bản mới không chạy, dừng PID mới, rồi khôi phục executable và thư mục web từ các
file `*.previous.<timestamp>` đã tạo ở bước trên. Không xóa backup cho đến khi đã xác
nhận daemon mới hoạt động ổn định.

## Chế độ khác và tham số

### Theo dõi file relay

Giá trị mặc định:

```sh
./edge_csi_daemon
```

Các tham số có thể cấu hình:

```text
--web-port PORT       HTTP và SSE (mặc định 8080)
--ingest-port PORT    TCP relay ingest (mặc định 10000)
--web-root PATH       Thư mục chứa index.html, style.css và script.js (mặc định ./web)
--watch-dir PATH      Thư mục relay file (mặc định /tmp)
--prefix PREFIX       Prefix tên relay file (mặc định cfr_dump_phy00_)
--replay FILE         Replay relay capture để thử nghiệm
```

### TCP ingest

Thiết bị gửi có thể gửi luồng relay nhị phân vào cổng TCP `10000`. Có thể ghép nhiều
record liên tiếp; ranh giới TCP không nhất thiết trùng ranh giới record. Backend vẫn
chỉ chấp nhận định dạng relay ở trên, không nhận text log hoặc I/Q rời.

### Replay capture

Để phát lại file relay đã lưu và xem trên dashboard:

```sh
./edge_csi_daemon --web-root ./web --replay /path/to/test2.bin
```

Khi dùng `--replay` mà không chỉ định `--watch-dir`, watcher file trực tiếp được tắt.
Replay giữ dashboard hoạt động cho đến khi nhận tín hiệu dừng.

## Lưu ý vận hành

HTTP và TCP ingest bind trên mọi interface router, không có authentication hoặc TLS.
Chỉ sử dụng trong LAN/firewall tin cậy, không mở các cổng này ra WAN. Backend không tự
bật CFR capture; dùng capture kit để cấu hình và khởi chạy producer theo quy trình riêng.
