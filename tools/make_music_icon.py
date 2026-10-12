#!/usr/bin/env python3
"""Tao bieu tuong cho kieu song nhac "Phi hanh gia" -> src/music_icon.h (RGB565, nen den).

  python tools/make_music_icon.py                 # ve bieu tuong mac dinh (phi hanh gia deo tai nghe)
  python tools/make_music_icon.py --src icon.png  # dung anh cua ban

Anh nguon PHAI la PNG co nen trong suot that su (kenh alpha). Anh xem truoc tai tu cac trang anh
stock (nen o vuong ban co ve san, co chu dong dau) khong dung duoc: hay tai ban goc da cap phep.
Yeu cau: Pillow.
"""
import argparse
import os

from PIL import Image, ImageDraw

SIZE = 96          # canh bieu tuong tren man hinh (diem anh)
SS = 4             # ve lon gap 4 roi thu nho de canh muot


def draw_default() -> Image.Image:
    """Phi hanh gia deo tai nghe, ve bang hinh co ban (khong dung anh cua ai)."""
    n = SIZE * SS
    im = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    u = n / 96.0                                   # 1 don vi = 1 diem anh sau khi thu nho
    line, white, visor, blue = (58, 30, 78, 255), (255, 255, 255, 255), (42, 20, 64, 255), (40, 150, 255, 255)
    w = int(2.2 * u)

    def rr(box, r, fill):
        d.rounded_rectangle([int(v * u) for v in box], radius=int(r * u), fill=fill, outline=line, width=w)

    rr((30, 74, 44, 94), 6, white)                 # chan trai
    rr((52, 74, 66, 94), 6, white)                 # chan phai
    rr((8, 44, 26, 56), 6, white)                  # tay trai (gio len)
    rr((70, 44, 88, 56), 6, white)                 # tay phai
    rr((26, 50, 70, 82), 10, white)                # than
    rr((40, 60, 56, 70), 3, (225, 230, 240, 255))  # hop dieu khien tren nguc
    d.ellipse([int(v * u) for v in (45, 62, 51, 68)], fill=blue)
    # tai nghe: vong qua dau + hai chup tai
    d.arc([int(v * u) for v in (17, 2, 79, 62)], 180, 360, fill=blue, width=int(5 * u))
    d.ellipse([int(v * u) for v in (20, 4, 76, 58)], fill=white, outline=line, width=w)   # mu
    d.ellipse([int(v * u) for v in (26, 11, 70, 51)], fill=visor)                          # kinh
    d.arc([int(v * u) for v in (31, 15, 61, 45)], 195, 250, fill=(150, 130, 190, 255), width=int(2.5 * u))
    rr((12, 22, 24, 44), 5, blue)
    rr((72, 22, 84, 44), 5, blue)
    return im.resize((SIZE, SIZE), Image.LANCZOS)


def load_user(path: str) -> Image.Image:
    im = Image.open(path)
    if im.mode not in ("RGBA", "LA") and "transparency" not in im.info:
        raise SystemExit(
            f"{os.path.basename(path)}: anh khong co nen trong suot (che do {im.mode}).\n"
            "Can PNG co kenh alpha that. Anh xem truoc co nen o vuong / chu dong dau khong dung duoc.")
    im = im.convert("RGBA")
    box = im.getbbox()                 # cat sat phan co hinh
    if box:
        im = im.crop(box)
    im.thumbnail((SIZE, SIZE), Image.LANCZOS)
    out = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    out.paste(im, ((SIZE - im.width) // 2, (SIZE - im.height) // 2))
    return out


def main() -> None:
    root = os.path.join(os.path.dirname(__file__), "..")
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=None, help="PNG nen trong suot; bo trong de ve bieu tuong mac dinh")
    ap.add_argument("--out", default=os.path.join(root, "src", "music_icon.h"))
    ap.add_argument("--preview", default=None)
    args = ap.parse_args()

    icon = load_user(args.src) if args.src else draw_default()
    flat = Image.new("RGB", icon.size, (0, 0, 0))      # man hinh nhac nen den: tron san len den
    flat.paste(icon, mask=icon.getchannel("A"))
    if args.preview:
        flat.save(args.preview)

    px = flat.load()
    vals = [((px[x, y][0] >> 3) << 11) | ((px[x, y][1] >> 2) << 5) | (px[x, y][2] >> 3)
            for y in range(SIZE) for x in range(SIZE)]
    lines = ["// Tao tu dong boi tools/make_music_icon.py - khong sua tay.",
             f"// Nguon: {os.path.basename(args.src) if args.src else 'bieu tuong mac dinh (ve bang code)'}",
             "#pragma once", "#include <Arduino.h>", "",
             f"static const int MUSIC_ICON_W = {SIZE}, MUSIC_ICON_H = {SIZE};",
             f"static const uint16_t MUSIC_ICON[{SIZE * SIZE}] PROGMEM = {{"]
    for i in range(0, len(vals), 16):
        lines.append("  " + ",".join(f"0x{v:04X}" for v in vals[i:i + 16]) + ",")
    lines.append("};")
    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("Da ghi", os.path.normpath(args.out))


if __name__ == "__main__":
    main()
