// Dong ho + thoi tiet cho M5Stack Core2 for AWS.
//   Gio: dong bo NTP qua WiFi, luu vao RTC (BM8563) de van chay dung khi mat mang.
//   Thoi tiet: Open-Meteo, cap nhat 10 phut/lan.
//   Nut A: bam nhanh = cai dat gio ngu tren man hinh; giu = cap nhat thoi tiet ngay.
//   Nut B: bam nhanh doi che do LED trang tri (tat / cau vong / tho / duoi),
//          giu ~0,5 giay de vao che do ngu (tat het man hinh va LED).
//   Nut C: doi do sang man hinh.  Cham vao so dong ho: doi kieu so (6 kieu, duoc luu).
//   Vuot phai -> trai: man hinh nhay theo nhac (micro + FFT). Vuot trai -> phai: ve dong ho.
//   Che do ngu: cham man hinh hoac bam nut bat ky de danh thuc.
//   Lich ngu tu dong: dat bang nut A, luu trong NVS (gia tri mac dinh: SLEEP_* trong
//   include/config.h). Ngoai gio ngu, danh thuc bang tay thi man hinh tu tat lai sau
//   mot so giay khong thao tac.
//   Trang web cau hinh (cung mang WiFi): http://<IP dong ho>/ hoac http://m5clock.local/
//   - chinh thong so ngu, ngu ngay / danh thuc, va gui anh nen moi tu dien thoai.
//   IP hien o goc phai thanh tieu de khi vua ket noi WiFi va trong man hinh cai dat (nut A).
#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <esp_sntp.h>
#include <Preferences.h>
#include "config.h"
#include "vi_fonts.h"   // font VLW co chu tieng Viet co dau (tao boi tools/make_vi_fonts.py)
#include "bg_image.h"   // anh nen mac dinh 320x240 RGB565 (tao boi tools/make_background.py)
#include "web_page.h"   // trang web cau hinh
#include "music_icon.h" // bieu tuong 96x96 cho kieu song nhac "Phi hanh gia" (tools/make_music_icon.py)
#include "chime_default.h"   // am thanh bao gio mac dinh, <= 10 giay (tools/make_chime.py)

// Anh nen dang dung: mac dinh la anh nhung trong firmware; anh gui tu dien thoai duoc luu
// o LittleFS (/bg.raw) va nap vao RAM (PSRAM) luc khoi dong.
static const size_t BG_BYTES = (size_t)BG_W * BG_H * 2;
static const char *BG_FILE = "/bg.raw";
static const uint16_t *bgData = BG_IMAGE;

// Ve mot hinh chu nhat cua anh nen (nguon sx,sy,w,h) len `g` tai (dx,dy).
// Dung de xoa vung ve ve anh nen, va de khoi tao sprite voi nen trong suot.
static void drawBg(lgfx::LGFXBase &g, int dx, int dy, int sx, int sy, int w, int h) {
  if (sx < 0) { w += sx; dx -= sx; sx = 0; }
  if (sy < 0) { h += sy; dy -= sy; sy = 0; }
  if (sx + w > BG_W) w = BG_W - sx;
  if (sy + h > BG_H) h = BG_H - sy;
  if (w <= 0 || h <= 0) return;

  g.startWrite();
  if (w == BG_W) {   // ca chieu ngang: du lieu lien tuc, day mot lan
    g.pushImage(dx, dy, w, h, (const lgfx::rgb565_t *)&bgData[sy * BG_W]);
  } else {
    for (int r = 0; r < h; r++) {
      g.pushImage(dx, dy + r, w, 1, (const lgfx::rgb565_t *)&bgData[(sy + r) * BG_W + sx]);
    }
  }
  g.endWrite();
}

static const uint32_t WEATHER_INTERVAL_MS = 10UL * 60UL * 1000UL;
static const uint32_t WEATHER_RETRY_MS = 60UL * 1000UL;

// Thanh LED SK6812 o de M5GO Bottom2: 10 LED noi vao GPIO25
static const uint8_t LED_COUNT = 10;
static Adafruit_NeoPixel leds(LED_COUNT, 25, NEO_GRB + NEO_KHZ800);

enum LedMode : uint8_t { LED_OFF, LED_RAINBOW, LED_BREATH, LED_CHASE, LED_MODE_COUNT };
static const char *LED_MODE_NAMES[LED_MODE_COUNT] = {"LED: Off", "LED: Rainbow", "LED: Breath",
                                                      "LED: Chase"};
static const uint8_t LED_BRIGHTNESS = 60;   // do sang toi da cua LED (0-255)
static volatile uint8_t ledMode = LED_RAINBOW;

// Man hinh nhac (vuot phai -> trai): task LED doc cac gia tri nay de nhay theo nhac
static volatile bool inMusic = false;
static volatile uint8_t musicLevel = 0;   // do lon tong the 0-255
static volatile uint8_t musicBeat = 0;    // 255 khi vua co nhip bass, giam dan
static const uint8_t MUSIC_STYLE_COUNT = 7;
static uint8_t musicStyle = 0;            // kieu song nhac dang chon (xem enum MusicStyle), luu NVS

// Lich ngu mac dinh (co the ghi de trong include/config.h)
#ifndef SLEEP_SCHEDULE_ENABLED
#define SLEEP_SCHEDULE_ENABLED 0
#endif
#ifndef SLEEP_START_HOUR
#define SLEEP_START_HOUR 23
#define SLEEP_START_MIN 0
#define SLEEP_END_HOUR 6
#define SLEEP_END_MIN 0
#endif
#ifndef SLEEP_IDLE_SECONDS
#define SLEEP_IDLE_SECONDS 30
#endif

// Chuong bao gio moi: keu 3 not ngan moi khi sang gio (hh:00). Cau hinh tren trang web, luu NVS.
static bool chimeOn = true;          // bat / tat tieng chuong
static uint8_t chimeVol = 50;        // am luong 0..100
static bool chimeQuiet = true;       // im lang khi man hinh dang ngu (ban dem)
static int lastChimeHour = -1;       // gio da thay o lan kiem tra truoc, -1 = chua co
static uint32_t chimeCount = 0;      // so lan da keu tu luc bat may (de kiem tra tu xa)
static char chimeLastAt[8] = "";     // gio cua lan keu gan nhat, vi du "21:00"

// Am thanh bao gio: mac dinh nhung trong firmware; am thanh tai len tu trang web (mono, 16 kHz,
// 16 bit, toi da 10 giay) luu o LittleFS (/chime.raw) va nap vao RAM (PSRAM) luc khoi dong.
static const uint32_t CHIME_RATE = 16000;
static const uint32_t CHIME_MAX_SAMPLES = CHIME_RATE * 10;
static const char *CHIME_FILE = "/chime.raw";
static const int16_t *chimeData = CHIME_DEFAULT;
static uint32_t chimeLen = CHIME_DEFAULT_LEN;
static int16_t *chimeCustom = nullptr;      // am thanh tuy chinh dang dung (nullptr = mac dinh)
static int16_t *chimeUploadBuf = nullptr;   // bo dem nhan am thanh moi
static uint32_t chimeUploadLen = 0;         // byte da nhan
static bool chimeUploadOk = false;

static volatile bool sleeping = false;   // man hinh va LED dang tat
static bool manualSleep = false;         // ngu do nguoi dung giu nut B (khong theo lich)
static bool ignoreA = false;             // bo qua nut A / B cho toi khi tha ra (sau khi dung no
static bool ignoreB = false;             // de danh thuc, vao ngu hoac huy cai dat)
static uint32_t lastActivityMs = 0;

// Lich ngu dang dung: mac dinh lay tu config.h, duoc ghi de bang cai dat tren man hinh (luu NVS)
struct SleepSchedule {
  bool enabled;
  uint8_t startH, startM, endH, endM;
  uint16_t idleSec;   // trong gio ngu: tu tat lai sau bay nhieu giay khong thao tac
};
static SleepSchedule sched = {SLEEP_SCHEDULE_ENABLED != 0, SLEEP_START_HOUR, SLEEP_START_MIN,
                              SLEEP_END_HOUR, SLEEP_END_MIN, SLEEP_IDLE_SECONDS};

// Thong bao ngan o goc phai thanh tieu de khi doi che do
static const char *toastText = "";
static uint32_t toastUntil = 0;

// Bo cuc man hinh 320x240
static const int HEADER_H = 26;
static const int WEATHER_Y = 30;    // khoi thoi tiet o tren: y = 26..126 (WEATHER_H cao)
static const int WEATHER_H = 100;
static const int TIME_CY = 190;     // dong ho o duoi: so cao toi da 96 -> y = 142..238

static const uint16_t COL_BG = TFT_BLACK;
static const uint16_t COL_HEADER = 0x18E3;
static const uint16_t COL_DIM = 0x8410;
static const uint16_t COL_TIME = TFT_WHITE;
static const uint16_t COL_ACCENT = 0x05FF;

// Mau chu rieng cho tung noi dung tren man hinh chinh. Chinh tren trang web, luu NVS (RGB888).
enum ColorItem : uint8_t { C_TIME, C_COLON, C_DATE, C_TEMP, C_DESC, C_LABEL, C_VALUE, C_COUNT };
static const char *COLOR_KEYS[C_COUNT] = {"time", "colon", "date", "temp", "desc", "label", "value"};
static const uint32_t COLOR_DEFAULTS[C_COUNT] = {
    0xFFFFFF,   // time : so gio va phut
    0x00BFFF,   // colon: dau hai cham va thanh giay
    0xFFFFFF,   // date : thu, ngay thang
    0xFFFFFF,   // temp : nhiet do
    0x00BFFF,   // desc : mo ta thoi tiet
    0x808080,   // label: nhan "Do am", "Gio"
    0xFFFFFF,   // value: gia tri do am, gio
};
static uint32_t colorRgb[C_COUNT] = {0xFFFFFF, 0x00BFFF, 0xFFFFFF, 0xFFFFFF, 0x00BFFF, 0x808080, 0xFFFFFF};

// RGB888 -> RGB565 cho man hinh
static uint16_t col(ColorItem i) {
  uint32_t c = colorRgb[i];
  return ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F);
}
static const uint16_t COL_CLOUD = 0xC618;
static const uint16_t COL_SUN = 0xFE60;

static const uint8_t BRIGHTNESS_LEVELS[] = {255, 200, 160};
static uint8_t brightnessIdx = 0;

enum WeatherKind { WX_CLEAR, WX_PARTLY, WX_CLOUDY, WX_FOG, WX_RAIN, WX_SNOW, WX_STORM };

struct Weather {
  bool valid = false;
  float temp = 0, feels = 0, wind = 0, tMax = 0, tMin = 0;
  int humidity = 0, code = 0;
  bool isDay = true;
  char updatedAt[6] = "";
};

static Weather weather;

// Khu vuc xem thoi tiet: tu dong theo dia chi IP cua mang (mac dinh) hoac nhap tay tren trang web.
// Gia tri khoi dau lay tu config.h; ket qua tra cuu / nhap tay duoc luu NVS.
static bool locAuto = true;
static String locName = LOCATION_NAME;
static float locLat = LATITUDE, locLon = LONGITUDE;
static uint32_t nextLocAt = 0;                         // lan tra cuu IP ke tiep (millis)
static const uint32_t LOC_INTERVAL_MS = 6UL * 3600UL * 1000UL;   // dem may sang noi khac: 6 gio la kip
static const uint32_t LOC_RETRY_MS = 5UL * 60UL * 1000UL;

static uint32_t nextWeatherAt = 0;
static bool weatherEverTried = false;
static volatile bool ntpSynced = false;
static time_t lastSecond = 0;
static int lastMinute = -1;
static String lastHeader;
static uint8_t clockStyle = 0;   // kieu so dong ho (cham vao dong ho de doi, luu trong NVS)

// WiFi dang dung: luu trong NVS (dat qua trang web); lan dau lay tu include/config.h
static String wifiSsid, wifiPass;
static bool wifiConfigured() { return wifiSsid.length() > 0; }

// Khi khong vao duoc WiFi, dong ho tu phat mang nay de dien thoai ket noi vao va cau hinh
static const char *AP_SSID = "M5Clock-Setup";
static const char *AP_PASS = "12345678";
static const uint32_t AP_AFTER_BOOT_MS = 30000;    // vua bat may ma 30 giay chua vao duoc mang
static const uint32_t AP_AFTER_LOSS_MS = 180000;   // dang chay ma mat mang 3 phut
static const uint32_t STA_RETRY_MS = 60000;        // khi dang phat AP: thu lai WiFi cu moi phut
static bool apActive = false;

// ---------- Thoi gian ----------

// Ngay gio UTC -> epoch (thuat toan days_from_civil)
static time_t utcToEpoch(int y, int m, int d, int hh, int mm, int ss) {
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = era * 146097L + (long)doe - 719468L;
  return (time_t)days * 86400 + hh * 3600 + mm * 60 + ss;
}

// RTC luu gio UTC; nap vao dong ho he thong luc khoi dong
static void systemTimeFromRtc() {
  auto dt = M5.Rtc.getDateTime();
  if (dt.date.year < 2024) return;
  timeval tv = {utcToEpoch(dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes,
                           dt.time.seconds),
                0};
  settimeofday(&tv, nullptr);
}

static void onNtpSync(struct timeval *) { ntpSynced = true; }

static bool timeValid(const struct tm &t) { return t.tm_year + 1900 >= 2024; }

// ---------- Thoi tiet ----------

static WeatherKind kindOf(int code) {
  if (code == 0 || code == 1) return WX_CLEAR;
  if (code == 2) return WX_PARTLY;
  if (code == 3) return WX_CLOUDY;
  if (code == 45 || code == 48) return WX_FOG;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return WX_SNOW;
  if (code >= 95) return WX_STORM;
  return WX_RAIN;
}

// Ma thoi tiet WMO -> mo ta tieng Viet (UTF-8, ve bang font VI_R18)
static const char *describe(int code) {
  switch (code) {
    case 0: return "Trời quang";
    case 1: return "Ít mây";
    case 2: return "Mây rải rác";
    case 3: return "Nhiều mây";
    case 45: case 48: return "Sương mù";
    case 51: case 53: case 55: return "Mưa phùn";
    case 56: case 57: case 66: case 67: return "Mưa băng";
    case 61: return "Mưa nhỏ";
    case 63: return "Mưa vừa";
    case 65: return "Mưa to";
    case 71: case 73: case 75: case 77: case 85: case 86: return "Tuyết";
    case 80: case 81: return "Mưa rào";
    case 82: return "Mưa rào lớn";
    case 95: return "Dông";
    case 96: case 99: return "Dông, mưa đá";
    default: return "Không rõ";
  }
}

// "Thành phố Hồ Chí Minh" -> "TP. Hồ Chí Minh"; "Tỉnh X" -> "X"
static String shortenPlace(String n) {
  static const char TP[] = "Thành phố ";
  static const char TINH[] = "Tỉnh ";
  if (n.startsWith(TP)) n = String("TP. ") + n.substring(strlen(TP));
  else if (n.startsWith(TINH)) n = n.substring(strlen(TINH));
  return n;
}

static String urlEncode(const String &s) {
  static const char HEX_DIGITS[] = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += HEX_DIGITS[c >> 4];
      out += HEX_DIGITS[c & 15];
    }
  }
  return out;
}

// Doc mot URL HTTP va phan tich JSON (gioi han thoi gian ngan de khong treo man hinh lau)
static bool httpGetJson(const String &url, JsonDocument &doc) {
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(5000);
  if (!http.begin(url)) return false;
  int status = http.GET();
  bool ok = status == HTTP_CODE_OK && !deserializeJson(doc, http.getString());
  if (status != HTTP_CODE_OK) Serial.printf("HTTP %d: %s\n", status, url.c_str());
  http.end();
  return ok;
}

static void saveLocation() {
  Preferences prefs;
  if (!prefs.begin("clock", false)) return;
  prefs.putBool("locAuto", locAuto);
  prefs.putString("locName", locName);
  prefs.putFloat("locLat", locLat);
  prefs.putFloat("locLon", locLon);
  prefs.end();
}

// Tra cuu khu vuc tu IP mang. Buoc 1: toa do + ten thanh pho (tieng Anh). Buoc 2: doi sang ten
// tieng Viet qua dich vu tim dia danh cua Open-Meteo (neu khong duoc thi giu ten tieng Anh).
// Tra ve true khi co toa do moi; *changed cho biet co khac gia tri dang dung khong.
static bool lookupLocation(bool *changed) {
  JsonDocument ip;
  if (!httpGetJson("http://ipwho.is/?fields=success,city,region,latitude,longitude", ip)) return false;
  if (!ip["success"].as<bool>()) return false;
  float lat = ip["latitude"].as<float>(), lon = ip["longitude"].as<float>();
  String city = ip["city"].as<String>();
  if (city.isEmpty()) city = ip["region"].as<String>();
  if (city.isEmpty() || (lat == 0 && lon == 0)) return false;

  String name = city;
  JsonDocument geo;
  if (httpGetJson("http://geocoding-api.open-meteo.com/v1/search?count=1&language=vi&name=" + urlEncode(city),
                  geo)) {
    JsonObject r = geo["results"][0];
    // Chi nhan ten tieng Viet neu ket qua nam gan toa do vua do (tranh trung ten voi noi khac)
    if (!r.isNull() && fabsf(r["latitude"].as<float>() - lat) < 1.0f &&
        fabsf(r["longitude"].as<float>() - lon) < 1.0f && r["name"].as<String>().length() > 0) {
      name = r["name"].as<String>();
    }
  }
  name = shortenPlace(name);
  if (name.length() > 40) name = name.substring(0, 40);

  *changed = name != locName || fabsf(lat - locLat) > 0.005f || fabsf(lon - locLon) > 0.005f;
  locName = name;
  locLat = lat;
  locLon = lon;
  return true;
}

static bool fetchWeather() {
  char url[320];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m,is_day"
           "&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1",
           (double)locLat, (double)locLon);

  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(5000);
  if (!http.begin(url)) return false;

  int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("Thoi tiet: HTTP %d\n", status);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err || !doc["current"]["temperature_2m"].is<float>()) {
    Serial.printf("Thoi tiet: JSON loi (%s)\n", err.c_str());
    return false;
  }

  JsonObject cur = doc["current"];
  weather.temp = cur["temperature_2m"];
  weather.feels = cur["apparent_temperature"];
  weather.humidity = cur["relative_humidity_2m"];
  weather.wind = cur["wind_speed_10m"];
  weather.code = cur["weather_code"];
  weather.isDay = cur["is_day"].as<int>() != 0;
  weather.tMax = doc["daily"]["temperature_2m_max"][0];
  weather.tMin = doc["daily"]["temperature_2m_min"][0];
  weather.valid = true;

  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  snprintf(weather.updatedAt, sizeof(weather.updatedAt), "%02d:%02d", t.tm_hour, t.tm_min);

  Serial.printf("Thoi tiet: %.1fC, %d%%, ma %d\n", weather.temp, weather.humidity, weather.code);
  return true;
}

// ---------- Ve ----------

// ---------- Icon thoi tiet dong ----------
// Ve vao sprite roi day len man hinh ~15 khung/giay nen khong bi nhap nhay.
static const int ICON_SIZE = 84;
static const int ICON_X = 2, ICON_Y = WEATHER_Y - 2;
static M5Canvas iconCanvas;
static bool iconReady = false;

static void drawCloud(M5Canvas &g, int cx, int cy, uint16_t color) {
  g.fillCircle(cx - 13, cy + 3, 11, color);
  g.fillCircle(cx + 1, cy - 6, 15, color);
  g.fillCircle(cx + 16, cy + 4, 10, color);
  g.fillRoundRect(cx - 24, cy + 2, 50, 13, 6, color);
}

// Mat troi: tia quay cham, do dai tia nhip nhang, loi dap nhe
static void drawSun(M5Canvas &g, int cx, int cy, int r, uint32_t t) {
  g.fillCircle(cx, cy, r + (int)lroundf(sinf(t * 0.004f)), COL_SUN);
  float spin = t * 0.0007f;
  for (int i = 0; i < 8; i++) {
    float a = spin + i * PI / 4;
    float len = 6 + 2 * sinf(t * 0.005f + i);
    float ca = cosf(a), sa = sinf(a);
    int x0 = cx + (int)lroundf(ca * (r + 5)), y0 = cy + (int)lroundf(sa * (r + 5));
    int x1 = cx + (int)lroundf(ca * (r + 5 + len)), y1 = cy + (int)lroundf(sa * (r + 5 + len));
    g.drawLine(x0, y0, x1, y1, COL_SUN);
    g.drawLine(x0 + 1, y0, x1 + 1, y1, COL_SUN);
    g.drawLine(x0, y0 + 1, x1, y1 + 1, COL_SUN);
  }
}

// Trang luoi liem, ba ngoi sao nhap nhay
static void drawMoon(M5Canvas &g, int cx, int cy, int r, uint32_t t) {
  // Luoi liem = hinh tron lon tru hinh tron nho lech phai-tren. Ve tung diem de phan bi
  // khuyet de lo anh nen (khong the xoa bang mau dac).
  const int ox = cx + r / 2, oy = cy - r / 3, r2 = r - 2;
  for (int y = -r; y <= r; y++) {
    for (int x = -r; x <= r; x++) {
      if (x * x + y * y > r * r) continue;
      int dx = cx + x - ox, dy = cy + y - oy;
      if (dx * dx + dy * dy <= r2 * r2) continue;
      g.drawPixel(cx + x, cy + y, 0xEF5B);
    }
  }
  static const int8_t STARS[3][2] = {{22, -22}, {27, 6}, {-24, -26}};
  for (int i = 0; i < 3; i++) {
    float k = 0.5f + 0.5f * sinf(t * 0.004f + i * 2.1f);
    uint8_t v = (uint8_t)(90 + 165 * k);
    uint16_t col = g.color565(v, v, v);
    int x = cx + STARS[i][0], y = cy + STARS[i][1];
    g.drawFastHLine(x - 2, y, 5, col);
    g.drawFastVLine(x, y - 2, 5, col);
  }
}

static void drawWeatherIcon(M5Canvas &g, int cx, int cy, WeatherKind kind, bool isDay, uint32_t t) {
  int bob = (int)lroundf(sinf(t * 0.003f) * 2);     // may nhun len xuong
  int drift = (int)lroundf(sinf(t * 0.0017f) * 4);  // may troi ngang
  switch (kind) {
    case WX_CLEAR:
      if (isDay) drawSun(g, cx, cy, 15, t); else drawMoon(g, cx, cy, 20, t);
      break;

    case WX_PARTLY:
      if (isDay) drawSun(g, cx - 12, cy - 12, 10, t); else drawMoon(g, cx - 12, cy - 12, 12, t);
      drawCloud(g, cx + 4 + drift, cy + 8 + bob, COL_CLOUD);
      break;

    case WX_CLOUDY:
      drawCloud(g, cx - 6 - drift, cy - 8, COL_DIM);
      drawCloud(g, cx + 4 + drift, cy + 6 + bob, COL_CLOUD);
      break;

    case WX_FOG:
      drawCloud(g, cx, cy - 12 + bob, COL_DIM);
      for (int i = 0; i < 3; i++) {
        int off = (int)lroundf(sinf(t * 0.002f + i * 1.7f) * 8);   // dai suong troi qua lai
        g.fillRoundRect(cx - 24 + off, cy + 10 + i * 9, 46, 3, 1, COL_CLOUD);
      }
      break;

    case WX_RAIN:
      drawCloud(g, cx, cy - 12 + bob, COL_CLOUD);
      for (int i = 0; i < 6; i++) {   // giot mua roi cheo, moi giot lech pha
        float ph = fmodf(t * 0.06f + i * 11.0f, 24.0f);   // roi het trong khung sprite
        int x = cx - 22 + i * 9 - (int)(ph * 0.15f);
        int y = cy + 8 + (int)ph;
        g.drawLine(x, y, x - 2, y + 6, COL_ACCENT);
      }
      break;

    case WX_SNOW:
      drawCloud(g, cx, cy - 12 + bob, COL_CLOUD);
      for (int i = 0; i < 6; i++) {   // bong tuyet roi cham va lac nhe
        float ph = fmodf(t * 0.02f + i * 7.0f, 26.0f);
        int x = cx - 20 + i * 8 + (int)lroundf(sinf(t * 0.003f + i) * 3);
        g.fillCircle(x, cy + 8 + (int)ph, 2, TFT_WHITE);
      }
      break;

    case WX_STORM: {
      uint32_t p = t % 2000;
      bool flash = p < 120 || (p > 260 && p < 360);   // hai lan loe sang moi 2 giay
      drawCloud(g, cx, cy - 12 + bob, flash ? COL_CLOUD : COL_DIM);
      if (flash) {
        g.fillTriangle(cx + 4, cy + 4, cx - 8, cy + 20, cx + 1, cy + 20, COL_SUN);
        g.fillTriangle(cx - 2, cy + 18, cx + 7, cy + 18, cx - 6, cy + 34, COL_SUN);
      }
      break;
    }
  }
}

static void drawIconFrame(uint32_t t) {
  if (!iconReady) {
    iconCanvas.setColorDepth(16);
    iconReady = iconCanvas.createSprite(ICON_SIZE, ICON_SIZE) != nullptr;
    if (!iconReady) return;
  }
  drawBg(iconCanvas, 0, 0, ICON_X, ICON_Y, ICON_SIZE, ICON_SIZE);
  drawWeatherIcon(iconCanvas, ICON_SIZE / 2, ICON_SIZE / 2, kindOf(weather.code), weather.isDay, t);
  iconCanvas.pushSprite(&M5.Display, ICON_X, ICON_Y);
}

// ----- Hang so lieu: nhiet do | do am | gio, moi muc co mot icon nho ve bang code -----
static void drawThermoIcon(int x, int cy, uint16_t c) {   // rong 10
  auto &d = M5.Display;
  d.fillRoundRect(x + 3, cy - 9, 5, 14, 2, c);
  d.fillCircle(x + 5, cy + 7, 5, c);
  d.fillRect(x + 5, cy - 6, 1, 11, TFT_WHITE);   // vach thuy ngan
}

static void drawDropIcon(int x, int cy, uint16_t c) {     // rong 12
  auto &d = M5.Display;
  d.fillTriangle(x + 6, cy - 10, x + 1, cy + 1, x + 11, cy + 1, c);
  d.fillCircle(x + 6, cy + 4, 6, c);
  d.fillCircle(x + 4, cy + 3, 1, TFT_WHITE);
}

static void drawWindIcon(int x, int cy, uint16_t c) {     // rong 17
  auto &d = M5.Display;
  d.fillRoundRect(x, cy - 7, 14, 2, 1, c);
  d.fillRoundRect(x + 3, cy - 1, 14, 2, 1, c);
  d.fillRoundRect(x, cy + 5, 10, 2, 1, c);
}

static const int STATS_X = 92, STATS_RIGHT = 316, STATS_GAP = 12;

static void drawStatsRow(int cy) {
  auto &d = M5.Display;
  char tbuf[8], hbuf[8], wbuf[8];
  snprintf(tbuf, sizeof(tbuf), "%d", (int)lroundf(weather.temp));
  snprintf(hbuf, sizeof(hbuf), "%d%%", weather.humidity);
  snprintf(wbuf, sizeof(wbuf), "%d", (int)lroundf(weather.wind));

  // Do do rong: nhiet do co chu lon neu ca hang van vua khung, khong thi ha xuong co vua
  d.setFont(&fonts::FreeSansBold9pt7b);
  const int humW = d.textWidth(hbuf), windW = d.textWidth(wbuf);
  d.setFont(&fonts::Font2);
  const int unitW = d.textWidth("km/h");
  d.setFont(&fonts::FreeSansBold18pt7b);
  const int tempBigW = d.textWidth(tbuf);
  d.setFont(&fonts::FreeSansBold12pt7b);
  const int cBigW = d.textWidth("C");
  const int rest = (16 + humW) + (20 + windW + 4 + unitW) + 2 * STATS_GAP;
  const bool big = 14 + tempBigW + 12 + cBigW + rest <= STATS_RIGHT - STATS_X;
  const int degW = big ? 12 : 9;

  int x = STATS_X;
  d.setTextDatum(m5gfx::textdatum_t::middle_left);

  // Nhiet do
  drawThermoIcon(x, cy, d.color565(255, 110, 70));
  x += 14;
  d.setTextColor(col(C_TEMP));
  if (big) d.setFont(&fonts::FreeSansBold18pt7b); else d.setFont(&fonts::FreeSansBold12pt7b);
  x += d.drawString(tbuf, x, cy);
  d.drawCircle(x + 4, cy - (big ? 10 : 7), 3, col(C_TEMP));
  d.drawCircle(x + 4, cy - (big ? 10 : 7), 2, col(C_TEMP));
  if (big) d.setFont(&fonts::FreeSansBold12pt7b); else d.setFont(&fonts::FreeSansBold9pt7b);
  x += degW;
  x += d.drawString("C", x, cy + (big ? 2 : 1));
  x += STATS_GAP;

  // Do am
  drawDropIcon(x, cy, d.color565(70, 180, 255));
  x += 16;
  d.setFont(&fonts::FreeSansBold9pt7b);
  d.setTextColor(col(C_VALUE));
  x += d.drawString(hbuf, x, cy);
  x += STATS_GAP;

  // Gio
  drawWindIcon(x, cy, d.color565(180, 220, 235));
  x += 20;
  d.setFont(&fonts::FreeSansBold9pt7b);
  x += d.drawString(wbuf, x, cy);
  d.setFont(&fonts::Font2);
  d.drawString("km/h", x + 4, cy + 2);
}

// Khu vuc: ghim ban do + ten, canh phai, nam cung hang voi mo ta thoi tiet
static String shortenToFit(const String &name, int maxW) {
  auto &d = M5.Display;
  String s = name;
  while (s.length() > 4 && d.textWidth(s.c_str()) > maxW) {
    int i = s.length() - 1;
    while (i > 0 && ((uint8_t)s[i] & 0xC0) == 0x80) i--;   // lui ve dau ky tu UTF-8 cuoi
    s.remove(i);
    s.trim();
  }
  return s == name ? s : s + "...";
}

static void drawLocation(int cy, int maxW) {
  auto &d = M5.Display;
  const uint16_t lc = col(C_LABEL);
  d.loadFont(VI_R14);
  String s = shortenToFit(locName, maxW);
  int w = d.textWidth(s.c_str());
  d.setTextDatum(m5gfx::textdatum_t::middle_right);
  d.setTextColor(lc);
  d.drawString(s.c_str(), STATS_RIGHT, cy);
  d.unloadFont();
  int px = STATS_RIGHT - w - 12;   // ghim ban do ben trai ten
  d.fillCircle(px + 4, cy - 3, 4, lc);
  d.fillTriangle(px, cy - 1, px + 8, cy - 1, px + 4, cy + 7, lc);
  d.fillCircle(px + 4, cy - 3, 1, TFT_BLACK);
}

// Mo ta thoi tiet (tu ma WMO): vua khung thi dung yen, dai hon thi chay chu ngang
static const int DESC_X = 92, DESC_W = 98, DESC_H = 30;   // du cao cho dau mu/dau thanh
static const int ROW1_CY = WEATHER_Y + 28;   // hang nhiet do | do am | gio
static const int ROW2_CY = WEATHER_Y + 70;   // hang mo ta | khu vuc
static const int DESC_Y = ROW2_CY - 14;
static M5Canvas descCanvas;
static bool descReady = false;
static int descTextW = 0;

static void drawDescFrame(uint32_t t) {
  if (!descReady) {
    descCanvas.setColorDepth(16);
    descReady = descCanvas.createSprite(DESC_W, DESC_H) != nullptr;
    if (!descReady) return;
    descCanvas.loadFont(VI_R18);   // giu font nay trong suot vong doi sprite
  }
  const char *text = describe(weather.code);
  descCanvas.setTextDatum(m5gfx::textdatum_t::top_left);
  descCanvas.setTextColor(col(C_DESC));
  descTextW = descCanvas.textWidth(text);

  drawBg(descCanvas, 0, 0, DESC_X, DESC_Y, DESC_W, DESC_H);
  if (descTextW <= DESC_W) {
    descCanvas.drawString(text, 0, 2);
  } else {
    int span = descTextW + 32;              // chu + khoang trong truoc khi lap lai
    int off = (int)((t / 33) % span);       // ~30 px/giay
    descCanvas.drawString(text, -off, 2);
    descCanvas.drawString(text, span - off, 2);
  }
  descCanvas.pushSprite(&M5.Display, DESC_X, DESC_Y);
}

static void drawWeatherMessage(const char *line1, const char *line2, const char *line3 = nullptr) {
  auto &d = M5.Display;
  d.setTextDatum(m5gfx::textdatum_t::middle_center);
  d.loadFont(VI_B18);
  d.setTextColor(TFT_WHITE);
  d.drawString(line1, 160, WEATHER_Y + (line3 ? 24 : 34));
  d.unloadFont();
  d.loadFont(VI_R14);
  d.setTextColor(line3 ? TFT_WHITE : COL_DIM);
  d.drawString(line2, 160, WEATHER_Y + (line3 ? 50 : 62));
  if (line3) d.drawString(line3, 160, WEATHER_Y + 72);
  d.unloadFont();
}

static void drawWeather() {
  auto &d = M5.Display;
  // Xoa tu ngay duoi thanh giay (man hinh ma QR dung them vai diem anh phia tren vung thoi tiet)
  const int top = WEATHER_Y - 4;
  drawBg(d, 0, top, 0, top, d.width(), WEATHER_H);

  if (apActive) {   // dang phat WiFi cau hinh: ma QR thay cho thoi tiet
    // Ma trai: vao WiFi cua dong ho (khong can go ten va mat khau). Ma phai: mo trang cai dat.
    // Nen trang rong quanh ma de may quet nhan duoc tren anh nen toi.
    char wifiQr[80], url[40];
    snprintf(wifiQr, sizeof(wifiQr), "WIFI:T:WPA;S:%s;P:%s;;", AP_SSID, AP_PASS);
    IPAddress apIp = WiFi.softAPIP();
    if (apIp == IPAddress(0, 0, 0, 0)) apIp = IPAddress(192, 168, 4, 1);   // AP chua bat (xem truoc)
    snprintf(url, sizeof(url), "http://%s", apIp.toString().c_str());
    const int h = WEATHER_H;
    d.fillRect(0, top, 110, h, TFT_WHITE);
    d.qrcode(wifiQr, 9, top + (h - 92) / 2, 92, 3);
    d.fillRect(212, top, 108, h, TFT_WHITE);
    d.qrcode(url, 227, top + (h - 78) / 2, 78, 2);
    d.fillRect(110, top, 102, h, COL_HEADER);   // nen toi cho chu o giua, doc duoc tren moi anh nen

    d.loadFont(VI_R14);
    d.setTextDatum(m5gfx::textdatum_t::middle_center);
    d.setTextColor(TFT_WHITE);
    d.drawString("< 1. Vào WiFi", 161, top + 14);
    d.drawString("2. Mở trang >", 161, top + 84);
    d.setTextColor(TFT_LIGHTGREY);
    d.drawString(AP_SSID, 161, top + 38);
    char pass[24];
    snprintf(pass, sizeof(pass), "MK: %s", AP_PASS);
    d.drawString(pass, 161, top + 58);
    d.unloadFont();
    return;
  }

  if (!weather.valid) {
    if (!wifiConfigured()) {
      drawWeatherMessage("Chưa cấu hình WiFi", "Đang bật chế độ cài đặt bằng điện thoại...");
    } else if (WiFi.status() != WL_CONNECTED) {
      drawWeatherMessage("Đang kết nối WiFi...", wifiSsid.c_str());
    } else {
      drawWeatherMessage(weatherEverTried ? "Không lấy được thời tiết" : "Đang tải thời tiết...",
                         weatherEverTried ? "Sẽ thử lại sau 1 phút (giữ nút A: thử ngay)" : locName.c_str());
    }
    return;
  }

  drawIconFrame(millis());   // phan con lai do loop() cap nhat ~15 khung/giay

  drawStatsRow(ROW1_CY);       // nhiet do | do am | gio, moi muc mot icon
  drawDescFrame(millis());     // mo ta thoi tiet; neu dai hon khung, loop() cho chay ngang
  drawLocation(ROW2_CY, STATS_RIGHT - (DESC_X + DESC_W) - 18);   // khu vuc, cung hang voi mo ta
}

// ---------- Am lich (thuat toan Ho Ngoc Duc, mui gio UTC+7 theo lich Viet Nam) ----------

static int jdFromDate(int dd, int mm, int yy) {
  int a = (14 - mm) / 12, y = yy + 4800 - a, m = mm + 12 * a - 3;
  int jd = dd + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
  if (jd < 2299161) jd = dd + (153 * m + 2) / 5 + 365 * y + y / 4 - 32083;
  return jd;
}

static double newMoonJd(int k) {   // ngay Julian cua ky Sóc thu k (tinh theo UT)
  const double dr = PI / 180.0;
  double T = k / 1236.85, T2 = T * T, T3 = T2 * T;
  double jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * T2 - 0.000000155 * T3;
  jd1 += 0.00033 * sin((166.56 + 132.87 * T - 0.009173 * T2) * dr);
  double M = 359.2242 + 29.10535608 * k - 0.0000333 * T2 - 0.00000347 * T3;
  double Mpr = 306.0253 + 385.81691806 * k + 0.0107306 * T2 + 0.00001236 * T3;
  double F = 21.2964 + 390.67050646 * k - 0.0016528 * T2 - 0.00000239 * T3;
  double C1 = (0.1734 - 0.000393 * T) * sin(M * dr) + 0.0021 * sin(2 * dr * M);
  C1 -= 0.4068 * sin(Mpr * dr) - 0.0161 * sin(dr * 2 * Mpr);
  C1 -= 0.0004 * sin(dr * 3 * Mpr);
  C1 += 0.0104 * sin(dr * 2 * F) - 0.0051 * sin(dr * (M + Mpr));
  C1 -= 0.0074 * sin(dr * (M - Mpr)) - 0.0004 * sin(dr * (2 * F + M));
  C1 -= 0.0004 * sin(dr * (2 * F - M)) + 0.0006 * sin(dr * (2 * F + Mpr));
  C1 += 0.0010 * sin(dr * (2 * F - Mpr)) + 0.0005 * sin(dr * (2 * Mpr + M));
  double dt = T < -11 ? 0.001 + 0.000839 * T + 0.0002261 * T2 - 0.00000845 * T3 - 0.000000081 * T * T3
                      : -0.000278 + 0.000265 * T + 0.000262 * T2;
  return jd1 + C1 - dt;
}

static int sunLongitudeIdx(int jdn, int tz) {   // kinh do Mat Troi luc nua dem dia phuong, chia 30 do (0..11)
  const double dr = PI / 180.0;
  double T = (jdn - 0.5 - tz / 24.0 - 2451545.0) / 36525.0, T2 = T * T;
  double M = 357.52910 + 35999.05030 * T - 0.0001559 * T2 - 0.00000048 * T * T2;
  double L0 = 280.46645 + 36000.76983 * T + 0.0003032 * T2;
  double DL = (1.914600 - 0.004817 * T - 0.000014 * T2) * sin(dr * M);
  DL += (0.019993 - 0.000101 * T) * sin(dr * 2 * M) + 0.000290 * sin(dr * 3 * M);
  double L = (L0 + DL) * dr;
  L -= PI * 2 * floor(L / (PI * 2));
  return (int)floor(L / PI * 6);
}

static int newMoonDay(int k, int tz) { return (int)floor(newMoonJd(k) + 0.5 + tz / 24.0); }

static int lunarMonth11(int yy, int tz) {   // ngay mung 1 thang 11 am lich (chua dong chi)
  int off = jdFromDate(31, 12, yy) - 2415021;
  int k = (int)floor(off / 29.530588853);
  int nm = newMoonDay(k, tz);
  if (sunLongitudeIdx(nm, tz) >= 9) nm = newMoonDay(k - 1, tz);
  return nm;
}

static int leapMonthOffset(int a11, int tz) {
  int k = (int)floor((a11 - 2415021.076998695) / 29.530588853 + 0.5);
  int last, i = 1;
  int arc = sunLongitudeIdx(newMoonDay(k + i, tz), tz);
  do {
    last = arc;
    i++;
    arc = sunLongitudeIdx(newMoonDay(k + i, tz), tz);
  } while (arc != last && i < 14);
  return i - 1;
}

struct LunarDate { int day, month, year; bool leap; };

static LunarDate solarToLunar(int dd, int mm, int yy, int tz = 7) {
  int dayNumber = jdFromDate(dd, mm, yy);
  int k = (int)floor((dayNumber - 2415021.076998695) / 29.530588853);
  int monthStart = newMoonDay(k + 1, tz);
  if (monthStart > dayNumber) monthStart = newMoonDay(k, tz);
  int a11 = lunarMonth11(yy, tz), b11 = a11, year;
  if (a11 >= monthStart) {
    year = yy;
    a11 = lunarMonth11(yy - 1, tz);
  } else {
    year = yy + 1;
    b11 = lunarMonth11(yy + 1, tz);
  }
  LunarDate r;
  r.day = dayNumber - monthStart + 1;
  int diff = (monthStart - a11) / 29;
  r.leap = false;
  r.month = diff + 11;
  if (b11 - a11 > 365) {   // nam co thang nhuan
    int leapDiff = leapMonthOffset(a11, tz);
    if (diff >= leapDiff) {
      r.month = diff + 10;
      if (diff == leapDiff) r.leap = true;
    }
  }
  if (r.month > 12) r.month -= 12;
  if (r.month >= 11 && diff < 4) year -= 1;
  r.year = year;
  return r;
}

static const char *CAN_NAMES[10] = {"Giáp", "Ất", "Bính", "Đinh", "Mậu", "Kỷ", "Canh", "Tân", "Nhâm", "Quý"};
static const char *CHI_NAMES[12] = {"Tý", "Sửu", "Dần", "Mão", "Thìn", "Tỵ", "Ngọ", "Mùi", "Thân", "Dậu", "Tuất", "Hợi"};

// Ngay tren thanh tieu de: 0 = duong lich, 1 = am lich, 2 = tu dong luan phien moi 10 giay (mac dinh).
// Cham vao dong ngay de doi vong 2 -> 0 -> 1 -> 2; luu NVS.
static uint8_t dateMode = 2;
static const uint32_t DATE_SWAP_SEC = 10;

static void drawHeader(const struct tm &t) {
  static const char *WEEKDAYS[] = {"Chủ nhật", "Thứ hai", "Thứ ba", "Thứ tư", "Thứ năm", "Thứ sáu", "Thứ bảy"};

  char date[48] = "--/--/----";
  char dateShort[40] = "";   // am lich khong kem ten nam, dung khi dong ngay qua dai
  if (timeValid(t)) {
    const bool showLunar = dateMode == 1 || (dateMode == 2 && (time(nullptr) / DATE_SWAP_SEC) % 2 == 1);
    if (showLunar) {
      // Chi tinh lai khi qua ngay moi (phep tinh co nhieu ham luong giac)
      static int cacheKey = -1;
      static LunarDate lunar;
      int key = (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
      if (key != cacheKey) {
        lunar = solarToLunar(t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
        cacheKey = key;
      }
      snprintf(dateShort, sizeof(dateShort), "%s, ÂL %d/%d%s", WEEKDAYS[t.tm_wday], lunar.day, lunar.month,
               lunar.leap ? " nhuận" : "");
      snprintf(date, sizeof(date), "%s %s %s", dateShort, CAN_NAMES[(lunar.year + 6) % 10],
               CHI_NAMES[(lunar.year + 8) % 12]);
    } else {
      snprintf(date, sizeof(date), "%s, %02d/%02d/%04d", WEEKDAYS[t.tm_wday], t.tm_mday, t.tm_mon + 1,
               t.tm_year + 1900);
    }
  }
  // Muc song WiFi: 0..3 cung sang them cham tron (-1 = mat ket noi). Pin: bieu tuong, khong con so.
  const bool wifiUp = WiFi.status() == WL_CONNECTED;
  int level = -1;
  if (wifiUp) {
    int rssi = WiFi.RSSI();
    level = rssi >= -60 ? 3 : rssi >= -70 ? 2 : rssi >= -80 ? 1 : 0;
  }
  const int batt = constrain((int)M5.Power.getBatteryLevel(), 0, 100);
  const bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  const bool toast = (int32_t)(toastUntil - millis()) > 0;

  // Chi ve lai khi noi dung thay doi (ngay thang, thong bao, muc song, pin)
  String header = String(date) + (toast ? toastText : "") + (toast ? "T" : "B") + String(level) + "," +
                  String(batt / 5) + (charging ? "c" : "n");
  if (header == lastHeader) return;
  lastHeader = header;

  auto &d = M5.Display;
  d.fillRect(0, 0, d.width(), HEADER_H, COL_HEADER);
  d.loadFont(VI_B18);
  d.setTextColor(col(C_DATE), COL_HEADER);
  d.setTextDatum(m5gfx::textdatum_t::middle_left);
  // Chua qua phan icon ben phai (~x=245); am lich qua dai thi bo ten nam
  const char *shown = (dateShort[0] && d.textWidth(date) > 236) ? dateShort : date;
  d.drawString(shown, 8, HEADER_H / 2);
  d.unloadFont();

  if (toast) {   // thong bao tam thoi thay cho cac icon
    d.setFont(&fonts::Font2);
    d.setTextColor(wifiUp ? TFT_GREEN : COL_DIM, COL_HEADER);
    d.setTextDatum(m5gfx::textdatum_t::middle_right);
    d.drawString(toastText, d.width() - 8, HEADER_H / 2);
    return;
  }

  const uint16_t off = 0x4208;   // vach / cung tat
  const int cy = HEADER_H / 2;

  // Pin: khung bo goc 22x11, dau cuc, ruot do day theo phan tram
  const int bx = d.width() - 8 - 25, by = cy - 6;
  const uint16_t bcol = charging ? d.color565(255, 214, 0) : (batt <= 15 ? TFT_RED : batt <= 30 ? d.color565(255, 140, 40) : TFT_GREEN);
  d.drawRoundRect(bx, by, 22, 12, 3, TFT_LIGHTGREY);
  d.fillRect(bx + 22, by + 4, 2, 4, TFT_LIGHTGREY);
  int fill = (18 * batt + 50) / 100;
  if (batt > 0 && fill < 1) fill = 1;
  if (fill > 0) d.fillRoundRect(bx + 2, by + 2, fill, 8, 1, bcol);
  if (charging) {   // tia chop chong len ruot pin
    d.fillTriangle(bx + 12, by + 1, bx + 7, by + 7, bx + 11, by + 7, TFT_WHITE);
    d.fillTriangle(bx + 10, by + 11, bx + 15, by + 5, bx + 11, by + 5, TFT_WHITE);
  }

  // WiFi: cham tron o duoi + 3 cung toa len tren (phong cach bieu tuong WiFi pho bien)
  const int wx = bx - 12 - 8, wy = cy + 8;   // tam cac cung (diem goc)
  const int lit = level < 0 ? 0 : level;      // so cung dang sang
  const uint16_t wcol = level < 0 ? off : (level == 0 ? d.color565(255, 140, 40) : TFT_GREEN);
  d.fillCircle(wx, wy - 1, 2, level < 0 ? off : wcol);
  static const int R0[3] = {5, 9, 13};
  for (int i = 0; i < 3; i++) {
    d.fillArc(wx, wy - 1, R0[i], R0[i] + 2, 227, 313, i < lit ? wcol : off);
  }
  if (level < 0) {   // mat ket noi: gach cheo do
    d.drawLine(wx - 11, wy + 1, wx + 11, wy - 15, TFT_RED);
    d.drawLine(wx - 10, wy + 1, wx + 12, wy - 15, TFT_RED);
  }
}

static const uint8_t CLOCK_STYLES = 6;
static const char *CLOCK_STYLE_NAMES[CLOCK_STYLES] = {"So: DejaVu",   "So: Font8", "So: 7 doan",
                                                       "So: Orbitron", "So: Day dam", "So: Manh"};

// Chon font + co chu cho kieu so hien tai (moi kieu cao toi da ~90 px)
static void applyClockFont(M5GFX &d) {
  switch (clockStyle) {
    case 1: d.setFont(&fonts::Font8); d.setTextSize(1); break;                 // so tron, day
    case 2: d.setFont(&fonts::Font7); d.setTextSize(2); break;                 // 7 doan (LED)
    case 3: d.setFont(&fonts::Orbitron_Light_32); d.setTextSize(2); break;     // cong nghe
    case 4: d.setFont(&fonts::FreeSansBold24pt7b); d.setTextSize(1.6f); break; // sans dam
    case 5: d.setFont(&fonts::Roboto_Thin_24); d.setTextSize(2.5f); break;     // net manh
    default: d.setFont(&fonts::DejaVu72); d.setTextSize(1); break;             // min, mac dinh
  }
}

static void drawClock(const struct tm &t) {
  auto &d = M5.Display;
  const int cx = d.width() / 2;

  if (t.tm_min != lastMinute) {
    lastMinute = t.tm_min;
    char hh[4] = "--", mm[4] = "--";
    if (timeValid(t)) {
      snprintf(hh, sizeof(hh), "%02d", t.tm_hour);
      snprintf(mm, sizeof(mm), "%02d", t.tm_min);
    }
    // Gio va phut ve rieng hai ben, dau hai cham o giua ve tay de nhap nhay moi giay
    drawBg(d, 0, TIME_CY - 48, 0, TIME_CY - 48, d.width(), 96);
    applyClockFont(d);
    d.setTextColor(col(C_TIME));
    d.setTextDatum(m5gfx::textdatum_t::middle_right);
    d.drawString(hh, cx - 16, TIME_CY);
    d.setTextDatum(m5gfx::textdatum_t::middle_left);
    d.drawString(mm, cx + 16, TIME_CY);
    d.setTextSize(1);   // cac ham ve khac mac dinh co chu 1
  }

  // Dau hai cham nhap nhay theo giay
  drawBg(d, cx - 8, TIME_CY - 22, cx - 8, TIME_CY - 22, 16, 44);   // xoa dau cu bang anh nen
  if (!timeValid(t) || t.tm_sec % 2 == 0) {
    d.fillCircle(cx, TIME_CY - 14, 5, col(C_COLON));
    d.fillCircle(cx, TIME_CY + 14, 5, col(C_COLON));
  }
}

// ---------- LED trang tri ----------

// Chay rieng mot task de hieu ung van muot ke ca khi loop() dang cho HTTP
static void ledTask(void *) {
  uint32_t step = 0;
  for (;;) {
    if (inMusic && !sleeping) {
      // Nhay theo nhac: sang theo do lon, loe trang hon khi co nhip bass, mau troi dan
      uint8_t lvl = musicLevel, beat = musicBeat;
      uint8_t v = lvl > beat ? lvl : beat;
      leds.fill(leds.gamma32(leds.ColorHSV((uint16_t)(step * 300), 255 - beat / 2, v)));
      leds.show();
      step++;
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    uint8_t mode = sleeping ? LED_OFF : ledMode;
    switch (mode) {
      case LED_RAINBOW:
        leds.rainbow((uint16_t)(step * 400), 1, 255, 255, true);
        break;

      case LED_BREATH: {  // tho nhip ~4 giay, mau troi cham theo vong mau
        float k = (1.0f - cosf(step * 0.0471f)) * 0.5f;   // 0..1, chu ky ~133 buoc
        uint8_t v = (uint8_t)(12 + 243 * k);
        leds.fill(leds.gamma32(leds.ColorHSV((uint16_t)(step * 60), 255, v)));
        break;
      }

      case LED_CHASE: {  // mot vet sang chay vong quanh thanh LED
        uint16_t hue = (uint16_t)(step * 120);
        uint8_t head = (step / 2) % LED_COUNT;
        for (uint8_t i = 0; i < LED_COUNT; i++) {
          uint8_t d = (head + LED_COUNT - i) % LED_COUNT;   // khoang cach sau dau vet
          uint8_t v = d < 5 ? (uint8_t)(255 >> d) : 0;
          leds.setPixelColor(i, leds.gamma32(leds.ColorHSV(hue, 255, v)));
        }
        break;
      }

      default:
        leds.clear();
        break;
    }
    leds.show();
    step++;
    vTaskDelay(pdMS_TO_TICKS(mode == LED_OFF ? 100 : 30));
  }
}

// ---------- Che do ngu ----------

static void enterSleep(bool manual) {
  sleeping = true;
  manualSleep = manual;
  M5.Display.setBrightness(0);   // tat den nen; LED duoc task LED tat theo co `sleeping`
  Serial.printf("Ngu (%s)\n", manual ? "thu cong" : "theo lich");
}

static void wakeUp() {
  sleeping = false;
  manualSleep = false;
  lastActivityMs = millis();
  M5.Display.setBrightness(BRIGHTNESS_LEVELS[brightnessIdx]);
  Serial.println("Danh thuc");
}

// Gio hien tai co nam trong khoang ngu khong (ho tro khoang qua nua dem, vi du 23:00-06:00)
static bool inSleepWindow(const struct tm &t) {
  if (!sched.enabled || !timeValid(t)) return false;
  int nowMin = t.tm_hour * 60 + t.tm_min;
  int start = sched.startH * 60 + sched.startM;
  int end = sched.endH * 60 + sched.endM;
  if (start == end) return false;
  return start < end ? (nowMin >= start && nowMin < end) : (nowMin >= start || nowMin < end);
}

static void setLedMode(uint8_t mode) {
  ledMode = mode;
  toastText = LED_MODE_NAMES[mode];
  toastUntil = millis() + 2000;
  lastSecond = 0;   // ve lai thanh tieu de ngay de hien thong bao
  Serial.printf("%s\n", LED_MODE_NAMES[mode]);
}

static void setDateMode(uint8_t mode) {
  dateMode = mode % 3;
  Preferences prefs;
  if (prefs.begin("clock", false)) {
    prefs.putUChar("dateMode", dateMode);
    prefs.end();
  }
  lastHeader = "";   // ve lai thanh tieu de ngay (khong hien chu thong bao)
  lastSecond = 0;
  Serial.printf("Ngay: %s\n", dateMode == 0 ? "duong lich" : dateMode == 1 ? "am lich" : "tu dong");
}

static void cycleClockStyle() {
  clockStyle = (clockStyle + 1) % CLOCK_STYLES;
  Preferences prefs;
  if (prefs.begin("clock", false)) {
    prefs.putUChar("tmStyle", clockStyle);
    prefs.end();
  }
  toastText = CLOCK_STYLE_NAMES[clockStyle];
  toastUntil = millis() + 2000;
  lastMinute = -1;   // ve lai so ngay
  lastSecond = 0;
  Serial.printf("%s\n", CLOCK_STYLE_NAMES[clockStyle]);
}

// ---------- Cai dat gio ngu (nut A) ----------
//   A (bam): sang muc tiep theo, muc cuoi la "Luu".  A (giu): huy bo.
//   B: giam.  C: tang.  Tu thoat (huy) sau 60 giay khong thao tac.

static const uint8_t SETTINGS_FIELDS = 5;
static const uint32_t SETTINGS_IDLE_MS = 60000;
static bool inSettings = false;

// Dang hien man hinh khac (cai dat hoac nhac) de len dong ho: khong ve dong ho / thoi tiet
static bool overlayActive() { return inSettings || inMusic; }
static SleepSchedule editSched;
static uint8_t editField = 0;
static bool settingsFull = false;        // ve lai toan bo (chi khi vua vao man hinh cai dat)
static uint8_t settingsRowsDirty = 0;    // bit i = muc i can ve lai
static bool settingsHeaderDirty = false;
static bool settingsBtnDirty = false;
static M5Canvas rowCanvas, hdrCanvas, btnCanvas;

static void loadSchedule() {
  Preferences prefs;
  if (!prefs.begin("clock", true)) return;   // chua co du lieu: dung mac dinh
  sched.enabled = prefs.getBool("slEn", sched.enabled);
  sched.startH = prefs.getUChar("slSH", sched.startH) % 24;
  sched.startM = prefs.getUChar("slSM", sched.startM) % 60;
  sched.endH = prefs.getUChar("slEH", sched.endH) % 24;
  sched.endM = prefs.getUChar("slEM", sched.endM) % 60;
  sched.idleSec = constrain(prefs.getUShort("slIdle", sched.idleSec), 5, 3600);
  for (uint8_t i = 0; i < C_COUNT; i++) {
    char key[8];
    snprintf(key, sizeof(key), "col%u", i);
    colorRgb[i] = prefs.getUInt(key, colorRgb[i]) & 0xFFFFFF;
  }
  clockStyle = prefs.getUChar("tmStyle", clockStyle) % CLOCK_STYLES;
  musicStyle = prefs.getUChar("muStyle", musicStyle) % MUSIC_STYLE_COUNT;
  dateMode = prefs.getUChar("dateMode", dateMode) % 3;
  chimeOn = prefs.getBool("chOn", chimeOn);
  chimeVol = min<uint8_t>(100, prefs.getUChar("chVol", chimeVol));
  chimeQuiet = prefs.getBool("chQuiet", chimeQuiet);
  locAuto = prefs.getBool("locAuto", locAuto);
  if (prefs.isKey("locName")) {   // khu vuc da luu (ket qua tra cuu gan nhat hoac nhap tay)
    locName = prefs.getString("locName", locName);
    if (locAuto) locName = shortenPlace(locName);   // ten da luu truoc khi co ban rut gon
    locLat = prefs.getFloat("locLat", locLat);
    locLon = prefs.getFloat("locLon", locLon);
  }
  prefs.end();
}

static void saveSchedule() {
  Preferences prefs;
  if (!prefs.begin("clock", false)) return;
  prefs.putBool("slEn", sched.enabled);
  prefs.putUChar("slSH", sched.startH);
  prefs.putUChar("slSM", sched.startM);
  prefs.putUChar("slEH", sched.endH);
  prefs.putUChar("slEM", sched.endM);
  prefs.putUShort("slIdle", sched.idleSec);
  prefs.end();
}

// Ve qua bo dem (sprite) roi day len man hinh mot lan, nen khong bi nhap nhay
static const int ROW_X = 6, ROW_Y0 = 32, ROW_W = 308, ROW_H = 27, ROW_STEP = 29;
static const uint8_t INFO_ROW = SETTINGS_FIELDS;   // dong cuoi: dia chi web (chi hien thi)
static const int RANGE_W = 150;
static const int BTN_LABEL_W = 100, BTN_LABEL_H = 20, BTN_LABEL_Y = 219;
static const int BTN_CENTER_X[3] = {64, 160, 256};   // tam 3 nut cam ung A / B / C

static void drawRow(uint8_t i) {
  static const char *LABELS[SETTINGS_FIELDS + 1] = {"Tu dong ngu",  "Gio bat dau ngu", "Phut bat dau ngu",
                                                    "Gio thuc day", "Phut thuc day",   "Dia chi web"};
  char buf[24];
  switch (i) {
    case 0: snprintf(buf, sizeof(buf), "%s", editSched.enabled ? "BAT" : "TAT"); break;
    case 1: snprintf(buf, sizeof(buf), "%02d", editSched.startH); break;
    case 2: snprintf(buf, sizeof(buf), "%02d", editSched.startM); break;
    case 3: snprintf(buf, sizeof(buf), "%02d", editSched.endH); break;
    case 4: snprintf(buf, sizeof(buf), "%02d", editSched.endM); break;
    default:   // IP de mo trang cau hinh tren dien thoai
      if (WiFi.status() == WL_CONNECTED) snprintf(buf, sizeof(buf), "%s", WiFi.localIP().toString().c_str());
      else snprintf(buf, sizeof(buf), "Chua co WiFi");
      break;
  }

  bool sel = i == editField;
  auto &c = rowCanvas;
  drawBg(c, 0, 0, ROW_X, ROW_Y0 + i * ROW_STEP, ROW_W, ROW_H);   // nen = anh nen
  if (sel) {
    c.fillRoundRect(0, 0, ROW_W, ROW_H, 6, COL_HEADER);
    c.drawRoundRect(0, 0, ROW_W, ROW_H, 6, COL_ACCENT);
  }

  c.setFont(&fonts::FreeSans9pt7b);
  c.setTextDatum(m5gfx::textdatum_t::middle_left);
  c.setTextColor(sel ? TFT_WHITE : TFT_LIGHTGREY);
  c.drawString(LABELS[i], 10, ROW_H / 2);

  c.setFont(&fonts::FreeSansBold12pt7b);
  c.setTextDatum(m5gfx::textdatum_t::middle_right);
  c.setTextColor(sel ? COL_ACCENT : (i == INFO_ROW ? TFT_GREEN : TFT_WHITE));
  c.drawString(buf, ROW_W - 10, ROW_H / 2);

  c.pushSprite(&M5.Display, ROW_X, ROW_Y0 + i * ROW_STEP);
}

static void drawRangeHeader() {
  char buf[24];
  snprintf(buf, sizeof(buf), "%02d:%02d - %02d:%02d", editSched.startH, editSched.startM,
           editSched.endH, editSched.endM);
  auto &c = hdrCanvas;
  c.fillSprite(COL_HEADER);
  c.setFont(&fonts::Font2);
  c.setTextDatum(m5gfx::textdatum_t::middle_right);
  c.setTextColor(editSched.enabled ? TFT_GREEN : COL_DIM, COL_HEADER);
  c.drawString(buf, RANGE_W - 8, HEADER_H / 2);
  c.pushSprite(&M5.Display, M5.Display.width() - RANGE_W, 0);
}

static void drawButtonLabel() {
  auto &c = btnCanvas;
  drawBg(c, 0, 0, BTN_CENTER_X[0] - BTN_LABEL_W / 2, BTN_LABEL_Y, BTN_LABEL_W, BTN_LABEL_H);
  c.setFont(&fonts::Font2);
  c.setTextDatum(m5gfx::textdatum_t::middle_center);
  c.setTextColor(COL_ACCENT);
  c.drawString(editField == SETTINGS_FIELDS - 1 ? "Luu" : "Tiep", BTN_LABEL_W / 2, BTN_LABEL_H / 2);
  c.pushSprite(&M5.Display, BTN_CENTER_X[0] - BTN_LABEL_W / 2, BTN_LABEL_Y);
}

// Lan dau (settingsFull): ve phan co dinh. Sau do chi ve lai nhung phan da doi.
static void drawSettings() {
  auto &d = M5.Display;

  if (settingsFull) {
    settingsFull = false;
    rowCanvas.setColorDepth(16);
    rowCanvas.createSprite(ROW_W, ROW_H);
    hdrCanvas.setColorDepth(16);
    hdrCanvas.createSprite(RANGE_W, HEADER_H);
    btnCanvas.setColorDepth(16);
    btnCanvas.createSprite(BTN_LABEL_W, BTN_LABEL_H);

    drawBg(d, 0, 0, 0, 0, d.width(), d.height());
    d.fillRect(0, 0, d.width(), HEADER_H, COL_HEADER);
    d.setFont(&fonts::FreeSansBold9pt7b);
    d.setTextDatum(m5gfx::textdatum_t::middle_left);
    d.setTextColor(TFT_WHITE, COL_HEADER);
    d.drawString("Cai dat gio ngu", 8, HEADER_H / 2);

    d.setFont(&fonts::Font2);
    d.setTextDatum(m5gfx::textdatum_t::middle_center);
    d.setTextColor(COL_DIM);
    d.drawString("Giu A: huy bo", d.width() / 2, 211);
    d.setTextColor(COL_ACCENT);
    d.drawString("-", BTN_CENTER_X[1], BTN_LABEL_Y + BTN_LABEL_H / 2);
    d.drawString("+", BTN_CENTER_X[2], BTN_LABEL_Y + BTN_LABEL_H / 2);

    settingsRowsDirty = (1 << (SETTINGS_FIELDS + 1)) - 1;   // 5 muc + dong dia chi web
    settingsHeaderDirty = true;
    settingsBtnDirty = true;
  }

  for (uint8_t i = 0; i <= INFO_ROW; i++) {
    if (settingsRowsDirty & (1 << i)) drawRow(i);
  }
  settingsRowsDirty = 0;
  if (settingsHeaderDirty) {
    settingsHeaderDirty = false;
    drawRangeHeader();
  }
  if (settingsBtnDirty) {
    settingsBtnDirty = false;
    drawButtonLabel();
  }
}

static void adjustField(int dir) {
  switch (editField) {
    case 0: editSched.enabled = !editSched.enabled; break;
    case 1: editSched.startH = (editSched.startH + 24 + dir) % 24; break;
    case 2: editSched.startM = (editSched.startM + 60 + 5 * dir) % 60; break;
    case 3: editSched.endH = (editSched.endH + 24 + dir) % 24; break;
    default: editSched.endM = (editSched.endM + 60 + 5 * dir) % 60; break;
  }
}

static void enterSettings() {
  inSettings = true;
  editSched = sched;
  editField = 0;
  settingsFull = true;
  lastActivityMs = millis();
}

static void exitSettings(bool save) {
  inSettings = false;
  rowCanvas.deleteSprite();
  hdrCanvas.deleteSprite();
  btnCanvas.deleteSprite();
  if (save) {
    sched = editSched;
    saveSchedule();
    Serial.printf("Lich ngu: %s %02d:%02d - %02d:%02d\n", sched.enabled ? "BAT" : "TAT", sched.startH,
                  sched.startM, sched.endH, sched.endM);
  }
  toastText = save ? "Da luu" : "Da huy";
  toastUntil = millis() + 2000;

  // Ve lai man hinh dong ho
  drawBg(M5.Display, 0, 0, 0, 0, M5.Display.width(), M5.Display.height());
  lastMinute = -1;
  lastHeader = "";
  lastSecond = 0;
  drawWeather();
}

// ---------- Anh nen tuy chinh + trang web cau hinh ----------

static WebServer server(80);
static bool webStarted = false;
static bool fsReady = false;
static uint16_t *bgCustom = nullptr;      // anh nen tuy chinh dang hien thi (PSRAM)
static uint16_t *bgUploadBuf = nullptr;   // bo dem nhan anh moi; chi doi sang khi nhan du
static size_t bgUploadLen = 0;
static bool bgUploadOk = false;
static bool bgChanged = false;            // loop() se ve lai toan bo man hinh
static char ipToast[24];

// ----- WiFi: luu cau hinh, diem phat de cai dat (AP + captive portal) -----
static DNSServer dns;
static bool everOnline = false;       // da tung vao duoc mang ke tu luc bat may / doi WiFi
static uint32_t offlineSince = 0;     // moc bat dau mat mang
static uint32_t lastStaRetry = 0;
static uint32_t wifiApplyAt = 0;      // != 0: thoi diem ap dung WiFi vua luu (sau khi tra loi trang web)
static void startWeb();

static void loadWifi() {
  if (strcmp(WIFI_SSID, "TEN_WIFI") != 0) {   // mac dinh tu config.h
    wifiSsid = WIFI_SSID;
    wifiPass = WIFI_PASS;
  }
  Preferences prefs;
  if (!prefs.begin("clock", true)) return;
  if (prefs.isKey("wSsid")) {
    wifiSsid = prefs.getString("wSsid", wifiSsid);
    wifiPass = prefs.getString("wPass", wifiPass);
  }
  prefs.end();
}

static void saveWifi() {
  Preferences prefs;
  if (!prefs.begin("clock", false)) return;
  prefs.putString("wSsid", wifiSsid);
  prefs.putString("wPass", wifiPass);
  prefs.end();
}

static void startAp() {
  WiFi.setAutoReconnect(false);   // de STA khong quet lien tuc lam AP chap chon
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);
  dns.start(53, "*", WiFi.softAPIP());   // moi ten mien deu tro ve dong ho -> dien thoai tu mo trang
  if (!webStarted) startWeb();
  apActive = true;
  lastStaRetry = millis();
  Serial.printf("AP: phat \"%s\" (mat khau %s), mo http://%s/\n", AP_SSID, AP_PASS,
                WiFi.softAPIP().toString().c_str());
  if (!overlayActive()) drawWeather();
}

static void stopAp() {
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.setAutoReconnect(true);
  apActive = false;
  Serial.println("AP: tat");
  if (!overlayActive()) drawWeather();
}

static String jsonEscape(const String &s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
  return out;
}

static void handleWifiScan() {
  int n = WiFi.scanNetworks();
  String out = "[";
  int count = 0;
  for (int i = 0; i < n && count < 20; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    if (count++) out += ',';
    out += "{\"ssid\":\"" + jsonEscape(ssid) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"lock\":" +
           (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
  }
  out += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", out);
}

static void handleWifiSave() {
  String ssid = server.arg("ssid"), pass = server.arg("pass");
  ssid.trim();
  if (ssid.length() == 0 || ssid.length() > 32 || (pass.length() > 0 && pass.length() < 8) || pass.length() > 63) {
    server.send(400, "text/plain; charset=utf-8",
                "Tên WiFi phải có 1-32 ký tự; mật khẩu để trống hoặc 8-63 ký tự");
    return;
  }
  wifiSsid = ssid;
  wifiPass = pass;
  saveWifi();
  // Tra loi xong moi doi mang. Cho 3 giay de phan hoi kip toi dien thoai (ke ca khi phai gui lai);
  // ngat som hon thi trang web co luc khong nhan duoc cau tra loi du da luu.
  wifiApplyAt = millis() + 3000;
  Serial.printf("Web: WiFi moi \"%s\"\n", wifiSsid.c_str());
  server.send(200, "text/plain; charset=utf-8",
              "Đã lưu. Đồng hồ đang kết nối WiFi mới; nếu thành công, địa chỉ mới sẽ hiện trên màn hình "
              "đồng hồ. Nếu sau khoảng 30 giây vẫn không vào được, đồng hồ sẽ phát lại WiFi M5Clock-Setup.");
}

static uint16_t *allocBgBuffer() {
  void *p = ps_malloc(BG_BYTES);
  if (!p) p = malloc(BG_BYTES);
  return (uint16_t *)p;
}

// Nap anh nen da luu (neu co) luc khoi dong
static void loadCustomBg() {
  fsReady = LittleFS.begin(true);   // true: tu dinh dang o lan dau
  if (!fsReady) {
    Serial.println("LittleFS: khong mo duoc, chi dung anh nen mac dinh");
    return;
  }
  if (!LittleFS.exists(BG_FILE)) return;   // chua tung gui anh: dung anh mac dinh
  File f = LittleFS.open(BG_FILE, "r");
  if (!f) return;
  if (f.size() == BG_BYTES) {
    uint16_t *buf = allocBgBuffer();
    if (buf && f.read((uint8_t *)buf, BG_BYTES) == BG_BYTES) {
      bgCustom = buf;
      bgData = buf;
      Serial.println("Anh nen: dung anh tuy chinh da luu");
    } else {
      free(buf);
    }
  }
  f.close();
}

static void saveChime() {
  Preferences prefs;
  if (!prefs.begin("clock", false)) return;
  prefs.putBool("chOn", chimeOn);
  prefs.putUChar("chVol", chimeVol);
  prefs.putBool("chQuiet", chimeQuiet);
  prefs.end();
}

// Phat am thanh bao gio (mac dinh hoac tai len). Loa dung chung I2S voi micro nen khong phat o man hinh nhac.
static bool playChime() {
  if (inMusic) return false;
  if (!M5.Speaker.isEnabled()) M5.Speaker.begin();   // sau khi ra khoi man hinh nhac loa dang tat
  M5.Speaker.setVolume((uint8_t)(chimeVol * 255 / 100));
  if (chimeLen > 0) {
    M5.Speaker.playRaw(chimeData, chimeLen, CHIME_RATE, false, 1, 0, true);   // bat dau ngay, khong chan loop()
  } else {   // du phong: ba not len cao dan
    static const struct { uint16_t freq, ms; } SEQ[] = {{880, 170}, {1175, 170}, {1568, 520}};
    for (const auto &n : SEQ) M5.Speaker.tone(n.freq, n.ms, 0, false);
  }
  return true;
}

static int16_t *allocChimeBuffer() {
  void *p = ps_malloc(CHIME_MAX_SAMPLES * 2);
  if (!p) p = malloc(CHIME_MAX_SAMPLES * 2);
  return (int16_t *)p;
}

// Nap am thanh da luu (neu co) luc khoi dong. Can LittleFS da mo (loadCustomBg).
static void loadCustomChime() {
  if (!LittleFS.exists(CHIME_FILE)) return;
  File f = LittleFS.open(CHIME_FILE, "r");
  if (!f) return;
  size_t sz = f.size();
  if (sz >= 3200 && sz <= CHIME_MAX_SAMPLES * 2 && sz % 2 == 0) {
    int16_t *buf = allocChimeBuffer();
    if (buf && f.read((uint8_t *)buf, sz) == sz) {
      chimeCustom = buf;
      chimeData = buf;
      chimeLen = sz / 2;
      Serial.printf("Chuong: dung am thanh tuy chinh (%.1f giay)\n", chimeLen / (float)CHIME_RATE);
    } else {
      free(buf);
    }
  }
  f.close();
}

// Nhan am thanh tu trang web (multipart, du lieu tho int16 mono 16 kHz). Ghi vao bo dem rieng de
// am thanh dang dung khong hong neu viec gui bi dut giua chung.
static void handleChimeUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    chimeUploadLen = 0;
    if (!chimeUploadBuf) chimeUploadBuf = allocChimeBuffer();
    chimeUploadOk = chimeUploadBuf != nullptr;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (chimeUploadOk && chimeUploadLen + up.currentSize <= CHIME_MAX_SAMPLES * 2) {
      memcpy((uint8_t *)chimeUploadBuf + chimeUploadLen, up.buf, up.currentSize);
      chimeUploadLen += up.currentSize;
    } else {
      chimeUploadOk = false;   // qua 10 giay hoac het bo nho
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    chimeUploadOk = false;
  }
}

static void handleChimeUploadDone() {
  if (!chimeUploadOk || chimeUploadLen < 3200 || chimeUploadLen % 2 != 0) {
    Serial.printf("Web: am thanh loi (%u byte)\n", (unsigned)chimeUploadLen);
    server.send(400, "text/plain; charset=utf-8",
                "Âm thanh không hợp lệ: cần từ 0,1 đến 10 giây (mono, 16 kHz, 16 bit)");
    return;
  }
  M5.Speaker.stop();   // dang phat bo dem cu thi dung truoc khi doi

  bool saved = false;
  if (fsReady) {
    File f = LittleFS.open(CHIME_FILE, "w");
    if (f) {
      saved = f.write((const uint8_t *)chimeUploadBuf, chimeUploadLen) == chimeUploadLen;
      f.close();
    }
  }

  int16_t *old = chimeCustom;   // doi bo dem: am thanh moi thanh am thanh dang dung
  chimeCustom = chimeUploadBuf;
  chimeUploadBuf = old;
  chimeData = chimeCustom;
  chimeLen = chimeUploadLen / 2;
  float sec = chimeLen / (float)CHIME_RATE;
  Serial.printf("Web: am thanh bao gio moi %.2f giay, %s\n", sec, saved ? "da luu" : "KHONG luu duoc");
  char msg[120];
  snprintf(msg, sizeof(msg), saved ? "Đã đổi âm thanh (%.1f giây) và lưu vào đồng hồ."
                                   : "Đã đổi âm thanh (%.1f giây), nhưng không lưu được (mất khi tắt máy).", (double)sec);
  server.send(200, "text/plain; charset=utf-8", msg);
}

static void handleChimeReset() {
  M5.Speaker.stop();
  if (fsReady) LittleFS.remove(CHIME_FILE);
  chimeData = CHIME_DEFAULT;
  chimeLen = CHIME_DEFAULT_LEN;
  if (chimeCustom) {
    if (!chimeUploadBuf) chimeUploadBuf = chimeCustom; else free(chimeCustom);
    chimeCustom = nullptr;
  }
  server.send(200, "text/plain; charset=utf-8", "Đã quay về âm thanh mặc định.");
}

// Goi moi giay voi gio hien tai; phat chuong khi vua sang gio moi
static void checkHourChime(const struct tm &t) {
  if (!timeValid(t)) return;
  if (lastChimeHour >= 0 && t.tm_hour != lastChimeHour && t.tm_min < 2) {   // <2 phut: tranh keu nham khi NTP vua chinh gio
    bool mute = !chimeOn || chimeVol == 0 || (chimeQuiet && sleeping);
    Serial.printf("Gio moi %02d:00 - chuong %s\n", t.tm_hour, mute ? "(im)" : "keu");
    if (!mute && playChime()) {
      chimeCount++;
      snprintf(chimeLastAt, sizeof(chimeLastAt), "%02d:%02d", t.tm_hour, t.tm_min);
    }
  }
  lastChimeHour = t.tm_hour;
}

static void handleChime() {
  if (server.hasArg("on")) chimeOn = server.arg("on").toInt() != 0;
  if (server.hasArg("quiet")) chimeQuiet = server.arg("quiet").toInt() != 0;
  if (server.hasArg("vol")) {
    long v = server.arg("vol").toInt();
    if (v < 0 || v > 100) {
      server.send(400, "text/plain; charset=utf-8", "Âm lượng từ 0 đến 100");
      return;
    }
    chimeVol = (uint8_t)v;
  }
  saveChime();
  server.send(200, "text/plain; charset=utf-8", "OK");
}

static void handleState() {
  char json[1400];
  int n = snprintf(json, sizeof(json),
                   "{\"enabled\":%s,\"start\":\"%02d:%02d\",\"end\":\"%02d:%02d\",\"idle\":%u,"
                   "\"sleeping\":%s,\"customBg\":%s,\"colors\":{",
                   sched.enabled ? "true" : "false", sched.startH, sched.startM, sched.endH, sched.endM,
                   (unsigned)sched.idleSec, sleeping ? "true" : "false", bgCustom ? "true" : "false");
  for (uint8_t i = 0; i < C_COUNT; i++) {
    n += snprintf(json + n, sizeof(json) - n, "%s\"%s\":\"#%06x\"", i ? "," : "", COLOR_KEYS[i],
                  (unsigned)colorRgb[i]);
  }
  bool connected = WiFi.status() == WL_CONNECTED;
  snprintf(json + n, sizeof(json) - n,
           "},\"wifi\":{\"ssid\":\"%s\",\"connected\":%s,\"ip\":\"%s\",\"ap\":%s},"
           "\"music\":{\"on\":%s,\"style\":%u},"
           "\"loc\":{\"auto\":%s,\"name\":\"%s\",\"lat\":%.4f,\"lon\":%.4f},\"dateMode\":%u,"
           "\"chime\":{\"on\":%s,\"vol\":%u,\"quiet\":%s,\"count\":%u,\"last\":\"%s\","
           "\"custom\":%s,\"ms\":%u}}",
           jsonEscape(wifiSsid).c_str(), connected ? "true" : "false",
           connected ? WiFi.localIP().toString().c_str() : "", apActive ? "true" : "false",
           inMusic ? "true" : "false", (unsigned)musicStyle, locAuto ? "true" : "false",
           jsonEscape(locName).c_str(), (double)locLat, (double)locLon, (unsigned)dateMode,
           chimeOn ? "true" : "false", (unsigned)chimeVol, chimeQuiet ? "true" : "false",
           (unsigned)chimeCount, chimeLastAt, chimeCustom ? "true" : "false",
           (unsigned)(chimeLen * 1000ULL / CHIME_RATE));
  server.send(200, "application/json", json);
}

// auto=1: bat tu dong, dò lai ngay. auto=0 kem name, lat, lon: dat tay.
static void handleLocation() {
  if (server.arg("auto").toInt() != 0) {
    locAuto = true;
    nextLocAt = 0;   // loop() tra cuu ngay
    saveLocation();
    server.send(200, "text/plain; charset=utf-8", "Đang dò vị trí theo mạng...");
    return;
  }

  String name = server.arg("name");
  name.trim();
  char *e1 = nullptr, *e2 = nullptr;
  float lat = strtof(server.arg("lat").c_str(), &e1);
  float lon = strtof(server.arg("lon").c_str(), &e2);
  if (name.length() < 1 || name.length() > 40 || server.arg("lat").isEmpty() || *e1 != '\0' ||
      server.arg("lon").isEmpty() || *e2 != '\0' || lat < -90 || lat > 90 || lon < -180 || lon > 180) {
    server.send(400, "text/plain; charset=utf-8",
                "Cần tên (1-40 ký tự), vĩ độ từ -90 đến 90 và kinh độ từ -180 đến 180");
    return;
  }
  locAuto = false;
  locName = name;
  locLat = lat;
  locLon = lon;
  saveLocation();
  nextWeatherAt = 0;   // tai lai thoi tiet cho noi moi
  Serial.printf("Web: khu vuc nhap tay \"%s\" %.4f,%.4f\n", locName.c_str(), (double)lat, (double)lon);
  server.send(200, "text/plain; charset=utf-8", "OK");
}

static void saveColors() {
  Preferences prefs;
  if (!prefs.begin("clock", false)) return;
  for (uint8_t i = 0; i < C_COUNT; i++) {
    char key[8];
    snprintf(key, sizeof(key), "col%u", i);
    prefs.putUInt(key, colorRgb[i]);
  }
  prefs.end();
}

// Nhan mau dang "#rrggbb" cho tung noi dung; muc nao khong gui thi giu nguyen
static void handleColors() {
  uint32_t next[C_COUNT];
  memcpy(next, colorRgb, sizeof(next));
  for (uint8_t i = 0; i < C_COUNT; i++) {
    if (!server.hasArg(COLOR_KEYS[i])) continue;
    String v = server.arg(COLOR_KEYS[i]);
    char *end = nullptr;
    unsigned long rgb = strtoul(v.c_str() + (v.startsWith("#") ? 1 : 0), &end, 16);
    if (v.length() != (v.startsWith("#") ? 7u : 6u) || *end != '\0') {
      server.send(400, "text/plain; charset=utf-8", "Màu không hợp lệ (cần dạng #rrggbb)");
      return;
    }
    next[i] = rgb & 0xFFFFFF;
  }
  memcpy(colorRgb, next, sizeof(next));
  saveColors();
  bgChanged = true;   // ve lai toan bo man hinh voi mau moi
  server.send(200, "text/plain; charset=utf-8", "OK");
}

static void handleColorsReset() {
  memcpy(colorRgb, COLOR_DEFAULTS, sizeof(colorRgb));
  saveColors();
  bgChanged = true;
  server.send(200, "text/plain; charset=utf-8", "OK");
}

static bool parseHm(const String &s, uint8_t &h, uint8_t &m) {
  int hh, mm;
  if (sscanf(s.c_str(), "%d:%d", &hh, &mm) != 2 || hh < 0 || hh > 23 || mm < 0 || mm > 59) return false;
  h = hh;
  m = mm;
  return true;
}

static void handleSleepConfig() {
  SleepSchedule s = sched;
  s.enabled = server.arg("enabled").toInt() != 0;
  long idle = server.arg("idle").toInt();
  if (!parseHm(server.arg("start"), s.startH, s.startM) || !parseHm(server.arg("end"), s.endH, s.endM) ||
      idle < 5 || idle > 3600) {
    server.send(400, "text/plain; charset=utf-8", "Giá trị không hợp lệ");
    return;
  }
  s.idleSec = (uint16_t)idle;
  sched = s;
  saveSchedule();
  lastActivityMs = millis();   // khong ngu ngay lap tuc neu dang o trong khoang gio moi
  Serial.printf("Web: lich ngu %s %02d:%02d - %02d:%02d, idle %us\n", sched.enabled ? "BAT" : "TAT",
                sched.startH, sched.startM, sched.endH, sched.endM, (unsigned)sched.idleSec);
  server.send(200, "text/plain; charset=utf-8", "OK");
}

// Nhan tung khuc du lieu anh (multipart). Ghi vao bo dem rieng de anh dang hien thi khong bi hong
// neu viec gui bi dut giua chung.
static void handleBgUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    bgUploadLen = 0;
    if (!bgUploadBuf) bgUploadBuf = allocBgBuffer();
    bgUploadOk = bgUploadBuf != nullptr;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (bgUploadOk && bgUploadLen + up.currentSize <= BG_BYTES) {
      memcpy((uint8_t *)bgUploadBuf + bgUploadLen, up.buf, up.currentSize);
      bgUploadLen += up.currentSize;
    } else {
      bgUploadOk = false;
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    bgUploadOk = false;
  }
}

static void handleBgDone() {
  if (!bgUploadOk || bgUploadLen != BG_BYTES) {
    Serial.printf("Web: anh nen loi (%u/%u byte)\n", (unsigned)bgUploadLen, (unsigned)BG_BYTES);
    server.send(400, "text/plain; charset=utf-8", "Dữ liệu ảnh không đúng kích thước (cần 320x240 RGB565)");
    return;
  }

  bool saved = false;
  if (fsReady) {
    File f = LittleFS.open(BG_FILE, "w");
    if (f) {
      saved = f.write((const uint8_t *)bgUploadBuf, BG_BYTES) == BG_BYTES;
      f.close();
    }
  }

  // Doi bo dem: anh moi thanh anh dang dung, bo dem cu de danh cho lan gui sau
  uint16_t *old = bgCustom;
  bgCustom = bgUploadBuf;
  bgUploadBuf = old;
  bgData = bgCustom;
  bgChanged = true;
  Serial.printf("Web: anh nen moi, %s\n", saved ? "da luu" : "KHONG luu duoc");
  server.send(200, "text/plain; charset=utf-8",
              saved ? "Đã đổi ảnh nền và lưu vào đồng hồ."
                    : "Đã đổi ảnh nền, nhưng không lưu được (sẽ mất khi tắt máy).");
}

static void handleBgReset() {
  if (fsReady) LittleFS.remove(BG_FILE);
  bgData = BG_IMAGE;
  if (bgCustom) {
    if (!bgUploadBuf) bgUploadBuf = bgCustom; else free(bgCustom);
    bgCustom = nullptr;
  }
  bgChanged = true;
  server.send(200, "text/plain; charset=utf-8", "Đã quay về ảnh nền mặc định.");
}

// ---------- Man hinh nhay theo nhac ----------
//   Vao: vuot tu phai sang trai tren man hinh dong ho.  Ve: vuot tu trai sang phai.
//   Micro PDM tich hop (SPM1423) -> FFT 256 diem o 16 kHz -> 16 dai tan (125 Hz .. 7 kHz).
//   Cot tren man hinh, thanh LED va vien man hinh nhay theo nhac.

static const int MUSIC_N = 256;              // so mau moi khung (16 ms)
static const uint32_t MUSIC_RATE = 16000;
static const int BARS = 16, SEGS = 24;       // 16 cot, moi cot 24 nac
static const int BAR_X0 = 8, BAR_PITCH = 19, BAR_W = 15;
static const int SEG_PITCH = 8, SEG_H = 6;
static const int BAR_BOTTOM = 232;
static const int MUSIC_TOP = 28, BORDER = 4; // vien loe sang quanh vung cot
static const float AGC_FLOOR = 3000.0f;      // duoi muc nay coi nhu im lang (khong khuech dai tap am)

static int16_t micBuf[2][MUSIC_N];
static uint8_t micIdx = 0;
static float fftRe[MUSIC_N], fftIm[MUSIC_N], hannWin[MUSIC_N];
static uint8_t bandEdge[BARS + 1];
static uint16_t segColor[SEGS];
static float barLevel[BARS];                 // 0..1, len nhanh xuong cham
static uint8_t barShown[BARS];               // so nac dang ve tren man hinh
static float noiseFloor[BARS];               // tieng on nen uoc luong cho tung dai (quat, may lanh...)
static float bandSmooth[BARS];               // do lon tung dai da lam muot, de uoc luong tieng on nen
static float musicAgc = AGC_FLOOR, bassAvg = 0, beatLevel = 0;
static bool musicTablesReady = false;
static bool musicFull = false;               // ve lai toan bo man hinh nhac
static float musicPeakLog = 0;               // de in muc tin hieu ra Serial moi giay
static int musicPeakBar = 0;
static uint32_t musicLogAt = 0;

// FFT co so 2 tai cho (re/im), n la luy thua cua 2
static void fft(float *re, float *im, int n) {
  for (int i = 1, j = 0; i < n; i++) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      std::swap(re[i], re[j]);
      std::swap(im[i], im[j]);
    }
  }
  for (int len = 2; len <= n; len <<= 1) {
    float ang = -2.0f * PI / len, wr = cosf(ang), wi = sinf(ang);
    int half = len >> 1;
    for (int i = 0; i < n; i += len) {
      float cr = 1, ci = 0;
      for (int k = 0; k < half; k++) {
        int a = i + k, b = a + half;
        float vr = re[b] * cr - im[b] * ci, vi = re[b] * ci + im[b] * cr;
        re[b] = re[a] - vr;
        im[b] = im[a] - vi;
        re[a] += vr;
        im[a] += vi;
        float ncr = cr * wr - ci * wi;
        ci = cr * wi + ci * wr;
        cr = ncr;
      }
    }
  }
}

static void initMusicTables() {
  for (int i = 0; i < MUSIC_N; i++) hannWin[i] = 0.5f - 0.5f * cosf(2.0f * PI * i / (MUSIC_N - 1));
  // Bien cac dai tan chia theo thang logarit: bin 2 (125 Hz) -> bin 112 (7 kHz)
  for (int b = 0; b <= BARS; b++) {
    int e = (int)lroundf(2.0f * powf(56.0f, (float)b / BARS));
    if (b > 0 && e <= bandEdge[b - 1]) e = bandEdge[b - 1] + 1;
    bandEdge[b] = e;
  }
  // Mau nac: xanh la o duoi -> vang -> do o tren
  for (int s = 0; s < SEGS; s++) {
    uint32_t rgb = leds.ColorHSV((uint16_t)((120 - 120 * s / (SEGS - 1)) * 65536L / 360), 255, 255);
    segColor[s] = ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F);
  }
  musicTablesReady = true;
}

// Cac kieu song nhac. Doi bang nut B (khi dang o man hinh nhac) hoac tu trang web; luu NVS.
enum MusicStyle : uint8_t {
  MS_BARS, MS_ORB, MS_MIRROR, MS_RADIAL, MS_WATERFALL, MS_ICON, MS_HEARTS, MS_COUNT
};
static const char *MUSIC_STYLE_NAMES[MS_COUNT] = {"Cột", "Quả cầu", "Đối xứng", "Vòng tròn", "Thác phổ",
                                                  "Phi hành gia", "Tim bay"};
static uint32_t musicStartMs = 0;   // luc vao man hinh nhac, de dem thoi gian
static_assert(MS_COUNT == MUSIC_STYLE_COUNT, "cap nhat MUSIC_STYLE_COUNT khi them kieu song");

// Vung ve ben trong vien. Kieu "Cot" ve truc tiep phan thay doi; cac kieu khac ve lai ca khung
// vao sprite (PSRAM) roi day len mot lan de khong nhap nhay.
static const int MC_X = BORDER, MC_Y = MUSIC_TOP + BORDER;
static const int MC_W = 320 - 2 * BORDER, MC_H = 240 - MUSIC_TOP - 2 * BORDER;   // 312 x 204
static M5Canvas musicCanvas;
static bool musicCanvasReady = false;
static float wavePeak = 400;   // bien do gan day cua dang song, de tu co gian chieu cao

static uint16_t hsv565(uint16_t hue, uint8_t sat, uint8_t val) {
  uint32_t rgb = leds.ColorHSV(hue, sat, val);
  return ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F);
}

static bool ensureMusicCanvas() {
  if (!musicCanvasReady) {
    musicCanvas.setPsram(true);
    musicCanvas.setColorDepth(16);
    musicCanvasReady = musicCanvas.createSprite(MC_W, MC_H) != nullptr;
    if (musicCanvasReady) musicCanvas.fillSprite(TFT_BLACK);
  }
  return musicCanvasReady;
}

static void drawMusicStatic() {
  auto &d = M5.Display;
  d.fillScreen(TFT_BLACK);
  d.fillRect(0, 0, d.width(), HEADER_H, COL_HEADER);
  d.loadFont(VI_B18);
  d.setTextDatum(m5gfx::textdatum_t::middle_left);
  d.setTextColor(TFT_WHITE, COL_HEADER);
  d.drawString("Nhạc", 8, HEADER_H / 2);
  d.unloadFont();
  char title[48];
  snprintf(title, sizeof(title), "%s  |  B: đổi kiểu", MUSIC_STYLE_NAMES[musicStyle]);
  d.loadFont(VI_R14);
  d.setTextDatum(m5gfx::textdatum_t::middle_center);
  d.setTextColor(TFT_LIGHTGREY, COL_HEADER);
  d.drawString(title, 156, HEADER_H / 2);
  d.unloadFont();
  memset(barShown, 0, sizeof(barShown));
  if (musicCanvasReady) musicCanvas.fillSprite(TFT_BLACK);
}

static void setMusicStyle(uint8_t style) {
  musicStyle = style % MS_COUNT;
  Preferences prefs;
  if (prefs.begin("clock", false)) {
    prefs.putUChar("muStyle", musicStyle);
    prefs.end();
  }
  musicFull = true;   // xoa vung ve va ve lai tieu de voi ten kieu moi
  Serial.printf("Nhac: kieu %u\n", (unsigned)musicStyle);
}

// ----- Cac kieu ve vao sprite (g rong MC_W x MC_H) -----

// Qua cau: qua cau phat sang, ben trong la cac lop mau long cuon song (tim / hong / xanh lo /
// xanh ngoc), ngoai la vien sang va quang hao quang. Bass lam cau phong to va lop duoi dang cao,
// am trung va am cao lam cac lop tren gon song; hao quang loe ra theo tung dai tan.
static void renderOrb(M5Canvas &g, uint32_t nowMs) {
  g.fillSprite(g.color565(8, 4, 30));   // nen xanh tim than nhu trong hinh mau
  const int cx = MC_W / 2, cy = MC_H / 2;
  float bass = 0, mid = 0, tre = 0;
  for (int b = 0; b < BARS; b++) (b < 4 ? bass : b < 10 ? mid : tre) += barLevel[b];
  bass /= 4;
  mid /= 6;
  tre /= 6;
  float lvl = min(1.0f, (bass + mid + tre) / 1.5f + beatLevel * 0.5f);
  float t = nowMs * 0.001f;
  const int R = 56 + (int)lroundf(min(1.0f, bass) * 6 + beatLevel * 6);   // 56..68

  // Quang sang: cac dia tron long nhau, sang dan vao trong
  for (int i = 8; i >= 1; i--) {
    float k = 1.0f - i / 9.0f;
    k = k * k * (0.45f + 0.55f * lvl);
    // Cong them vao mau nen (8,4,30) de quang sang khong bao gio toi hon nen
    g.fillCircle(cx, cy, R + i * 2,
                 g.color565((uint8_t)(8 + 120 * k), (uint8_t)(4 + 50 * k), (uint8_t)(30 + 225 * k)));
  }

  // Hao quang: duong vien mem bao quanh, phinh ra theo tung dai tan (doi xung trai / phai)
  const int pts = 60;
  for (int pass = 0; pass < 2; pass++) {
    uint16_t c = pass ? g.color565(90, 220, 255) : g.color565(190, 120, 255);
    int px = 0, py = 0;
    for (int i = 0; i <= pts; i++) {
      int j = i % pts;
      int b = (j < pts / 2 ? j : pts - 1 - j) * BARS / (pts / 2);
      float a = j * 2.0f * PI / pts - PI / 2;
      float r = R + 5 + pass * 3 + barLevel[b] * (20 - pass * 8) + 2.5f * sinf(j * 0.9f + t * (pass ? -2.3f : 1.9f));
      int x = cx + (int)lroundf(cosf(a) * r), y = cy + (int)lroundf(sinf(a) * r);
      if (i) g.drawLine(px, py, x, y, c);
      px = x;
      py = y;
    }
  }

  // Ben trong cau: ve tung cot doc, chia thanh 5 lop boi 4 duong song
  static const uint8_t LAYER[5][3] = {{96, 60, 220}, {214, 70, 226}, {40, 190, 255}, {0, 214, 160}, {70, 110, 255}};
  static const float OFF[4] = {-0.45f, -0.08f, 0.22f, 0.52f};   // vi tri duong song (theo ban kinh)
  static const float FRQ[4] = {2.4f, 3.1f, 2.0f, 3.6f}, SPD[4] = {1.3f, -1.7f, 2.1f, -1.1f};
  const float drive[4] = {tre, mid, mid, bass};
  for (int dx = -R; dx <= R; dx++) {
    int h = (int)sqrtf((float)(R * R - dx * dx));
    int top = cy - h, bot = cy + h;
    float u = dx / (float)R;
    float shade = 0.55f + 0.45f * h / R;   // toi dan ve hai mep cho ra dang khoi cau
    uint16_t col[5];
    for (int k = 0; k < 5; k++) {
      col[k] = g.color565((uint8_t)(LAYER[k][0] * shade), (uint8_t)(LAYER[k][1] * shade),
                          (uint8_t)(LAYER[k][2] * shade));
    }
    int prev = top;
    for (int k = 0; k < 4; k++) {
      float amp = 0.08f + 0.34f * min(1.0f, drive[k]);
      float y = cy + R * (OFF[k] + amp * sinf(u * FRQ[k] + t * SPD[k] + k * 1.7f) +
                          0.05f * sinf(u * 7.0f + t * 3.0f + k));
      int yk = constrain((int)y, prev, bot);
      if (yk > prev) g.drawFastVLine(cx + dx, prev, yk - prev, col[k]);
      prev = yk;
    }
    if (bot >= prev) g.drawFastVLine(cx + dx, prev, bot - prev + 1, col[4]);
  }

  // Vien kinh va vet sang phia tren ben trai
  g.drawCircle(cx, cy, R, g.color565(190, 230, 255));
  g.drawCircle(cx, cy, R - 1, g.color565(120, 170, 255));
  g.fillArc(cx, cy, R - 6, R - 9, 205, 250, g.color565(235, 245, 255));
}

// Doi xung: cot moc ra ca hai phia tu duong giua
static void renderMirror(M5Canvas &g) {
  g.fillSprite(TFT_BLACK);
  const int half = SEGS / 2, mid = MC_H / 2;
  for (int b = 0; b < BARS; b++) {
    int n = min(half, (int)(barLevel[b] * half + 0.5f));
    int x = BAR_X0 - MC_X + b * BAR_PITCH;
    for (int s = 0; s < n; s++) {
      uint16_t c = segColor[s * 2];
      g.fillRect(x, mid - (s + 1) * SEG_PITCH + 1, BAR_W, SEG_H, c);
      g.fillRect(x, mid + s * SEG_PITCH + 1, BAR_W, SEG_H, c);
    }
  }
}

// Vong tron: 32 tia quanh tam (16 dai tan, soi guong hai nua), tam phap phong theo nhip
static void renderRadial(M5Canvas &g, uint32_t nowMs) {
  g.fillSprite(TFT_BLACK);
  const int cx = MC_W / 2, cy = MC_H / 2, rays = BARS * 2;
  float r0 = 26 + beatLevel * 8;
  for (int i = 0; i < rays; i++) {
    int b = i < BARS ? i : rays - 1 - i;
    float a = (i + 0.5f) * 2.0f * PI / rays - PI / 2;
    float len = 4 + barLevel[b] * 62;
    float ca = cosf(a), sa = sinf(a);
    int x0 = cx + (int)lroundf(ca * r0), y0 = cy + (int)lroundf(sa * r0);
    int x1 = cx + (int)lroundf(ca * (r0 + len)), y1 = cy + (int)lroundf(sa * (r0 + len));
    uint16_t c = hsv565((uint16_t)(i * 65536L / rays + nowMs * 8), 255, 255);
    g.drawLine(x0, y0, x1, y1, c);
    g.drawLine(x0 + 1, y0, x1 + 1, y1, c);
    g.drawLine(x0, y0 + 1, x1, y1 + 1, c);
  }
  g.fillCircle(cx, cy, (int)r0 - 6, hsv565((uint16_t)(nowMs * 12), 255, (uint8_t)(50 + 205 * beatLevel)));
}

// Phi hanh gia: bieu tuong o giua nhun theo nhip bass, quanh no la vong tia song theo 16 dai tan,
// hai ben la hai dai song nho chay theo am thanh.
static void renderIcon(M5Canvas &g, const int16_t *data, float mean, uint32_t nowMs) {
  g.fillSprite(TFT_BLACK);
  const int cx = MC_W / 2, cy = MC_H / 2, rays = BARS * 2;
  int bounce = (int)lroundf(beatLevel * 7);   // nhay len khi co nhip

  // Song hai ben: nua dau cua khung mau ve ben trai, nua sau ve ben phai
  float scale = 26.0f / wavePeak;
  float pk = 1;
  for (int i = 0; i < MUSIC_N; i++) pk = max(pk, fabsf(data[i] - mean));
  wavePeak = max(wavePeak * 0.97f, pk);
  if (wavePeak < 400) wavePeak = 400;
  uint16_t wc = hsv565((uint16_t)(nowMs * 12 + 32768), 180, 200);
  const int sideW = 44;
  for (int side = 0; side < 2; side++) {
    int xBase = side ? MC_W - 4 - sideW : 4;
    int px = xBase, py = cy;
    for (int i = 0; i <= sideW; i += 2) {
      int idx = (side ? MUSIC_N / 2 : 0) + i * (MUSIC_N / 2 - 1) / sideW;
      int y = constrain(cy - (int)((data[idx] - mean) * scale), cy - 30, cy + 30);
      g.drawLine(px, py, xBase + i, y, wc);
      px = xBase + i;
      py = y;
    }
  }

  g.pushImage(cx - MUSIC_ICON_W / 2, cy - MUSIC_ICON_H / 2 - bounce + 3, MUSIC_ICON_W, MUSIC_ICON_H,
              (const lgfx::rgb565_t *)MUSIC_ICON);

  // Vong tia ve sau bieu tuong de khong bi goc den cua anh che mat
  const float r0 = 62;
  for (int i = 0; i < rays; i++) {
    int b = i < BARS ? i : rays - 1 - i;
    float a = (i + 0.5f) * 2.0f * PI / rays - PI / 2;
    float len = 3 + barLevel[b] * 36;
    float ca = cosf(a), sa = sinf(a);
    int x0 = cx + (int)lroundf(ca * r0), y0 = cy + (int)lroundf(sa * r0);
    int x1 = cx + (int)lroundf(ca * (r0 + len)), y1 = cy + (int)lroundf(sa * (r0 + len));
    uint16_t c = hsv565((uint16_t)(i * 65536L / rays + nowMs * 8), 255, 255);
    g.drawLine(x0, y0, x1, y1, c);
    g.drawLine(x0 + 1, y0, x1 + 1, y1, c);
    g.drawLine(x0, y0 + 1, x1, y1 + 1, c);
  }
}

// Tim bay: nen den day sao lap lanh, hang cham sang o duoi voi cac vach manh xanh lo vot len doi
// xung hai ben (bass o hai dau, am cao o giua), trai tim hong bay len nhanh cham theo am luong.
static void renderHearts(M5Canvas &g, uint32_t nowMs) {
  static const int STARS = 46, HEARTS = 12;
  static int16_t starX[STARS], starY[STARS];
  static uint8_t starPhase[STARS];
  struct Heart { float x, y, speed, size, phase; };
  static Heart hearts[HEARTS];
  static bool ready = false;
  auto spawn = [](Heart &h, bool anywhere) {
    h.x = random(10, MC_W - 10);
    h.y = anywhere ? random(0, MC_H) : MC_H + random(4, 30);
    h.speed = 0.35f + random(0, 100) / 110.0f;
    h.size = 2.0f + random(0, 100) / 28.0f;     // ban kinh moi thuy tim: 2 .. 5.5
    h.phase = random(0, 628) / 100.0f;
  };
  if (!ready) {
    for (int i = 0; i < STARS; i++) {
      starX[i] = random(2, MC_W - 2);
      starY[i] = random(2, MC_H - 34);
      starPhase[i] = random(0, 256);
    }
    for (auto &h : hearts) spawn(h, true);
    ready = true;
  }

  g.fillSprite(TFT_BLACK);
  float t = nowMs * 0.001f;
  float lvl = min(1.0f, musicLevel / 160.0f);

  // Sao: moi ngoi nhap nhay theo nhip rieng
  for (int i = 0; i < STARS; i++) {
    float k = 0.5f + 0.5f * sinf(t * (1.2f + (starPhase[i] & 7) * 0.35f) + starPhase[i]);
    uint8_t v = (uint8_t)(40 + 200 * k * k);
    g.drawPixel(starX[i], starY[i], g.color565(v, v, v));
    if ((starPhase[i] & 3) == 0 && k > 0.8f) {   // vai ngoi sao lon hon co tia chu thap
      g.drawPixel(starX[i] - 1, starY[i], g.color565(v / 2, v / 2, v / 2));
      g.drawPixel(starX[i] + 1, starY[i], g.color565(v / 2, v / 2, v / 2));
      g.drawPixel(starX[i], starY[i] - 1, g.color565(v / 2, v / 2, v / 2));
      g.drawPixel(starX[i], starY[i] + 1, g.color565(v / 2, v / 2, v / 2));
    }
  }

  // Trai tim: bay len, lac nhe; nhac cang lon bay cang nhanh, co nhip thi phong to mot chut
  for (auto &h : hearts) {
    h.y -= h.speed * (0.5f + 2.8f * lvl + beatLevel);
    h.x += sinf(t * 1.6f + h.phase) * 0.45f;
    if (h.y < -12) spawn(h, false);
    float r = h.size * (1.0f + 0.25f * beatLevel);
    int x = (int)h.x, y = (int)h.y, ri = max(1, (int)lroundf(r));
    uint16_t c = g.color565(235, 40, 255);
    g.fillCircle(x - ri, y, ri, c);
    g.fillCircle(x + ri, y, ri, c);
    g.fillTriangle(x - 2 * ri, y + ri / 2, x + 2 * ri, y + ri / 2, x, y + ri * 5 / 2, c);
    if (ri >= 3) g.fillCircle(x - ri, y - 1, 1, g.color565(255, 190, 255));   // vet bong
  }

  // Hang cham + vach: 36 vi tri, soi guong qua giua
  const int n = 36, pitch = 6, baseY = MC_H - 22;
  const int x0 = (MC_W - (n - 1) * pitch) / 2;
  for (int i = 0; i < n; i++) {
    int d = i < n / 2 ? i : n - 1 - i;              // 0 o hai dau .. 17 o giua
    int b = d * BARS / (n / 2);
    int x = x0 + i * pitch;
    int h = (int)lroundf(min(1.0f, barLevel[b]) * 120);
    float mix = d / (float)(n / 2 - 1);             // xanh lo o hai dau -> trang o giua
    uint16_t dot = g.color565((uint8_t)(60 + 195 * mix), 255, (uint8_t)(235 + 20 * mix));
    if (h > 2) {
      g.fillRoundRect(x - 2, baseY - h - 1, 5, h + 3, 2, g.color565(0, 70, 80));    // quang mo
      g.fillRoundRect(x - 1, baseY - h, 3, h + 2, 1, g.color565(40, 255, 240));     // than vach
      g.drawFastVLine(x, baseY - h + 1, h, g.color565(190, 255, 255));              // loi sang
    }
    g.fillRect(x - 1, baseY, 3, 3, dot);
  }

  // Thoi gian da phat o goc tren trai
  uint32_t sec = (nowMs - musicStartMs) / 1000;
  char buf[12];
  snprintf(buf, sizeof(buf), "%u:%02u", (unsigned)(sec / 60), (unsigned)(sec % 60));
  g.setFont(&fonts::Font2);
  g.setTextDatum(m5gfx::textdatum_t::top_left);
  g.setTextColor(g.color565(90, 220, 255));
  g.drawString(buf, 8, 6);
}

// Thac pho: moi khung them mot dong mau o tren cung (xanh = nho, do = lon), cac dong cu troi xuong
static void renderWaterfall(M5Canvas &g) {
  const int rowH = 3;
  g.scroll(0, rowH);
  for (int b = 0; b < BARS; b++) {
    float lv = min(1.0f, barLevel[b]);
    int x0 = b * MC_W / BARS, x1 = (b + 1) * MC_W / BARS;
    uint16_t c = lv < 0.03f ? TFT_BLACK
                            : hsv565((uint16_t)((240 - 240 * lv) * 65536L / 360), 255, (uint8_t)(70 + 185 * lv));
    g.fillRect(x0, 0, x1 - x0, rowH, c);
  }
}

// Gio:phut o goc phai thanh tieu de cua man hinh nhac
static void drawMusicClock(const struct tm &t) {
  auto &d = M5.Display;
  char buf[8] = "--:--";
  if (timeValid(t)) snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  d.fillRect(d.width() - 64, 0, 64, HEADER_H, COL_HEADER);
  d.loadFont(VI_B18);
  d.setTextDatum(m5gfx::textdatum_t::middle_right);
  d.setTextColor(TFT_WHITE, COL_HEADER);
  d.drawString(buf, d.width() - 8, HEADER_H / 2);
  d.unloadFont();
}

static bool enterMusic() {
  if (!musicTablesReady) initMusicTables();
  M5.Speaker.end();   // loa va micro dung chung I2S tren Core2
  auto mc = M5.Mic.config();
  mc.sample_rate = MUSIC_RATE;
  M5.Mic.config(mc);
  if (!M5.Mic.begin()) {
    Serial.println("Nhac: khong mo duoc micro");
    return false;
  }
  memset(barLevel, 0, sizeof(barLevel));
  memset(noiseFloor, 0, sizeof(noiseFloor));
  memset(bandSmooth, 0, sizeof(bandSmooth));
  musicAgc = AGC_FLOOR;
  bassAvg = beatLevel = 0;
  musicFull = true;
  musicStartMs = millis();
  inMusic = true;
  Serial.println("Nhac: bat");
  return true;
}

static void exitMusic() {
  inMusic = false;
  musicLevel = musicBeat = 0;
  M5.Mic.end();
  musicCanvas.deleteSprite();   // tra lai ~128 KB PSRAM
  musicCanvasReady = false;
  bgChanged = true;   // loop() ve lai man hinh dong ho
  Serial.println("Nhac: tat");
}

// Mot khung: lay 256 mau moi nhat tu micro, phan tich, cap nhat cot / vien / LED
static void musicFrame(uint32_t nowMs) {
  auto &d = M5.Display;
  if (musicFull) {
    musicFull = false;
    drawMusicStatic();
    lastSecond = 0;   // ve gio ngay
  }

  // record() xep bo dem moi vao hang doi; bo dem cua lan goi truoc luc nay da day
  if (!M5.Mic.record(micBuf[micIdx], MUSIC_N, MUSIC_RATE)) return;
  const int16_t *data = micBuf[micIdx ^ 1];
  micIdx ^= 1;

  long sum = 0;
  for (int i = 0; i < MUSIC_N; i++) sum += data[i];
  float mean = (float)sum / MUSIC_N;   // bo thanh phan mot chieu
  for (int i = 0; i < MUSIC_N; i++) {
    fftRe[i] = (data[i] - mean) * hannWin[i];
    fftIm[i] = 0;
  }
  fft(fftRe, fftIm, MUSIC_N);

  float mags[BARS], frameMax = 0, floorSum = 0;
  int peakBar = 0;
  for (int b = 0; b < BARS; b++) {
    float s = 0;
    for (int k = bandEdge[b]; k < bandEdge[b + 1]; k++) s += sqrtf(fftRe[k] * fftRe[k] + fftIm[k] * fftIm[k]);
    s = s / (bandEdge[b + 1] - bandEdge[b]) * (1.0f + b * 0.15f);   // bu cho dai cao von yeu hon

    // Tieng on nen: bam theo muc thap nhat gan day (tut xuong ngay, nhich len rat cham ~12%/giay).
    // Tieng on deu (quat, may tinh) bi tru di; nhac thay doi lien tuc nen vuot len tren no.
    // Bam theo gia tri da lam muot (khong phai tung khung) vi tap am dao dong manh giua cac khung.
    bandSmooth[b] = bandSmooth[b] <= 0 ? s : bandSmooth[b] * 0.9f + s * 0.1f;
    if (noiseFloor[b] <= 0 || bandSmooth[b] < noiseFloor[b]) noiseFloor[b] = bandSmooth[b];
    else noiseFloor[b] *= 1.002f;
    s = max(0.0f, s - 2.5f * noiseFloor[b]);

    mags[b] = s;
    if (s > frameMax) { frameMax = s; peakBar = b; }
    floorSum += noiseFloor[b];
  }
  if (frameMax > musicPeakLog) { musicPeakLog = frameMax; musicPeakBar = peakBar; }

  // Tu dong chinh do nhay: theo dinh gan day, giam cham; khong xuong duoi nguong im lang
  // (nguong = hang so hoac vai lan tieng on nen, de tap am con sot khong bi khuech dai len day cot)
  float agcMin = max(AGC_FLOOR, 3.0f * floorSum / BARS);
  musicAgc = max(musicAgc * 0.992f, frameMax);
  if (musicAgc < agcMin) musicAgc = agcMin;

  // Muc cua tung dai: len nhanh, xuong cham
  float total = 0;
  for (int b = 0; b < BARS; b++) {
    float target = powf(mags[b] / musicAgc, 0.6f);
    barLevel[b] = target > barLevel[b] ? target : max(0.0f, barLevel[b] - 0.05f);
    total += barLevel[b];
  }

  // Nhip: bass vot len so voi trung binh gan day
  float bass = (mags[0] + mags[1] + mags[2]) / 3.0f / musicAgc;
  if (bass > bassAvg * 1.5f && bass > 0.25f) beatLevel = 1.0f; else beatLevel *= 0.82f;
  bassAvg = bassAvg * 0.9f + bass * 0.1f;

  float level = total / BARS;
  musicLevel = (uint8_t)min(255.0f, level * 400.0f);
  musicBeat = (uint8_t)(beatLevel * 255.0f);

  d.startWrite();
  if (musicStyle == MS_BARS || !ensureMusicCanvas()) {
    // Cot: chi ve phan thay doi (them nac moi hoac xoa nac thua)
    for (int b = 0; b < BARS; b++) {
      int segs = min(SEGS, (int)(barLevel[b] * SEGS + 0.5f));
      int x = BAR_X0 + b * BAR_PITCH;
      for (int s = barShown[b]; s < segs; s++) {
        d.fillRect(x, BAR_BOTTOM - (s + 1) * SEG_PITCH + (SEG_PITCH - SEG_H), BAR_W, SEG_H, segColor[s]);
      }
      for (int s = segs; s < barShown[b]; s++) {
        d.fillRect(x, BAR_BOTTOM - (s + 1) * SEG_PITCH + (SEG_PITCH - SEG_H), BAR_W, SEG_H, TFT_BLACK);
      }
      barShown[b] = segs;
    }
  } else {
    switch (musicStyle) {
      case MS_ORB: renderOrb(musicCanvas, nowMs); break;
      case MS_MIRROR: renderMirror(musicCanvas); break;
      case MS_RADIAL: renderRadial(musicCanvas, nowMs); break;
      case MS_ICON: renderIcon(musicCanvas, data, mean, nowMs); break;
      case MS_HEARTS: renderHearts(musicCanvas, nowMs); break;
      default: renderWaterfall(musicCanvas); break;
    }
    musicCanvas.pushSprite(&d, MC_X, MC_Y);
  }

  // Vien quanh vung cot: mau troi dan, sang theo nhip
  uint8_t v = (uint8_t)(30 + 225 * max(beatLevel, level));
  uint32_t rgb = leds.ColorHSV((uint16_t)(nowMs * 12), 255, v);
  uint16_t c = ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F);
  int w = d.width(), h = d.height() - MUSIC_TOP;
  d.fillRect(0, MUSIC_TOP, w, BORDER, c);
  d.fillRect(0, d.height() - BORDER, w, BORDER, c);
  d.fillRect(0, MUSIC_TOP, BORDER, h, c);
  d.fillRect(w - BORDER, MUSIC_TOP, BORDER, h, c);
  d.endWrite();

  if (nowMs - musicLogAt >= 1000) {   // muc tin hieu, de can chinh nguong
    musicLogAt = nowMs;
    Serial.printf("Nhac: dinh %.0f o cot %d, nen %.0f, agc %.0f, muc %u\n", musicPeakLog, musicPeakBar,
                  floorSum / BARS, musicAgc, (unsigned)musicLevel);
    musicPeakLog = 0;
  }
}

// Chup man hinh: doc nguoc diem anh tu LCD, tra ve BMP 24 bit (http://<ip>/screenshot.bmp).
// Them ?preview=ap de xem man hinh ma QR cai WiFi ma khong can lam mat mang that.
static void handleScreenshot() {
  auto &d = M5.Display;
  const int w = d.width(), h = d.height();
  const uint32_t rowBytes = (uint32_t)w * 3;   // 320*3 chia het cho 4 nen khong can dem
  const uint32_t dataBytes = rowBytes * h;
  uint8_t *row = (uint8_t *)malloc(rowBytes);
  if (!row) {
    server.send(500, "text/plain", "het bo nho");
    return;
  }

  bool previewAp = server.arg("preview") == "ap" && !apActive && !overlayActive();
  if (previewAp) {
    apActive = true;
    drawWeather();
    apActive = false;
  }

  uint8_t hdr[54] = {'B', 'M'};
  auto put32 = [&](int off, uint32_t v) { memcpy(hdr + off, &v, 4); };
  put32(2, 54 + dataBytes);
  put32(10, 54);
  put32(14, 40);
  put32(18, w);
  put32(22, h);        // duong: dong duoi cung truoc
  hdr[26] = 1;
  hdr[28] = 24;
  put32(34, dataBytes);

  WiFiClient client = server.client();
  server.setContentLength(54 + dataBytes);
  server.send(200, "image/bmp", "");
  client.write(hdr, sizeof(hdr));
  for (int y = h - 1; y >= 0; y--) {
    d.readRectRGB(0, y, w, 1, row);                                       // R,G,B
    for (uint32_t i = 0; i < rowBytes; i += 3) std::swap(row[i], row[i + 2]);   // BMP can B,G,R
    client.write(row, rowBytes);
  }
  free(row);

  if (previewAp) drawWeather();   // tra lai man hinh binh thuong
}

// ---------- Cap nhat firmware tu xa (FOTA) ----------
// Hai cach: (1) chon file firmware.bin tren trang web; (2) nhap URL file .bin de dong ho tu tai.
// Ghi vao slot OTA con lai (phan vung default_16MB co 2 slot), xong moi doi slot va khoi dong lai,
// nen mat dien / loi giua chung van chay tiep ban cu. OTA_PASSWORD (config.h) khac rong thi phai
// gui kem tham so key.
#ifndef FW_VERSION
#define FW_VERSION __DATE__ " " __TIME__
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

static bool otaBusy = false;          // dang ghi firmware vao flash
static bool otaUrlPending = false;    // da nhan URL, loop() se tai
static String otaUrl, otaMd5;
static String otaMsg;                 // ket qua lan gan nhat ("OK" hoac ly do loi), trang web doc
static String otaUpErr;               // loi trong luc nhan file tu trang web
static bool otaUpOk = false;
static size_t otaWritten = 0;
static int otaLastDrawn = -1;         // % da ve; -1 = chua ve khung man hinh

static bool otaAuthorized() {
  return strlen(OTA_PASSWORD) == 0 || server.arg("key") == OTA_PASSWORD;
}

// Man hinh tien do. total <= 0: chua biet dung luong, hien so KB da nhan.
static void otaDraw(size_t written, size_t total) {
  auto &d = M5.Display;
  int pct = total > 0 ? (int)min<size_t>(100, written * 100 / total) : 0;
  int key = total > 0 ? pct : (int)(written / 16384);
  if (key == otaLastDrawn) return;
  if (otaLastDrawn < 0) {
    d.fillScreen(TFT_BLACK);
    d.setTextDatum(m5gfx::textdatum_t::middle_center);
    d.loadFont(VI_B18);
    d.setTextColor(TFT_WHITE);
    d.drawString("Đang cập nhật firmware", 160, 80);
    d.unloadFont();
    d.loadFont(VI_R14);
    d.setTextColor(COL_DIM);
    d.drawString("Đừng tắt nguồn", 160, 200);
    d.unloadFont();
    d.drawRoundRect(30, 110, 260, 22, 6, TFT_WHITE);
  }
  otaLastDrawn = key;
  if (total > 0) d.fillRect(32, 112, 256 * pct / 100, 18, COL_ACCENT);
  char txt[16];
  if (total > 0) snprintf(txt, sizeof(txt), "%d%%", pct); else snprintf(txt, sizeof(txt), "%u KB", (unsigned)(written / 1024));
  d.fillRect(110, 145, 100, 26, TFT_BLACK);
  d.setFont(&fonts::FreeSansBold9pt7b);
  d.setTextDatum(m5gfx::textdatum_t::middle_center);
  d.setTextColor(TFT_WHITE);
  d.drawString(txt, 160, 158);
}

// Giai phong tai nguyen dung chung truoc khi ghi flash
static void otaPrepare() {
  if (inMusic) exitMusic();
  M5.Speaker.stop();
  otaLastDrawn = -1;
  otaWritten = 0;
}

static void otaFailed(const String &why) {
  Update.abort();
  otaBusy = false;
  otaMsg = why;
  Serial.printf("OTA: loi - %s\n", why.c_str());
  otaLastDrawn = -1;
  bgChanged = true;   // ve lai man hinh dong ho
  toastText = "Cap nhat loi";
  toastUntil = millis() + 5000;
}

static void otaRestart() {
  otaMsg = "OK";
  auto &d = M5.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextDatum(m5gfx::textdatum_t::middle_center);
  d.loadFont(VI_B18);
  d.setTextColor(TFT_GREEN);
  d.drawString("Xong, đang khởi động lại", 160, 120);
  d.unloadFont();
  Serial.println("OTA: xong, khoi dong lai");
  delay(1000);
  ESP.restart();
}

// Cach 1: nhan file multipart tu trang web va ghi thang vao flash (khong dem trong RAM)
static void handleOtaUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaUpErr = "";
    otaUpOk = false;
    if (!otaAuthorized()) { otaUpErr = "Sai mật khẩu cập nhật"; return; }
    if (otaBusy || otaUrlPending) { otaUpErr = "Đang cập nhật, thử lại sau"; return; }
    otaPrepare();
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { otaFailed(Update.errorString()); otaUpErr = otaMsg; return; }
    otaBusy = true;
    Serial.printf("OTA: nhan file %s\n", up.filename.c_str());
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (!otaBusy) return;
    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
      otaFailed(Update.errorString());
      otaUpErr = otaMsg;
      return;
    }
    otaWritten += up.currentSize;
    otaDraw(otaWritten, (size_t)max(0, server.clientContentLength()));   // gom ca khung multipart: xap xi
  } else if (up.status == UPLOAD_FILE_END) {
    if (!otaBusy) return;
    if (!Update.end(true)) { otaFailed(Update.errorString()); otaUpErr = otaMsg; return; }
    otaBusy = false;
    otaUpOk = true;
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (otaBusy) { otaFailed("Bị ngắt giữa chừng"); otaUpErr = otaMsg; }
  }
}

static void handleOtaDone() {
  if (!otaUpOk) {
    if (otaUpErr.isEmpty()) otaUpErr = otaAuthorized() ? "Không nhận được file" : "Sai mật khẩu cập nhật";
    server.send(otaAuthorized() ? 400 : 403, "text/plain; charset=utf-8", otaUpErr);
    return;
  }
  server.send(200, "text/plain; charset=utf-8", "Đã ghi firmware mới. Đồng hồ đang khởi động lại...");
  otaRestart();
}

// Cach 2: dong ho tu tai file .bin tu URL (http, hoac https khong kiem tra chung chi). md5 la tuy chon.
static void handleOtaUrl() {
  if (!otaAuthorized()) {
    server.send(403, "text/plain; charset=utf-8", "Sai mật khẩu cập nhật");
    return;
  }
  String url = server.arg("url"), md5 = server.arg("md5");
  url.trim();
  md5.trim();
  md5.toLowerCase();
  bool md5Ok = md5.isEmpty() || md5.length() == 32;
  for (size_t i = 0; md5Ok && i < md5.length(); i++) md5Ok = isxdigit((uint8_t)md5[i]);
  if ((!url.startsWith("http://") && !url.startsWith("https://")) || url.length() > 300 || !md5Ok) {
    server.send(400, "text/plain; charset=utf-8", "Cần URL http(s) hợp lệ; MD5 để trống hoặc 32 ký tự hex");
    return;
  }
  if (otaBusy || otaUrlPending) {
    server.send(409, "text/plain; charset=utf-8", "Đang cập nhật, thử lại sau");
    return;
  }
  otaUrl = url;
  otaMd5 = md5;
  otaMsg = "";
  otaUrlPending = true;
  server.send(200, "text/plain; charset=utf-8", "Đồng hồ đang tải firmware, xem tiến độ trên màn hình.");
}

// Chay trong loop() (sau khi da tra loi trang web): tai file va ghi vao flash
static void otaRunUrl() {
  otaUrlPending = false;
  otaPrepare();
  otaBusy = true;
  Serial.printf("OTA: tai %s\n", otaUrl.c_str());
  otaDraw(0, 1);

  WiFiClientSecure secure;
  WiFiClient plain;
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);   // GitHub Releases chuyen huong sang CDN
  bool began;
  if (otaUrl.startsWith("https://")) {
    secure.setInsecure();   // khong kiem tra chung chi: nen dat md5 de xac thuc file
    began = http.begin(secure, otaUrl);
  } else {
    began = http.begin(plain, otaUrl);
  }
  if (!began) return otaFailed("URL không hợp lệ");

  int status = http.GET();
  if (status != HTTP_CODE_OK) {
    http.end();
    return otaFailed(status > 0 ? "Máy chủ trả về HTTP " + String(status) : "Không kết nối được máy chủ");
  }
  int total = http.getSize();
  if (total <= 0) {
    http.end();
    return otaFailed("Máy chủ không báo dung lượng file");
  }
  if (!Update.begin(total)) {
    http.end();
    return otaFailed(Update.errorString());
  }
  if (otaMd5.length() == 32) Update.setMD5(otaMd5.c_str());

  WiFiClient *stream = http.getStreamPtr();
  static uint8_t buf[1460];
  uint32_t lastData = millis();
  while (otaWritten < (size_t)total) {
    size_t avail = stream->available();
    if (avail) {
      int n = stream->readBytes(buf, min(avail, sizeof(buf)));
      if (Update.write(buf, n) != (size_t)n) {
        http.end();
        return otaFailed(Update.errorString());
      }
      otaWritten += n;
      lastData = millis();
      otaDraw(otaWritten, total);
    } else if (!stream->connected() || millis() - lastData > 15000) {
      break;
    } else {
      delay(2);
    }
  }
  http.end();
  if (otaWritten != (size_t)total) return otaFailed("Tải thiếu dữ liệu, thử lại");
  if (!Update.end(true)) return otaFailed(Update.errorString());
  otaBusy = false;
  otaRestart();
}

static void handleOtaStatus() {
  const esp_partition_t *run = esp_ota_get_running_partition();
  char json[360];
  snprintf(json, sizeof(json),
           "{\"version\":\"%s\",\"partition\":\"%s\",\"busy\":%s,\"msg\":\"%s\",\"needKey\":%s,\"up\":%lu}",
           FW_VERSION, run ? run->label : "?", (otaBusy || otaUrlPending) ? "true" : "false",
           jsonEscape(otaMsg).c_str(), strlen(OTA_PASSWORD) ? "true" : "false", (unsigned long)(millis() / 1000));
  server.send(200, "application/json; charset=utf-8", json);
}

static void startWeb() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", WEB_PAGE); });
  server.on("/api/ota", HTTP_POST, handleOtaDone, handleOtaUpload);
  server.on("/api/ota/url", HTTP_POST, handleOtaUrl);
  server.on("/api/ota/status", HTTP_GET, handleOtaStatus);
  server.on("/screenshot.bmp", HTTP_GET, handleScreenshot);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/sleep", HTTP_POST, handleSleepConfig);
  server.on("/api/sleepnow", HTTP_POST, []() {
    if (!sleeping) enterSleep(true);
    server.send(200, "text/plain", "OK");
  });
  server.on("/api/wake", HTTP_POST, []() {
    if (sleeping) wakeUp();
    server.send(200, "text/plain", "OK");
  });
  // Bat / tat man hinh nhac va chon kieu song tu xa. Tham so: on=0|1 (tuy chon), style=0..4 (tuy chon)
  server.on("/api/music", HTTP_POST, []() {
    if (inSettings) {
      server.send(409, "text/plain; charset=utf-8", "Đồng hồ đang ở màn hình cài đặt");
      return;
    }
    if (server.hasArg("style")) {
      long s = server.arg("style").toInt();
      if (s < 0 || s >= MS_COUNT) {
        server.send(400, "text/plain; charset=utf-8", "Kiểu sóng không hợp lệ");
        return;
      }
      setMusicStyle((uint8_t)s);
    }
    bool ok = true;
    bool on = server.hasArg("on") ? server.arg("on").toInt() != 0 : inMusic;
    if (on && !inMusic) ok = enterMusic();
    else if (!on && inMusic) exitMusic();
    server.send(ok ? 200 : 500, "text/plain; charset=utf-8", ok ? "OK" : "Không mở được micro");
  });
  server.on("/api/location", HTTP_POST, handleLocation);
  server.on("/api/chime", HTTP_POST, handleChime);   // on, vol (0-100), quiet: tham so nao co thi doi
  server.on("/api/chime/sound", HTTP_POST, handleChimeUploadDone, handleChimeUpload);
  server.on("/api/chime/sound/reset", HTTP_POST, handleChimeReset);
  server.on("/api/chime/test", HTTP_POST, []() {      // nghe thu ngay, bat ke dang tat hay bat
    if (!playChime()) {
      server.send(409, "text/plain; charset=utf-8", "Đang ở màn hình nhạc, loa dùng chung với micro");
      return;
    }
    server.send(200, "text/plain; charset=utf-8", "OK");
  });
  server.on("/api/datemode", HTTP_POST, []() {   // lunar=1: am lich, lunar=0: duong lich
    setDateMode((uint8_t)server.arg("mode").toInt());   // 0 duong, 1 am, 2 tu dong
    server.send(200, "text/plain", "OK");
  });
  // Doi mot ngay duong sang am lich (kiem tra thuat toan): /api/lunar?y=2026&m=2&d=17
  server.on("/api/lunar", HTTP_GET, []() {
    int y = server.arg("y").toInt(), m = server.arg("m").toInt(), d = server.arg("d").toInt();
    if (y < 1900 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) {
      server.send(400, "text/plain", "y 1900-2100, m 1-12, d 1-31");
      return;
    }
    LunarDate l = solarToLunar(d, m, y);
    char json[96];
    snprintf(json, sizeof(json), "{\"day\":%d,\"month\":%d,\"year\":%d,\"leap\":%s,\"name\":\"%s %s\"}", l.day,
             l.month, l.year, l.leap ? "true" : "false", CAN_NAMES[(l.year + 6) % 10], CHI_NAMES[(l.year + 8) % 12]);
    server.send(200, "application/json; charset=utf-8", json);
  });
  server.on("/api/colors", HTTP_POST, handleColors);
  server.on("/api/colors/reset", HTTP_POST, handleColorsReset);
  server.on("/api/bg", HTTP_POST, handleBgDone, handleBgUpload);
  server.on("/api/bg/reset", HTTP_POST, handleBgReset);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/wifi", HTTP_POST, handleWifiSave);
  server.onNotFound([]() {
    if (apActive) {   // captive portal: moi dia chi khac deu chuyen ve trang cau hinh
      server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });
  server.begin();
  if (MDNS.begin("m5clock")) MDNS.addService("http", "tcp", 80);
  webStarted = true;
}

// ---------- Chuong trinh chinh ----------

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  loadSchedule();
  loadWifi();
  loadCustomBg();
  loadCustomChime();

  leds.begin();
  leds.setBrightness(LED_BRIGHTNESS);
  leds.clear();
  leds.show();
  xTaskCreatePinnedToCore(ledTask, "led", 3072, nullptr, 1, nullptr, 1);

  M5.Display.setRotation(1);
  M5.Display.setBrightness(BRIGHTNESS_LEVELS[brightnessIdx]);
  drawBg(M5.Display, 0, 0, 0, 0, M5.Display.width(), M5.Display.height());
  // Dat mui gio truoc, sau do lay gio tu RTC de co gio ngay ca khi chua co mang
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
  systemTimeFromRtc();

  WiFi.mode(WIFI_STA);
  if (wifiConfigured()) {
    WiFi.setAutoReconnect(true);
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
    Serial.printf("WiFi: dang ket noi \"%s\"\n", wifiSsid.c_str());
  } else {
    Serial.println("WiFi: chua cau hinh, se phat WiFi cai dat");
  }

  drawWeather();
}

void loop() {
  M5.update();
  uint32_t nowMs = millis();
  bool online = WiFi.status() == WL_CONNECTED;

  // WiFi vua duoc doi tren trang web: ket noi lai bang thong tin moi
  if (wifiApplyAt && (int32_t)(nowMs - wifiApplyAt) >= 0) {
    wifiApplyAt = 0;
    everOnline = false;
    offlineSince = nowMs;
    lastStaRetry = nowMs;
    WiFi.disconnect();
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
    online = false;
  }

  // Vao duoc mang thi tat diem phat; mat mang du lau (hoac chua co cau hinh) thi bat len
  if (online) {
    offlineSince = nowMs;
    if (apActive) stopAp();
  } else if (!apActive) {
    uint32_t limit = everOnline ? AP_AFTER_LOSS_MS : AP_AFTER_BOOT_MS;
    if (!wifiConfigured() || (uint32_t)(nowMs - offlineSince) >= limit) startAp();
  } else if (wifiConfigured() && WiFi.softAPgetStationNum() == 0 &&
             (uint32_t)(nowMs - lastStaRetry) >= STA_RETRY_MS) {
    // Dang phat AP ma khong ai ket noi: thu lai WiFi da luu (router co the vua bat lai)
    lastStaRetry = nowMs;
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  }
  if (apActive) dns.processNextRequest();

  static bool wasOnline = false;
  if (online != wasOnline) {
    wasOnline = online;
    Serial.printf("WiFi: %s\n", online ? WiFi.localIP().toString().c_str() : "mat ket noi");
    if (online) {
      everOnline = true;
      nextWeatherAt = nowMs;
      if (!webStarted) startWeb();
      // Hien IP vai giay de biet dia chi mo trang cau hinh tren dien thoai
      snprintf(ipToast, sizeof(ipToast), "%s", WiFi.localIP().toString().c_str());
      toastText = ipToast;
      toastUntil = nowMs + 8000;
      lastSecond = 0;
      Serial.printf("Web: http://%s/  hoac  http://m5clock.local/\n", ipToast);
    }
    if (!weather.valid && !overlayActive()) drawWeather();
  }

  if (webStarted) server.handleClient();
  if (otaUrlPending) otaRunUrl();   // tai firmware tu URL (chan loop() den khi xong)

  // Anh nen vua doi qua web: ve lai toan bo
  if (bgChanged) {
    bgChanged = false;
    if (inSettings) {
      settingsFull = true;
    } else if (inMusic) {
      musicFull = true;
    } else {
      drawBg(M5.Display, 0, 0, 0, 0, M5.Display.width(), M5.Display.height());
      lastMinute = -1;
      lastHeader = "";
      lastSecond = 0;
      drawWeather();
    }
  }

  // NTP vua dong bo: ghi gio UTC vao RTC va ve lai dong ho
  if (ntpSynced) {
    ntpSynced = false;
    time_t now = time(nullptr);
    M5.Rtc.setDateTime(gmtime(&now));
    lastMinute = -1;
    Serial.println("NTP: da dong bo, da ghi RTC");
  }

  bool btnADown = M5.BtnA.wasPressed();
  bool btnAClick = M5.BtnA.wasClicked();   // bam nhanh, tha ra
  bool btnAHold = M5.BtnA.wasHold();       // giu
  bool btnBDown = M5.BtnB.wasPressed();
  bool btnBClick = M5.BtnB.wasClicked();
  bool btnBHold = M5.BtnB.wasHold();
  bool btnCDown = M5.BtnC.wasPressed();
  auto touch = M5.Touch.getDetail();
  bool touched = touch.wasPressed();
  bool anyPress = btnADown || btnBDown || btnCDown || touched;

  // Vuot ngang tren man hinh (tinh luc nha tay): du dai va ngang nhieu hon doc
  int swipeDx = touch.distanceX(), swipeDy = touch.distanceY();
  bool swiped = touch.wasReleased() && touch.base_y < M5.Display.height() &&
                abs(swipeDx) >= 60 && abs(swipeDx) > 2 * abs(swipeDy);
  bool swipeLeft = swiped && swipeDx < 0;    // phai -> trai: vao man hinh nhac
  bool swipeRight = swiped && swipeDx > 0;   // trai -> phai: ve dong ho
  // Cham nhe (khong keo) vao so dong ho: doi kieu so
  bool tapClock = touch.wasClicked() && touch.y >= TIME_CY - 48 && touch.y <= TIME_CY + 48;
  // Cham vao dong ngay thang o thanh tieu de (tru phan icon WiFi/pin): doi duong lich <-> am lich
  bool tapDate = touch.wasClicked() && touch.y >= 0 && touch.y < HEADER_H && touch.x < 245;

  // Nut vua dung de danh thuc / vao ngu / huy cai dat thi bo qua cho toi khi tha ra,
  // de lan tha tay khong bi tinh thanh mot lan bam nhanh
  if (ignoreA) {
    btnAClick = btnAHold = false;
    if (!M5.BtnA.isPressed()) ignoreA = false;
  }
  if (ignoreB) {
    btnBClick = btnBHold = false;
    if (!M5.BtnB.isPressed()) ignoreB = false;
  }

  if (anyPress) lastActivityMs = nowMs;

  if (sleeping) {
    // Lan cham/bam dau tien chi danh thuc, khong kich hoat chuc nang cua nut
    if (anyPress) {
      wakeUp();
      if (btnADown) ignoreA = true;
      if (btnBDown) ignoreB = true;
    }
  } else if (inSettings) {
    if (btnAHold) {
      exitSettings(false);
      ignoreA = true;
    } else if (btnAClick) {
      if (editField < SETTINGS_FIELDS - 1) {
        settingsRowsDirty |= (1 << editField) | (1 << (editField + 1));   // muc cu va muc moi
        settingsBtnDirty = true;                                          // nhan Tiep / Luu
        editField++;
      } else {
        exitSettings(true);
      }
    } else if (btnBDown || btnCDown) {
      adjustField(btnCDown ? +1 : -1);
      settingsRowsDirty |= 1 << editField;
      settingsHeaderDirty = true;
    } else if ((uint32_t)(nowMs - lastActivityMs) > SETTINGS_IDLE_MS) {
      exitSettings(false);
    }
  } else if (inMusic) {
    lastActivityMs = nowMs;   // dang xem nhac: khong tu ngu theo lich
    if (swipeRight) {
      exitMusic();
    } else if (btnBClick) {
      setMusicStyle(musicStyle + 1);   // nut B: doi kieu song nhac
    } else if (btnBHold) {
      enterSleep(true);
      ignoreB = true;
    } else if (btnCDown) {
      brightnessIdx = (brightnessIdx + 1) % sizeof(BRIGHTNESS_LEVELS);
      M5.Display.setBrightness(BRIGHTNESS_LEVELS[brightnessIdx]);
    }
  } else if (swipeLeft) {
    enterMusic();
  } else if (btnBHold) {
    enterSleep(true);
    ignoreB = true;
  } else if (btnBClick) {
    setLedMode((ledMode + 1) % LED_MODE_COUNT);
  } else if (btnAClick) {
    enterSettings();
  } else if (btnAHold) {
    if (online) nextWeatherAt = nowMs;   // cap nhat thoi tiet ngay
    ignoreA = true;
  } else if (btnCDown) {
    brightnessIdx = (brightnessIdx + 1) % sizeof(BRIGHTNESS_LEVELS);
    M5.Display.setBrightness(BRIGHTNESS_LEVELS[brightnessIdx]);
  } else if (tapClock) {
    cycleClockStyle();
  } else if (tapDate) {
    setDateMode(dateMode == 2 ? 0 : dateMode + 1);   // 2 -> 0 -> 1 -> 2
  }

  if (inMusic) musicFrame(nowMs);

  // Tu dong xac dinh khu vuc theo IP mang (truoc khi tai thoi tiet de dung toa do moi)
  if (online && locAuto && (int32_t)(nowMs - nextLocAt) >= 0) {
    bool changed = false;
    bool ok = lookupLocation(&changed);
    nextLocAt = millis() + (ok ? LOC_INTERVAL_MS : LOC_RETRY_MS);
    if (ok) {
      Serial.printf("Khu vuc: %s (%.4f, %.4f)%s\n", locName.c_str(), (double)locLat, (double)locLon,
                    changed ? " - moi" : "");
      saveLocation();
      if (changed) nextWeatherAt = millis();   // noi o doi: tai lai thoi tiet ngay
    } else {
      Serial.println("Khu vuc: tra cuu that bai, giu khu vuc cu");
    }
  }

  if (online && (int32_t)(nowMs - nextWeatherAt) >= 0) {
    bool ok = fetchWeather();
    weatherEverTried = true;
    nextWeatherAt = millis() + (ok ? WEATHER_INTERVAL_MS : WEATHER_RETRY_MS);
    if (!overlayActive()) drawWeather();
  }

  // Icon thoi tiet chuyen dong
  static uint32_t lastIconMs = 0;
  if (weather.valid && !apActive && !overlayActive() && !sleeping && (uint32_t)(nowMs - lastIconMs) >= 66) {
    lastIconMs = nowMs;
    drawIconFrame(nowMs);
    if (descTextW > DESC_W) drawDescFrame(nowMs);   // chu mo ta dai: chay ngang
  }

  if (inSettings && (settingsFull || settingsRowsDirty || settingsHeaderDirty || settingsBtnDirty)) {
    drawSettings();
  }

  time_t now = time(nullptr);
  if (now != lastSecond) {
    lastSecond = now;
    struct tm t;
    localtime_r(&now, &t);
    checkHourChime(t);
    if (!overlayActive()) {
      drawHeader(t);
      drawClock(t);
    } else if (inMusic && !musicFull) {
      drawMusicClock(t);
    }

    // Lich ngu: vao khoang ngu thi tat sau khi het thao tac; het khoang ngu thi tu danh thuc
    bool inWindow = inSleepWindow(t);
    if (sleeping && !manualSleep && !inWindow) {
      wakeUp();
    } else if (!sleeping && !overlayActive() && inWindow &&
               (nowMs - lastActivityMs) >= sched.idleSec * 1000UL) {
      enterSleep(false);
    }
  }

  delay(inMusic ? 1 : 20);   // man hinh nhac can khung hinh nhanh; record() da tu cho ~16 ms
}
