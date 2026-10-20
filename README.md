# Edge CSI Router

Backend C++17 cho Qualcomm ath12k Wi-Fi 8 CFR relay, kèm web dashboard chạy trực tiếp
trên router. Backend giải mã dữ liệu CFR nhận được; nó không tự bật cấu hình capture
trên driver và cũng không tự chạy `cfr_test_app`.

### Kiến trúc hệ thống

```text
Qualcomm CFR Driver (ath12k)
        │  CFR capture do capture workflow / cfr_test_app tạo ra
        ▼
Relay nhị phân
        ├── File relay trong /tmp (watcher đọc qua /proc/<pid>/fdinfo)
        └── TCP relay ingest :10000 (stream ngoài tùy chọn)
                    │
                    ▼
        CSIParser: Framing → Bóc tách TLV → I/Q raw 16-bit → Amplitude & Phase
                    │
                    ▼
       ┌─────────────┴─────────────┐
       ▼                           ▼
 MotionDetector               BreathDetector
 - Vector Amplitude toàn dải  - Differential phase (Chain0 - Chain1)
 - Chuẩn hóa gain             - Unwrap phase & Linear detrending
 - Pearson correlation        - Khớp phổ lượng giác (0.10 - 0.55 Hz)
 - Top-10% subcarrier delta   - Autocorrelation Regularity (RuView)
 - Median window 30 mẫu       - Weighted Subcarrier Fusion
 - Auto-calibration nền tĩnh  - Nhận diện nhịp thở người:
 - Bắt di chuyển & rung chân  • Ngưỡng pha nhạy: span >= 0.02 rad
                              • Phân tích chu kỳ nhịp & tự tương quan
       └─────────────┬─────────────┘
                     │
                     ▼
    WebServer: Bộ hợp nhất đúng 3 trạng thái cốt lõi
    ├── CÓ CHUYỂN ĐỘNG (human_moving) : Chuyển động người / đi lại / cử động / rung chân
    ├── CÓ NGƯỜI       (human_present): Sóng thở vi mô đặc trưng sinh học / đang ngủ
    └── KHÔNG CÓ NGƯỜI (empty_room)   : Phòng trống tĩnh
                     │
                     ▼
    Web Dashboard (web/): REST API :8080 & SSE /events
    Canvas 7 tab trực quan, thuần JS/CSS, hiển thị đúng 3 nhãn chuẩn
```

Các thành phần chính trong mã nguồn:

- `src/main.cpp`: Khởi tạo luồng, xử lý tham số CLI, theo dõi offset ghi của tiến trình capture qua `/proc/<pid>/fdinfo` để đọc relay thời gian thực.
- `src/csi_parser.cpp`: Ráp khung tin relay (magic `0xDEADBEAF` ... `0xBEAFDEAD`), bóc tách metadata CFR Qualcomm và giải mã các mảng I/Q thô.
- `src/motion.cpp`: Đo lường chuyển động vĩ mô dựa trên mức độ biến dạng phổ biên độ qua hệ số tương quan Pearson và máy trạng thái tự cân chỉnh ngưỡng nền.
- `src/breath.cpp`: Đo lường vi chuyển động hô hấp qua vi sai pha giữa các antenna chain, khớp phổ lượng giác, tính độ tự tương quan chu kỳ trễ và phân loại hình thái sóng sinh học.
- `src/web_server.cpp`: Máy chủ HTTP nhúng, cung cấp endpoint SSE thời gian thực, đồng thời đóng vai trò **Bộ hợp nhất trạng thái** đảm bảo không bị mất dấu mục tiêu khi chuyển đổi giữa đi lại và ngồi yên.
- `web/`: Giao diện giám sát trực tiếp gồm 7 tab hiển thị (Tổng quan, Dạng sóng thở, Biên độ, Pha, I/Q constellation, So sánh antenna, Thông tin gói).

---

## Chi tiết các thuật toán hiện tại

### 1. Luồng phân giải CSI (`csi_parser.cpp`)

1. Tìm kiếm magic header `0xDEADBEAF`, kiểm tra header relay 22 byte.
2. Bóc tách CFR metadata và magic `0xC0DE00BA`.
3. Đọc dữ liệu I/Q chain-major (mỗi tone gồm 2 byte I signed 16-bit và 2 byte Q signed 16-bit).
4. Tính toán hai đặc trưng cơ bản trên từng tone:
   $$\text{amplitude} = \sqrt{I^2 + Q^2}, \quad \text{phase} = \text{atan2}(Q, I) \quad (\text{rad})$$

### 2. Thuật toán phát hiện chuyển động vĩ mô (`motion.cpp`)

- **Chuẩn hóa biên độ**: Ghép toàn bộ tone trên mọi chain thành vector $X$. Chuẩn hóa $X_i' = X_i / (\bar{X} + 10^{-6})$ để loại trừ biến động do mạch tự động chỉnh độ lợi (AGC).
- **Hệ số biến đổi phổ (Pearson Correlation)**:
  $$r = \frac{\text{cov}(X_t', X_{t-1}')}{\sigma_{X_t'} \sigma_{X_{t-1}'}}, \quad \text{instant\_score} = 1 - r$$
- **Làm mượt cửa sổ trượt**: Duy trì bộ đệm 30 frame gần nhất, lấy trung vị (median) tạo `motion_score`.
- **Hiệu chuẩn nền tự động (Auto-calibration)**:
  Tích lũy 50 mẫu liên tiếp thỏa mãn đồng thời: `score <= 0.1`, bước nhảy $\le 0.02$, độ lệch chuẩn $\sigma \le 0.01$. Ngưỡng quyết định thiết lập theo công thức:
  $$\text{threshold}_{\text{high}} = \mu + 5\sigma, \quad \text{threshold}_{\text{low}} = \mu + 3\sigma$$

### 3. Thuật toán vi chuyển động hô hấp & hình thái sóng (`breath.cpp`)

1. **Khử sai lệch tần số và pha (Differential Phase)**:
   $$\Delta\theta_k = \text{wrap}(\theta_{k, \text{chain0}} - \theta_{k, \text{chain1}})$$
   Do hai chuỗi anten dùng chung Local Oscillator (LO) và bộ chuyển đổi ADC, phép trừ vi sai triệt tiêu hoàn toàn sai số tần số sóng mang (CFO) và sai lệch xung nhịp lấy mẫu (SFO).
2. **Phase Unwrapping & Khử dốc tuyến tính**:
   Gỡ xoắn $2\pi$ liên tục theo thời gian và áp dụng hồi quy tuyến tính least-squares trên từng tone để triệt tiêu thành phần trôi dạt DC do giãn nở nhiệt.
3. **Khớp phổ lượng giác & Weighted Subcarrier Fusion**:
   Quét dải tần $0.10 - 0.55\text{ Hz}$ ($6 - 33\text{ BPM}$) với bước $0.01\text{ Hz}$. Các tone có chất lượng khớp phổ cao được tổng hợp theo trọng số $w_k = q_k^2$.
4. **Phân tích hình thái sóng sinh học (`WaveMorphology`)**:
   - **Độ dạt pha (Phase Span)**: Lồng ngực dao động trong dải $[0.12, 3.20]\text{ rad}$. Nếu $>35\%$ số tone có span $> 4.0\text{ rad}$, hệ thống kết luận là rèm cửa đung đưa (`curtain_detected`).
   - **Tự tương quan chu kỳ trễ (Autocorrelation Regularity)**: Đánh giá độ lặp lại chu kỳ của nhịp thở tại độ trễ $\tau = F_s / f_{\text{breath}}$.
   - **Biến thiên chu kỳ (RRV CV) & Độ bất đối xứng (Asymmetry)**: Nhịp thở người có tính bất đối xứng tự nhiên giữa pha hít vào và thở ra ($> 7\%$), biến thiên $\text{CV} \in [3.5\%, 35\%]$. Quạt máy cơ học có tính đối xứng tuyệt đối ($\text{CV} < 3.5\%$) sẽ bị loại bỏ (`mechanical_detected`).
   - **Nhận diện ngủ (`is_sleeping`)**: Kích hoạt khi nhịp thở chậm đều ($8 - 20\text{ BPM}$), $\text{regularity} \ge 0.45$ và $\text{RRV} \le 22\%$.

---

## Phân tích điểm nghẽn độ nhạy (Sensitivity Bottlenecks)

Qua kiểm thử thực tế, hệ thống nhận diện tốt khi người di chuyển mạnh hoặc thở sâu, nhưng gặp hiện tượng **kém nhạy với rung chân và thở nhẹ bình thường**:

1. **Hiện tượng rung chân không nhận diện được**:
   - **Pha loãng tương quan (Correlation Dilution)**: Tính Pearson trên toàn bộ mảng subcarrier làm cho các đường truyền trực tiếp (LoS không đổi chiếm đa số) lấn át biến động cục bộ do rung chân tạo ra.
   - **Bộ lọc Median 30 mẫu quá dài**: Làm phẳng các rung động có chu kỳ ngắn và biên độ nhỏ.
2. **Hiện tượng thở bình thường chưa hiện sóng**:
   - **Ngưỡng chặn cứng `kMinHumanPhaseSpan = 0.12 rad`**: Thở nhẹ ở khoảng cách từ 2m trở lên chỉ tạo ra độ lệch pha $0.03 - 0.08\text{ rad}$, dẫn đến toàn bộ subcarrier bị loại bỏ ngay từ bước lọc thô.
   - **Linear Detrending gọt mất thành phần tần số thấp**: Hồi quy tuyến tính trên cửa sổ ngắn có xu hướng khử luôn một phần dạng sóng thở hình sin.

---

## Lộ trình nâng cấp thuật toán đề xuất (Solution Roadmap)

Để giải quyết bài toán độ nhạy mà vẫn giữ nguyên khả năng chống nhiễu:

### 1. Đối với vi chuyển động cử động / Rung chân:
- **Subcarrier Variance Top-K**: Tính phương sai thời gian hoặc vi phân bậc một trên từng tone riêng biệt $D_k(t) = |A_k(t) - A_k(t-1)|$. Thay vì lấy trung bình toàn bộ, chỉ lấy trung bình của **Top 10% subcarrier có năng lượng dao động mạnh nhất**.
- **Chỉ số năng lượng ngắn hạn (Short-term Energy)**: Bổ sung cửa sổ 5-8 frame phản ứng nhanh với các dao động vi mô tức thời.

### 2. Đối với nhịp thở nhẹ / bình thường:
- **Hạ ngưỡng pha tối thiểu thích ứng (Adaptive Span Floor)**: Giảm ngưỡng sàn từ `0.12 rad` xuống `0.03 rad`, kết hợp đánh giá tỷ số tín hiệu trên nhiễu (SNR) thay vì áp ngưỡng cứng.
- **Thay Linear Detrending bằng IIR Bandpass Filter**: Sử dụng bộ lọc thông dải số (Bandpass Filter $0.1 - 0.5\text{ Hz}$) bậc 2, giữ nguyên biên độ sóng thở nhỏ mà không làm méo dạng sóng.
- **Trọng số chọn tone tối ưu (Subcarrier Selection)**: Lọc ra nhóm 5-8 subcarrier có tương quan chu kỳ tốt nhất để tổng hợp dạng sóng thay vì ép buộc điều kiện đồng thuận trên diện rộng.

3. **Phân biệt người với rèm cửa, quạt máy và gió lùa (WaveMorphology)**:
   Detector duy trì lịch sử dạng sóng thở trong cửa sổ 10–25 giây để trích xuất các đặc trưng hình thái sóng:
   - **Lọc rèm cửa đung đưa (`curtain_detected`)**: Lồng ngực người dịch chuyển 2–10 mm, tạo độ lệch pha đỉnh-đáy (Phase Span) trong dải `0.12–3.2 rad`. Rèm cửa đung đưa trong gió dịch chuyển 5–30 cm, tạo độ lệch pha lớn (`> 4.0 rad`). Nếu hơn 35% số tone có Phase Span vượt `4.0 rad`, hoặc Phase Span tổng hợp vượt `3.2 rad`, detector gán nhãn `curtain_detected` và từ chối hiện diện người.
   - **Lọc quạt điện / dao động cơ học (`mechanical_detected`)**: Quạt máy quay tuốc-năng hoặc máy móc cơ học dao động theo motor điện có chu kỳ cực kỳ cứng nhắc và dạng sóng sin đối xứng. Detector đo khoảng cách các đỉnh liên tiếp (IBI) và tính hệ số biến thiên chu kỳ `RRV CV = std(IBI) / mean(IBI)`. Nếu `CV < 3.5%` (`0.035`) kết hợp với độ bất đối xứng sườn dốc hít/thở `|Asymmetry| < 0.05`, detector xác định là máy móc nhân tạo và gán nhãn `mechanical_detected`.
   - **Lọc gió lùa / nhiễu ngẫu nhiên (`turbulence_detected`)**: Gió lùa giật cục làm đồ vật rung lắc ngẫu nhiên, tạo biến thiên chu kỳ rất lớn (`CV > 35%`) hoặc không tìm đủ tối thiểu 2 đỉnh trong cửa sổ đánh giá.
   - **Xác nhận người thở thực tế (`human_present`)**: Nhịp thở người thỏa mãn đồng thời: IBI trung bình 1.8–10.0 giây, biến thiên chu kỳ tự nhiên `3.5% <= CV <= 35%`, Phase Span trong khoảng `0.12–3.2 rad`, và có tính bất đối xứng sinh học (thời gian hít vào nhanh hơn thở ra).

4. **Khóa nhịp thở khi có chuyển động vĩ mô (`gross_motion`)**:
   Khi người đi lại hoặc cử động lớn làm `motion_score > 0.35`, tín hiệu biên độ lớn che lấp vi chuyển động lồng ngực. Detector tạm ngưng cập nhật nhịp thở, giữ BPM gần nhất và đặt `presence_status = "gross_motion"` để tránh tính sai nhịp thở.

5. **Chống chập chờn (Persistence Debouncing)**:
   Trạng thái `is_human_presence = true` yêu cầu vượt qua bộ lọc hình thái ít nhất 2 chu kỳ đánh giá liên tiếp (`human_confirm_streak >= 2`). Để chuyển về không có người, cần ít nhất 2–3 lần đánh giá liên tiếp bị từ chối (`reject_streak >= 2` hoặc `3`).

### Đọc file relay vòng

Mặc định daemon quét `/tmp` mỗi 50 ms để tìm file `.bin` mới nhất có tên bắt đầu bằng
`cfr_dump_phy00_`. Nếu nhận diện được tiến trình `cfr_test_app` đang mở file đó, watcher
đọc vị trí ghi từ `/proc/<pid>/fdinfo`; khi vừa gắn vào file, watcher đọc dữ liệu relay
đã có từ đầu file đến vị trí ghi hiện tại, rồi tiếp tục theo dõi các lần ghi mới. Cách
này vẫn hoạt động với file ring/preallocated khi kích thước file không tăng và tránh
bỏ qua toàn bộ dữ liệu đã được capture trước lúc daemon khởi động. Nếu không có writer
đang mở, watcher xử lý file theo kiểu append.

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
4. Frame giải mã được gửi đến `WebServer::publish`. Server cập nhật song song:
   - `MotionDetector`: tính tương quan Pearson biên độ, cập nhật cửa sổ và ngưỡng baseline.
   - `BreathDetector`: tính vi sai pha, khớp phổ hô hấp và phân loại hình thái sóng sinh học.
   - Đồng bộ trạng thái hiện diện người: `presence_detected = is_human_presence`.
5. Server lưu frame mới nhất và phát sự kiện `csi` qua SSE đến các kết nối đang mở.
6. Trình duyệt kết nối `/events`, cập nhật metric/trạng thái/biểu đồ; `/latest_csi.json`
   trả snapshot cùng các trường hiệu chuẩn và nhịp thở. Nếu producer dừng, HTTP vẫn có thể
   hoạt động nhưng frame cuối không đại diện dữ liệu realtime.

### Đầu ra và các tab

- `GET /health.json`: tình trạng server và việc đã nhận frame hay chưa.
- `GET /latest_csi.json`: frame CSI mới nhất dạng JSON, gồm:
  - Thông số vô tuyến: `timestamp_ns`, `chip_tsf_us`, `phy_ppdu_id`, `num_chains`, `nss`, `channel_bw_code`, `packet_bw_code`, `tones_per_chain`, `ap_mac`, mảng I/Q, biên độ, pha.
  - Chuyển động vĩ mô: `motion_score`, `motion_ready`, `motion_calibrated`, `motion_threshold_high`, `motion_threshold_low`, `presence_detected`.
  - Nhịp thở và phân loại: `breath_bpm`, `breath_wave`, `breath_quality`, `breath_phase_span`, `breath_asymmetry`, `breath_rrv`, `breath_valid`, `is_human_presence`, `presence_status`.
- `GET /events`: SSE stream; mỗi frame phát ra dưới dạng sự kiện `csi`.
- `POST /api/calibrate`: xóa baseline chuyển động và nhịp thở, khởi động lại quá trình hiệu chuẩn.
- Dashboard gồm 7 tab hiển thị:
  1. **Tổng quan**: motion score, ngưỡng cao/thấp, trạng thái hiện diện và nhịp thở tóm tắt.
  2. **Dạng sóng thở**: đồ thị vi chuyển động pha thời gian thực và nhãn phân loại đối tượng (người / rèm / quạt / gió).
  3. **Biên độ CSI**: đáp ứng biên độ theo từng subcarrier.
  4. **Pha CSI**: đáp ứng pha theo từng subcarrier.
  5. **Chòm sao I/Q**: phân bố điểm mẫu I/Q trên mặt phẳng phức.
  6. **So sánh antenna**: so sánh tương quan biên độ giữa các antenna chain.
  7. **Thông tin gói**: chi tiết metadata frame, MAC, PPDU ID, TSF chip.

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
