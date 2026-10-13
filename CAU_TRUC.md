# Cấu trúc chương trình — Đồng hồ M5Stack Core2 for AWS

Đồng hồ để bàn trên M5Stack Core2 for AWS: giờ (NTP + RTC), thời tiết, âm lịch, hiệu ứng nhạc theo micro,
LED trang trí, chuông báo giờ, và trang web cấu hình mở bằng điện thoại.

- Phần cứng: ESP32 (240 MHz, 8 MB PSRAM, 16 MB flash), LCD 320×240 (ILI9342C), cảm ứng FT6336U, PMIC AXP192,
  RTC BM8563, micro PDM SPM1423, loa NS4168, thanh 10 LED SK6812 ở đế M5GO Bottom2 (GPIO 25).
- Khung chạy: Arduino trên PlatformIO. Toàn bộ logic nằm trong **một file** [src/main.cpp](src/main.cpp)
  (khoảng 2700 dòng) cộng các file dữ liệu sinh tự động và một trang web nhúng.

---

## 1. Cây thư mục

```
new/
├── platformio.ini            Cấu hình build (một môi trường: m5stack-core2)
├── CAU_TRUC.md               Tài liệu này
├── datasheet.md              Datasheet sản phẩm (bạn dán vào, chỉ để tham khảo)
├── include/
│   ├── config.h              Cấu hình riêng (WiFi, vị trí, lịch ngủ mặc định) — NẰM TRONG .gitignore
│   └── config.example.h      Bản mẫu của config.h
├── src/
│   ├── main.cpp              Toàn bộ chương trình
│   ├── web_page.h            Trang web cấu hình (HTML + CSS + JS trong một chuỗi PROGMEM)
│   ├── vi_fonts.h            Font mịn có dấu tiếng Việt (VLW)          — tự sinh, tools/make_vi_fonts.py
│   ├── bg_image.h            Ảnh nền mặc định 320×240 RGB565             — tự sinh, tools/make_background.py
│   ├── music_icon.h          Biểu tượng phi hành gia 96×96               — tự sinh, tools/make_music_icon.py
│   └── chime_default.h       Âm thanh báo giờ mặc định (≤ 10 giây)      — tự sinh, tools/make_chime.py
├── tools/                    Công cụ Python sinh các file *.h ở trên (cần Pillow / miniaudio / numpy)
├── anh/                      Ảnh nguồn (nền, biểu tượng) — chỉ là nguyên liệu, không nạp lên máy
└── amthanh/                  Âm thanh nguồn (MP3) — chỉ là nguyên liệu
```

Các file `src/*.h` trừ `web_page.h` là **dữ liệu sinh ra**: không sửa tay, muốn đổi thì chạy công cụ tương ứng
trong `tools/` (xem mục 9).

---

## 2. Build và nạp

```powershell
# Build + nạp (cổng USB tự nhận; board có thể hiện COM9 hoặc COM10)
pio run -d D:\tailieu\m5stack\new -t upload
```

| Mục | Giá trị |
|---|---|
| Nền tảng | `espressif32`, board `m5stack-core2`, framework `arduino` |
| Thư viện | `M5Unified` (kéo theo `M5GFX`), `Adafruit NeoPixel`, `ArduinoJson` 7, `ChronosESP32` (kéo theo `NimBLE-Arduino`, `ESP32Time`) |
| Thư viện của Arduino core | `WiFi`, `HTTPClient`, `WebServer`, `DNSServer`, `ESPmDNS`, `LittleFS`, `Preferences` |
| Phân vùng | `default_16MB.csv`: ứng dụng 6,4 MB, NVS 20 KB, LittleFS (`spiffs`) 3,4 MB |
| Kích thước hiện tại | firmware khoảng 1,8 MB (~28% phân vùng ứng dụng) |

Lần đầu: chép `include/config.example.h` thành `include/config.h` rồi điền WiFi. Sau đó WiFi còn đổi được
qua điện thoại (mục 7), nên `config.h` chỉ là giá trị khởi đầu.

---

## 3. Cấu hình và dữ liệu lưu

### 3.1 `include/config.h` (giá trị mặc định lúc biên dịch)

| Macro | Ý nghĩa |
|---|---|
| `WIFI_SSID`, `WIFI_PASS` | WiFi 2,4 GHz khởi đầu |
| `LOCATION_NAME`, `LATITUDE`, `LONGITUDE` | Khu vực thời tiết khi chưa dò được theo IP |
| `TZ_INFO` | Múi giờ POSIX, ví dụ `<+07>-7` |
| `SLEEP_SCHEDULE_ENABLED`, `SLEEP_START_*`, `SLEEP_END_*`, `SLEEP_IDLE_SECONDS` | Lịch ngủ mặc định |

### 3.2 NVS (namespace `clock`) — cài đặt người dùng, ghi đè `config.h`

| Khoá | Nội dung |
|---|---|
| `wSsid`, `wPass` | WiFi đang dùng |
| `slEn`, `slSH`, `slSM`, `slEH`, `slEM`, `slIdle` | Lịch ngủ: bật, giờ/phút bắt đầu, giờ/phút kết thúc, số giây tự tắt lại |
| `col0` … `col6` | Màu chữ RGB888 của 7 mục (giờ, dấu hai chấm, ngày, nhiệt độ, mô tả, khu vực, độ ẩm/gió) |
| `tmStyle` | Kiểu số đồng hồ (0–5) |
| `muStyle` | Kiểu sóng nhạc (0–6) |
| `dateMode` | Ngày: 0 dương, 1 âm, 2 tự động luân phiên 10 giây |
| `locAuto`, `locName`, `locLat`, `locLon` | Khu vực: tự dò theo IP hay nhập tay, tên và toạ độ |
| `chOn`, `chVol`, `chQuiet` | Chuông báo giờ: bật, âm lượng 0–100, im lặng khi đang ngủ |

### 3.3 LittleFS

| File | Nội dung | Kích thước |
|---|---|---|
| `/bg.raw` | Ảnh nền tuỳ chỉnh, RGB565 320×240 | đúng 153 600 byte |
| `/chime.raw` | Âm thanh báo giờ tuỳ chỉnh, int16 mono 16 kHz | tối đa 320 000 byte (10 giây) |

Không có file thì dùng bản nhúng trong firmware (`bg_image.h`, `chime_default.h`).

---

## 4. Kiến trúc `main.cpp`

### 4.1 Các luồng (task)

| Luồng | Việc | Ghi chú |
|---|---|---|
| `loop()` (core 1, Arduino) | Mọi thứ còn lại: cảm ứng, vẽ, mạng, web | Lặp mỗi 20 ms (1 ms ở màn hình nhạc) |
| `ledTask` (core 1, ưu tiên 1) | Hiệu ứng thanh LED ở 30 ms/khung | Đọc biến `volatile`: `ledMode`, `sleeping`, `inMusic`, `musicLevel`, `musicBeat` |

`ledTask` chạy riêng để LED vẫn mượt khi `loop()` đang chờ HTTP (tải thời tiết, dò vị trí).

### 4.2 Các phần của file (theo dòng chú thích `// ---------- ... ----------`)

| Phần | Việc | Hàm tiêu biểu |
|---|---|---|
| Đầu file | Include, ảnh nền, hằng số, trạng thái chung | `drawBg()` |
| Thời gian | Giờ từ RTC, đồng bộ NTP | `systemTimeFromRtc`, `onNtpSync`, `timeValid` |
| Thời tiết | Dò khu vực theo IP, tải Open-Meteo | `lookupLocation`, `fetchWeather`, `describe` |
| Vẽ — icon thời tiết động | Icon vẽ vào sprite, ~15 khung/giây | `drawWeatherIcon`, `drawIconFrame` |
| Vẽ — khối thời tiết | Hàng nhiệt độ/độ ẩm/gió, mô tả chạy chữ, khu vực | `drawStatsRow`, `drawDescFrame`, `drawLocation`, `drawWeather` |
| Âm lịch | Thuật toán Hồ Ngọc Đức, múi giờ UTC+7 | `solarToLunar` |
| Vẽ — thanh tiêu đề, đồng hồ | Ngày, icon WiFi, icon pin; số giờ | `drawHeader`, `drawClock`, `applyClockFont` |
| LED trang trí | 4 hiệu ứng + chế độ theo nhạc | `ledTask`, `setLedMode` |
| Chế độ ngủ | Tắt màn hình + LED, lịch tự động | `enterSleep`, `wakeUp`, `inSleepWindow` |
| Cài đặt giờ ngủ (nút A) | Màn hình sửa lịch ngủ | `drawSettings`, `adjustField`, `enterSettings`, `exitSettings` |
| Ảnh nền + web | WiFi, điểm phát cài đặt, chuông, các `handle*` | `startAp`, `loadCustomBg`, `playChime`, `handleState` |
| Màn hình nhạc | Micro → FFT → 7 kiểu hình | `musicFrame`, `fft`, `render*` |
| Web server | Đăng ký route, chụp màn hình | `startWeb`, `handleScreenshot` |
| Chương trình chính | `setup()` và `loop()` | |

### 4.3 `setup()`

1. `M5.begin()`, mở Serial 115200.
2. Nạp NVS (`loadSchedule` đọc hầu hết khoá), `loadWifi`, ảnh nền tuỳ chỉnh (`loadCustomBg`), âm thanh tuỳ chỉnh.
3. Khởi tạo LED, tạo `ledTask`.
4. Vẽ ảnh nền, đặt múi giờ, nạp giờ từ RTC, bắt đầu NTP.
5. Kết nối WiFi (nếu có cấu hình), vẽ khối thời tiết.

### 4.4 `loop()` — thứ tự mỗi vòng

1. **WiFi**: áp dụng WiFi mới nếu vừa lưu; bật điểm phát cài đặt khi mất mạng; khi vào được mạng thì khởi động web server.
2. `server.handleClient()`.
3. Vẽ lại toàn bộ nếu ảnh nền/màu vừa đổi (`bgChanged`); ghi RTC khi NTP vừa đồng bộ.
4. **Đầu vào**: nút A/B/C, cảm ứng, cử chỉ vuốt, chạm vào số giờ hoặc dòng ngày.
5. **Máy trạng thái** (xem 4.5) xử lý đầu vào theo màn hình đang hiện.
6. `musicFrame()` nếu đang ở màn hình nhạc.
7. Dò khu vực theo IP (6 giờ/lần), tải thời tiết (10 phút/lần).
8. Cập nhật icon thời tiết động, vẽ màn hình cài đặt nếu cần.
9. Mỗi giây: vẽ thanh tiêu đề + đồng hồ, kiểm tra chuông báo giờ, kiểm tra lịch ngủ.

### 4.5 Các trạng thái màn hình

Một cờ cho mỗi trạng thái, xét theo thứ tự ưu tiên trong chuỗi `if / else if` của `loop()`:

| Trạng thái | Cờ | Vào | Ra |
|---|---|---|---|
| Ngủ | `sleeping` | Giữ nút B; lịch ngủ; web `/api/sleepnow` | Chạm màn hình hoặc bấm nút bất kỳ |
| Cài đặt giờ ngủ | `inSettings` | Bấm nhanh nút A | Nút A ở mục cuối (lưu), giữ nút A (huỷ), 60 giây không chạm |
| Màn hình nhạc | `inMusic` | Vuốt phải → trái; web `/api/music` | Vuốt trái → phải |
| Mã QR cài WiFi | `apActive` | Tự động khi không vào được WiFi | Tự động khi vào được mạng |
| Màn hình chính | (mặc định) | | |

`overlayActive()` = `inSettings || inMusic`; các hàm vẽ màn hình chính kiểm tra nó để khỏi vẽ đè.

### 4.6 Bố cục màn hình chính (320×240)

| Vùng | y | Nội dung |
|---|---|---|
| Thanh tiêu đề | 0–26 | Ngày (dương/âm), icon WiFi, icon pin; thông báo tạm che icon |
| Khối thời tiết | 26–126 | Icon động bên trái; hàng 1: nhiệt độ \| độ ẩm \| gió; hàng 2: mô tả \| khu vực |
| Đồng hồ | 142–238 | Giờ, phút, dấu hai chấm nhấp nháy |

Mọi thứ vẽ trên **ảnh nền**: không có màu nền đặc. Muốn xoá vùng thì gọi `drawBg(...)` để chép lại đúng phần ảnh;
sprite thì khởi tạo bằng `drawBg` thay vì `fillSprite`.

---

## 5. Điều khiển trên đồng hồ

| Thao tác | Tác dụng |
|---|---|
| Nút A, bấm nhanh | Mở cài đặt giờ ngủ |
| Nút A, giữ | Cập nhật thời tiết ngay |
| Nút B, bấm nhanh | Đổi hiệu ứng LED (ở màn hình nhạc: đổi kiểu sóng) |
| Nút B, giữ | Vào chế độ ngủ |
| Nút C | Đổi độ sáng màn hình (255 → 200 → 160) |
| Chạm vào số giờ | Đổi kiểu số (6 kiểu) |
| Chạm vào dòng ngày | Đổi dương lịch / âm lịch / tự động |
| Vuốt phải → trái | Vào màn hình nhạc |
| Vuốt trái → phải | Về đồng hồ |

Trong màn hình cài đặt: A = mục kế (mục cuối = Lưu), B = giảm, C = tăng, giữ A = huỷ.
Lần chạm/bấm đầu tiên lúc đang ngủ chỉ để đánh thức, không kích hoạt chức năng.

---

## 6. Màn hình nhạc

Micro PDM → `M5.Mic.record` (256 mẫu, 16 kHz) → cửa sổ Hann → FFT 256 điểm → 16 dải tần log (125 Hz–7 kHz).
Mỗi dải trừ **tiếng ồn nền** (theo dõi mức thấp nhất, nhích lên rất chậm) rồi chuẩn hoá theo **AGC** (đỉnh gần đây).
Từ đó có `barLevel[16]` (0–1), `beatLevel` (nhịp bass) và `musicLevel` cho LED.

Bảy kiểu (enum `MusicStyle`): Cột, Quả cầu, Đối xứng, Vòng tròn, Thác phổ, Phi hành gia, Tim bay.
Kiểu *Cột* vẽ trực tiếp phần thay đổi; các kiểu còn lại dựng khung vào sprite `musicCanvas` (PSRAM) rồi đẩy lên màn hình.

> Loa và micro dùng chung I2S trên Core2, nên vào màn hình nhạc thì tắt loa (`Speaker.end`) và chuông báo giờ
> bị bỏ qua cho tới khi thoát.

---

## 6b. Màn hình chỉ đường (Google Maps qua app Chronos)

Điện thoại chạy app **Chronos** (fbiego), kết nối BLE tới thiết bị **M5Clock**, bật dẫn đường trên Google Maps.
Đồng hồ dùng thư viện `fbiego/ChronosESP32` (NimBLE) để nhận: icon rẽ 48×48 1-bit (phóng 3×), khoảng cách tới điểm
rẽ (`title`), tên đường/hướng dẫn, thời gian còn lại, quãng đường, giờ đến.

- **Tự vào / thoát**: có dữ liệu dẫn đường (`active && isNavigation`) thì tự vào màn hình (kể cả đang ngủ theo lịch),
  hết dẫn đường hoặc mất kết nối BLE thì tự về đồng hồ. Vuốt ngang để thoát tay (không tự vào lại cho tới khi
  hết chuyến). Từ màn hình nhạc vuốt trái để vào thủ công.
- **Luồng**: thư viện phân tích gói ngay trong task BLE, nên callback chép `Navigation` sang `navSnap` (mutex) rồi
  `loop()` mới đọc bản sao `navLocal` (`navPoll()`), tránh đọc `String` đang bị ghi.
- Mã: khối `Chi duong Google Maps` trong `main.cpp` (`drawNavScreen`, `navFrame`, `enterNav/exitNav`).
- Thư viện có `struct Weather` toàn cục nên struct thời tiết của dự án đổi tên `WeatherNow`.

> **IRAM**: WiFi + BLE làm tràn IRAM của Arduino-ESP32 2.0.x. `tools/slim_iram.py` (extra_scripts) tạo bản sao
> `sections.ld` bỏ nhóm hàm thời gian của libc (strftime, strptime, mktime, tzset…, ~10 KB) khỏi IRAM; chúng chạy
> từ flash. Nếu sau này lại tràn IRAM, xem `.pio/build/.../firmware.map` mục `.iram0.text`.

---

## 7. Mạng, trang web và API

### 7.1 Đường kết nối

- Bình thường: STA vào WiFi nhà, web ở `http://<IP>/` hoặc `http://m5clock.local/` (mDNS).
- Mất mạng (30 giây sau khi bật máy, hoặc 3 phút khi đang chạy): đồng hồ phát AP **`M5Clock-Setup`** (mật khẩu
  `12345678`, đổi ở `AP_PASS`) kèm DNS bắt mọi tên miền (captive portal). Màn hình hiện hai mã QR: vào WiFi,
  và mở `http://192.168.4.1`. Trong lúc phát AP vẫn thử lại WiFi cũ mỗi phút.

### 7.2 API HTTP (cổng 80, không có xác thực)

| Phương thức | Đường dẫn | Tham số | Việc |
|---|---|---|---|
| GET | `/` | | Trang cấu hình (`web_page.h`) |
| GET | `/api/state` | | Toàn bộ trạng thái: lịch ngủ, màu, WiFi, nhạc, khu vực, ngày, chuông |
| GET | `/screenshot.bmp` | `preview=ap` (tuỳ chọn) | Chụp màn hình LCD; `preview=ap` xem trước màn hình mã QR |
| GET | `/api/lunar` | `y`, `m`, `d` | Đổi một ngày dương sang âm (để kiểm tra thuật toán) |
| GET | `/api/wifi/scan` | | Quét WiFi xung quanh |
| POST | `/api/wifi` | `ssid`, `pass` | Lưu WiFi và kết nối lại |
| POST | `/api/sleep` | `enabled`, `start`, `end`, `idle` | Lịch ngủ |
| POST | `/api/sleepnow`, `/api/wake` | | Ngủ ngay / đánh thức |
| POST | `/api/colors` | `time`, `colon`, `date`, `temp`, `desc`, `label`, `value` | Đổi màu (`#rrggbb`), mục nào có thì đổi |
| POST | `/api/colors/reset` | | Màu mặc định |
| POST | `/api/bg` | multipart `bg` (153 600 byte RGB565) | Ảnh nền mới |
| POST | `/api/bg/reset` | | Ảnh nền mặc định |
| POST | `/api/location` | `auto=1`, hoặc `auto=0&name&lat&lon` | Khu vực thời tiết |
| POST | `/api/datemode` | `mode` = 0/1/2 | Dương / âm / tự động |
| POST | `/api/music` | `on`, `style` | Bật/tắt màn hình nhạc, chọn kiểu |
| POST | `/api/chime` | `on`, `vol`, `quiet` | Cấu hình chuông |
| POST | `/api/chime/sound` | multipart `snd` (int16 mono 16 kHz, ≤ 10 giây) | Âm thanh mới |
| POST | `/api/chime/sound/reset` | | Âm thanh mặc định |
| POST | `/api/chime/test` | | Nghe thử (từ chối ở màn hình nhạc) |

Ảnh và âm thanh được **trình duyệt chuyển đổi** (cắt ảnh về 320×240 RGB565, giải mã âm thanh về 16 kHz mono)
rồi mới gửi, nên đồng hồ không phải giải mã JPEG/MP3.

### 7.3 Dịch vụ bên ngoài (đều qua HTTP thường, không cần khoá API)

| Dịch vụ | Dùng để |
|---|---|
| `api.open-meteo.com` | Thời tiết hiện tại và cao/thấp trong ngày |
| `ipwho.is` | Dò toạ độ và thành phố theo IP |
| `geocoding-api.open-meteo.com` | Đổi tên thành phố sang tiếng Việt |
| `pool.ntp.org`, `time.google.com` | Đồng bộ giờ |

---

## 8. Bộ nhớ và lưu ý khi sửa code

- **PSRAM**: ảnh nền tuỳ chỉnh (150 KB), âm thanh tuỳ chỉnh (tối đa 320 KB), sprite màn hình nhạc (~127 KB),
  sprite icon/mô tả/cài đặt. Nếu thiếu PSRAM, `ps_malloc` rơi về `malloc`.
- **Cuộc gọi HTTP chặn `loop()`** vài giây (thời tiết, dò vị trí): đừng đặt việc cần mượt (LED, nhạc) trong
  `loop()` mà không có luồng riêng. LED đã được tách vào `ledTask`.
- **Biến chia sẻ giữa luồng** phải `volatile` (xem 4.1).
- **Tải ảnh/âm thanh lên** ghi vào bộ đệm riêng rồi mới hoán đổi, để dữ liệu đang dùng không hỏng nếu việc gửi bị đứt.
- **Chữ tiếng Việt**: dùng font VLW (`vi_fonts.h`) qua `loadFont(VI_R14/VI_R18/VI_B18)`; các font có sẵn của
  thư viện không có dấu. Nhớ `unloadFont()` sau khi vẽ.
- **Font số đồng hồ** `applyClockFont` đặt `setTextSize` khác 1 — luôn trả về `setTextSize(1)` sau khi vẽ.
- **Trang web nằm trong flash** (`web_page.h`, khoảng 27 KB) và được trả bằng `server.send_P`, không tốn RAM.
  Sửa trang xong phải build + nạp lại firmware mới thấy.

---

## 9. Công cụ trong `tools/`

| Lệnh | Sinh ra | Ghi chú |
|---|---|---|
| `python tools/make_vi_fonts.py` | `src/vi_fonts.h` | Segoe UI của Windows, 3 cỡ (R18, R14, B18) |
| `python tools/make_background.py [--src ảnh] [--crop-x 0..1] [--dim] [--dim-bottom]` | `src/bg_image.h` | Cắt 4:3, thu 320×240, làm tối |
| `python tools/make_music_icon.py [--src png]` | `src/music_icon.h` | Cần PNG nền trong suốt thật; mặc định tự vẽ |
| `python tools/make_chime.py [--src file] [--max-seconds N]` | `src/chime_default.h` | Mono 16 kHz, cắt ≤ 10 giây, bỏ đoạn lặng |

Chú ý: `make_chime.py` và `make_background.py` khi không có `--src` sẽ lấy **file đầu tiên theo thứ tự tên**
trong `amthanh/` hoặc `anh/`. Thư mục `amthanh/` hiện có nhiều file, nên hãy chỉ định `--src` rõ ràng (âm thanh
mặc định hiện tại là `ivan_luzan-happy-logo-167474.mp3`).

---

## 10. Cách thêm một thứ mới

- **Một tuỳ chọn cấu hình có lưu**: khai báo biến toàn cục → đọc trong `loadSchedule()` → hàm `save…()` →
  trường trong `handleState()` → route `server.on(...)` → phần HTML/JS trong `web_page.h`.
- **Một kiểu sóng nhạc**: thêm giá trị vào `enum MusicStyle` và tên vào `MUSIC_STYLE_NAMES`, tăng
  `MUSIC_STYLE_COUNT`, viết `renderXxx(M5Canvas &g, ...)`, thêm `case` trong `musicFrame()`, thêm nút trong
  `web_page.h` (`data-s`).
- **Một hiệu ứng LED**: thêm vào `enum LedMode` và `LED_MODE_NAMES`, thêm `case` trong `ledTask()`.
- **Một kiểu số đồng hồ**: thêm `case` trong `applyClockFont()` và tên trong `CLOCK_STYLE_NAMES`, tăng `CLOCK_STYLES`.
