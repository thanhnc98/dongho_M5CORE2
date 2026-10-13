# Pre-build (PlatformIO): giam IRAM de WiFi + BLE (app Chronos) cung vua.
# Ban dung san cua Arduino-ESP32 2.0.x bat CONFIG_SPIRAM_CACHE_WORKAROUND nen nhet CA nhom ham thoi gian
# cua libc (strftime, strptime, mktime, tzset...) vao IRAM (~10 KB). Core2 dung ESP32 rev3 (khong con loi
# cache PSRAM) va cac ham nay khong dung trong ngat, nen cho chung chay tu flash.
# Script sao chep sections.ld cua SDK, bo cac dong dat nhom nay vao IRAM, roi cho linker tim ban sao truoc.
Import("env")
import os, re

fw = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
src = os.path.join(fw, "tools", "sdk", "esp32", "ld", "sections.ld")
out_dir = os.path.join(env.subst("$BUILD_DIR"), "ld_override")
out = os.path.join(out_dir, "sections.ld")

TIME_OBJS = ("asctime", "asctime_r", "ctime", "ctime_r", "gettzinfo", "gmtime", "gmtime_r", "lcltime",
             "lcltime_r", "mktime", "month_lengths", "strftime", "strptime", "time", "tzcalc_limits",
             "tzlock", "tzset", "tzset_r", "tzvars")
rule = re.compile(r"^\s*\*libc\.a:lib_a-(%s)\.\*\(\.literal \.literal\.\* \.text \.text\.\*\)\s*$" %
                  "|".join(TIME_OBJS))

with open(src, "r", encoding="utf-8", newline="") as f:
    lines = f.read().split("\n")
kept = [l for l in lines if not rule.match(l.rstrip("\r"))]
os.makedirs(out_dir, exist_ok=True)
with open(out, "w", encoding="utf-8", newline="") as f:
    f.write("\n".join(kept))
print("slim_iram: bo %d dong IRAM (libc thoi gian) khoi sections.ld" % (len(lines) - len(kept)))
env.Prepend(LIBPATH=[out_dir])
