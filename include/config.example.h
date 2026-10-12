// Mau cau hinh: chep file nay thanh config.h roi dien thong tin cua ban.
#pragma once

// WiFi 2.4 GHz (ESP32 khong bat duoc 5 GHz)
#define WIFI_SSID "TEN_WIFI"
#define WIFI_PASS "MAT_KHAU"

// Vi tri lay thoi tiet (Open-Meteo, khong can API key)
#define LOCATION_NAME "TP.HCM"
#define LATITUDE 10.82
#define LONGITUDE 106.63

// Mui gio dang POSIX: "<+07>-7" = UTC+7
#define TZ_INFO "<+07>-7"

// Lich ngu tu dong: tat man hinh + LED trong khoang gio nay.
// Day chi la gia tri MAC DINH; cai dat tren man hinh (nut A) duoc luu va ghi de len cac gia tri nay.
// Dat SLEEP_SCHEDULE_ENABLED 0 de tat tinh nang. Ho tro khoang qua nua dem (vi du 23:00 -> 06:00).
// Trong gio ngu, cham man hinh/bam nut de danh thuc; sau SLEEP_IDLE_SECONDS giay khong
// thao tac man hinh se tu tat lai. Giu nut B ~0,5 giay de ngu thu cong bat ky luc nao.
#define SLEEP_SCHEDULE_ENABLED 1
#define SLEEP_START_HOUR 23
#define SLEEP_START_MIN 0
#define SLEEP_END_HOUR 6
#define SLEEP_END_MIN 0
#define SLEEP_IDLE_SECONDS 30

// Cap nhat firmware tu xa (FOTA) tren trang web. De "" = khong can mat khau (chi nen dung trong mang nha).
#define OTA_PASSWORD ""
// Chuoi phien bien hien tren trang web (mac dinh: ngay gio build)
// #define FW_VERSION "1.0.0"
