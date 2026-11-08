#!/usr/bin/env python3
"""Generate small original Android TV launcher art without Pillow or game assets.

TV banner: 320x180 pixels, 16:9, xhdpi. TV icon: 160x160 pixels.
Run from any directory: python3 android/tools/make_tv_art.py.
No commercial artwork, ROMs, fonts, or other copyrighted files are used.
"""
from pathlib import Path
import struct
import zlib

# 5x7 uppercase bitmap glyphs; all letterforms authored for this project.
FONT = {
    " ": ["00000"] * 7,
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "N": ["10001", "11001", "10101", "10101", "10011", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
}

def image(width, height):
    buf = bytearray(width * height * 3)
    for y in range(height):
        for x in range(width):
            # Dark arcade cabinet background, with a subtle red gradient.
            vignette = abs(x - width // 2) * 22 // width
            value = max(0, 25 - vignette + (height - y) // 16)
            offset = (y * width + x) * 3
            buf[offset:offset+3] = bytes((value + 25, value // 2 + 4, value // 2 + 8))
    return buf

def rectangle(buf, width, height, x0, y0, x1, y1, rgb):
    x0, y0, x1, y1 = max(0, x0), max(0, y0), min(width, x1), min(height, y1)
    for y in range(y0, y1):
        for x in range(x0, x1):
            p = (y * width + x) * 3
            buf[p:p+3] = bytes(rgb)

def text_width(label, scale, tracking=1):
    return len(label) * (5 * scale + tracking * scale) - tracking * scale

def write_text(buf, w, h, label, y, scale, rgb, tracking=1):
    x0 = (w - text_width(label, scale, tracking)) // 2
    for char in label:
        if char not in FONT:
            raise ValueError("Unknown glyph: " + char)
        for iy, line in enumerate(FONT[char]):
            for ix, px in enumerate(line):
                if px == "1":
                    rectangle(buf, w, h, x0 + ix * scale, y + iy * scale,
                              x0 + (ix + 1) * scale, y + (iy + 1) * scale, rgb)
        x0 += (5 + tracking) * scale

def chunk(kind, payload):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(
        ">I", zlib.crc32(kind + payload) & 0xffffffff)

def write_png(path, width, height, pixels):
    path.parent.mkdir(parents=True, exist_ok=True)
    rows = b"".join(b"\x00" + bytes(pixels[y * width * 3:(y + 1) * width * 3])
                    for y in range(height))
    data = (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))
    path.write_bytes(data)
    print(f"Generated {path} ({width}x{height}, {len(data)} bytes)")

def main():
    res = Path(__file__).resolve().parents[1] / "app" / "src" / "main" / "res" / "drawable-xhdpi"

    w, h = 320, 180
    banner = image(w, h)
    rectangle(banner, w, h, 0, 0, w, 6, (205, 38, 38))
    rectangle(banner, w, h, 0, h - 7, w, h, (205, 38, 38))
    rectangle(banner, w, h, 16, 29, w - 16, 31, (110, 30, 35))
    rectangle(banner, w, h, 16, 92, w - 16, 94, (110, 30, 35))
    write_text(banner, w, h, "KILLER INSTINCT", 48, 3, (251, 235, 211))
    write_text(banner, w, h, "ARCADE ANDROID TV", 116, 2, (244, 107, 77))
    write_png(res / "ki_tv_banner.png", w, h, banner)

    w, h = 160, 160
    icon = image(w, h)
    rectangle(icon, w, h, 0, 0, w, 7, (205, 38, 38))
    rectangle(icon, w, h, 0, h - 8, w, h, (205, 38, 38))
    write_text(icon, w, h, "KI", 34, 9, (251, 235, 211))
    write_text(icon, w, h, "TV", 111, 4, (244, 107, 77))
    write_png(res / "ki_tv_icon.png", w, h, icon)

if __name__ == "__main__":
    main()
