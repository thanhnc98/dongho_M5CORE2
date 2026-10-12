#!/usr/bin/env python3
"""Chuyen file am thanh (MP3/WAV/FLAC/OGG) thanh am thanh bao gio mac dinh -> src/chime_default.h

Am thanh duoc doi sang mono 16 kHz, 16 bit; cat toi da 10 giay; bo doan lang o cuoi; chuan hoa am
luong; them fade-in/fade-out ngan de khong bi tieng "tach" luc bat dau va ket thuc.

Yeu cau: miniaudio, numpy (pip install miniaudio numpy).
  python tools/make_chime.py                      # dung file dau tien trong thu muc amthanh/
  python tools/make_chime.py --src file.mp3 --max-seconds 8
"""
import argparse
import glob
import os

import miniaudio
import numpy as np

RATE = 16000


def load(path: str, max_seconds: float) -> np.ndarray:
    d = miniaudio.decode_file(path, output_format=miniaudio.SampleFormat.SIGNED16, nchannels=1, sample_rate=RATE)
    a = np.frombuffer(d.samples, dtype=np.int16).astype(np.float32)
    a = a[: int(max_seconds * RATE)]

    # Bo doan lang o cuoi: cat tai cua so 50 ms cuoi cung con lon hon 1% dinh
    win = RATE // 20
    thr = max(40.0, 0.01 * float(np.abs(a).max()))
    end = len(a)
    while end > win and np.sqrt(np.mean(a[end - win:end] ** 2)) < thr:
        end -= win
    a = a[: min(len(a), end + win)]

    a *= (0.89 * 32767.0) / max(1.0, float(np.abs(a).max()))   # dinh ~ -1 dBFS
    fi, fo = int(0.01 * RATE), int(0.12 * RATE)
    a[:fi] *= np.linspace(0, 1, fi)
    a[-fo:] *= np.linspace(1, 0, fo)
    return a.astype(np.int16)


def main() -> None:
    root = os.path.join(os.path.dirname(__file__), "..")
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=None)
    ap.add_argument("--max-seconds", type=float, default=10.0)
    ap.add_argument("--out", default=os.path.join(root, "src", "chime_default.h"))
    args = ap.parse_args()
    if args.max_seconds > 10.0:
        raise SystemExit("Toi da 10 giay (bo nho dem tren dong ho).")

    src = args.src
    if not src:
        c = sorted(glob.glob(os.path.join(root, "amthanh", "*.mp3")) + glob.glob(os.path.join(root, "amthanh", "*.wav")))
        if not c:
            raise SystemExit("Khong tim thay file am thanh trong thu muc amthanh/")
        src = c[0]

    pcm = load(src, args.max_seconds)
    lines = ["// Tao tu dong boi tools/make_chime.py - khong sua tay.",
             f"// Nguon: {os.path.basename(src)}  ({len(pcm) / RATE:.2f} s, {RATE} Hz, mono, 16 bit)",
             "#pragma once", "#include <Arduino.h>", "",
             f"static const uint32_t CHIME_DEFAULT_RATE = {RATE};",
             f"static const uint32_t CHIME_DEFAULT_LEN = {len(pcm)};   // so mau",
             f"static const int16_t CHIME_DEFAULT[{len(pcm)}] PROGMEM = {{"]
    for i in range(0, len(pcm), 16):
        lines.append("  " + ",".join(str(int(v)) for v in pcm[i:i + 16]) + ",")
    lines.append("};")
    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"Nguon: {os.path.basename(src)}")
    print(f"Da ghi {os.path.normpath(args.out)}: {len(pcm)} mau = {len(pcm) / RATE:.2f} giay, {len(pcm) * 2} byte")


if __name__ == "__main__":
    main()
