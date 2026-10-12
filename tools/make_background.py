#!/usr/bin/env python3
"""Chuyen anh nen (JPG/PNG) thanh mang RGB565 320x240 cho man hinh Core2, ghi ra src/bg_image.h.

Anh duoc cat ve ty le 4:3, thu ve 320x240 va lam toi (them gradient o phia duoi) de chu
trang tren no van doc ro.

Yeu cau: Pillow.   python tools/make_background.py [--src anh.jpg] [--crop-x 0.25] [--dim 0.55]
"""
import argparse
import glob
import os

from PIL import Image

W, H = 320, 240


def build(src: str, crop_x: float, dim_top: float, dim_bottom: float) -> Image.Image:
    im = Image.open(src).convert("RGB")
    sw, sh = im.size
    # Cat khung 4:3 cao bang anh goc; crop_x (0..1) chon vi tri khung theo chieu ngang
    cw = min(sw, round(sh * W / H))
    x0 = round((sw - cw) * max(0.0, min(1.0, crop_x)))
    im = im.crop((x0, 0, x0 + cw, sh)).resize((W, H), Image.LANCZOS)

    px = im.load()
    for y in range(H):
        k = dim_top + (dim_bottom - dim_top) * (y / (H - 1))   # toi dan xuong duoi
        for x in range(W):
            r, g, b = px[x, y]
            px[x, y] = (int(r * k), int(g * k), int(b * k))
    return im


def main() -> None:
    root = os.path.join(os.path.dirname(__file__), "..")
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=None, help="anh nguon (mac dinh: anh dau tien trong thu muc anh/)")
    ap.add_argument("--crop-x", type=float, default=0.25, help="0 = sat trai, 1 = sat phai")
    ap.add_argument("--dim", type=float, default=0.62, help="do sang phia tren (0..1)")
    ap.add_argument("--dim-bottom", type=float, default=0.42, help="do sang phia duoi (0..1)")
    ap.add_argument("--out", default=os.path.join(root, "src", "bg_image.h"))
    ap.add_argument("--preview", default=None, help="ghi them file PNG xem truoc")
    args = ap.parse_args()

    src = args.src
    if not src:
        cands = sorted(glob.glob(os.path.join(root, "anh", "*.jp*g")) + glob.glob(os.path.join(root, "anh", "*.png")))
        if not cands:
            raise SystemExit("Khong tim thay anh trong thu muc anh/")
        src = cands[0]

    im = build(src, args.crop_x, args.dim, args.dim_bottom)
    if args.preview:
        im.save(args.preview)

    px = im.load()
    vals = []
    for y in range(H):
        for x in range(W):
            r, g, b = px[x, y]
            vals.append(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))

    lines = [
        "// Tao tu dong boi tools/make_background.py - khong sua tay.",
        f"// Nguon: {os.path.basename(src)}  ({W}x{H}, RGB565)",
        "#pragma once",
        "#include <Arduino.h>",
        "",
        f"static const int BG_W = {W}, BG_H = {H};",
        f"static const uint16_t BG_IMAGE[{W * H}] PROGMEM = {{",
    ]
    for i in range(0, len(vals), 16):
        lines.append("  " + ",".join(f"0x{v:04X}" for v in vals[i:i + 16]) + ",")
    lines.append("};")
    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("Nguon:", src)
    print("Da ghi", os.path.normpath(args.out), f"({len(vals) * 2} byte du lieu)")


if __name__ == "__main__":
    main()
