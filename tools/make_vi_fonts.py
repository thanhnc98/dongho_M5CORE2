#!/usr/bin/env python3
"""Tao font VLW (font min, co chu tieng Viet co dau) cho M5GFX tu file TTF, ghi ra src/vi_fonts.h.

Yeu cau: Pillow (pip install pillow). Mac dinh dung Segoe UI cua Windows.
  python tools/make_vi_fonts.py
  python tools/make_vi_fonts.py --regular path/to/Regular.ttf --bold path/to/Bold.ttf
"""
import argparse
import os
import struct

from PIL import Image, ImageDraw, ImageFont

VI_LOWER = ("àáạảãâầấậẩẫăằắặẳẵèéẹẻẽêềếệểễìíịỉĩòóọỏõôồốộổỗơờớợởỡùúụủũưừứựửữỳýỵỷỹđ")
CHARS = sorted({chr(c) for c in range(0x20, 0x7F)} | set(VI_LOWER) | set(VI_LOWER.upper()) | {"°"})


def make_vlw(ttf: str, px: int) -> bytes:
    font = ImageFont.truetype(ttf, px)
    ascent = -font.getbbox("d", anchor="ls")[1]
    descent = font.getbbox("p", anchor="ls")[3]

    headers, bitmaps = [], []
    for ch in CHARS:
        adv = max(1, round(font.getlength(ch)))
        if ch == " ":
            w = h = left = top = 0
            data = b""
        else:
            left, top, right, bottom = font.getbbox(ch, anchor="ls")
            w, h = right - left, bottom - top
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((-left, -top), ch, font=font, fill=255, anchor="ls")
            data = img.tobytes()
        assert w <= 255 and adv <= 255 and -128 <= left <= 127, (ch, w, adv, left)
        # unicode, height, width, xAdvance, dY (tu duong co so len dinh), dX, padding
        headers.append(struct.pack(">7i", ord(ch), h, w, adv, -top, left, 0))
        bitmaps.append(data)

    head = struct.pack(">6i", len(CHARS), 11, px, 0, ascent, descent)
    return head + b"".join(headers) + b"".join(bitmaps)


def c_array(name: str, blob: bytes) -> str:
    lines = [f"// {len(blob)} byte, {len(CHARS)} ky tu", f"static const uint8_t {name}[] PROGMEM = {{"]
    for i in range(0, len(blob), 16):
        lines.append("  " + ",".join(f"0x{b:02X}" for b in blob[i:i + 16]) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n"


def main() -> None:
    fonts_dir = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")
    ap = argparse.ArgumentParser()
    ap.add_argument("--regular", default=os.path.join(fonts_dir, "segoeui.ttf"))
    ap.add_argument("--bold", default=os.path.join(fonts_dir, "segoeuib.ttf"))
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "src", "vi_fonts.h"))
    args = ap.parse_args()

    specs = [("VI_R18", args.regular, 18), ("VI_R14", args.regular, 14), ("VI_B18", args.bold, 18)]
    out = ["// Tao tu dong boi tools/make_vi_fonts.py - khong sua tay.",
           "// Font VLW co chu tieng Viet co dau (ASCII + cac chu cai tieng Viet).",
           "#pragma once", "#include <Arduino.h>", ""]
    for name, ttf, px in specs:
        blob = make_vlw(ttf, px)
        print(f"{name}: {os.path.basename(ttf)} {px}px -> {len(blob)} byte")
        out.append(c_array(name, blob))

    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))
    print("Da ghi", os.path.normpath(args.out))


if __name__ == "__main__":
    main()
