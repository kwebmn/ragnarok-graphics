#!/usr/bin/env python3
"""Draws src/app/app.ico, the settings app's icon: a monitor on a blue rounded square, 16, 32 and 48 pixels.

Run it again only to change the picture; the .ico is kept in git.
"""
import struct
import sys
from pathlib import Path

BLUE = (0xDF, 0x6C, 0x2D)  # BGR of #2D6CDF
DARK = (0x5A, 0x2E, 0x14)
WHITE = (0xFF, 0xFF, 0xFF)


def pixel(n, x, y):
    """The colour at (x, y) of an n x n icon, top row first; None = transparent."""
    u, v = (x + 0.5) / n, (y + 0.5) / n
    r = 0.18  # corner radius of the square
    cx = min(max(u, r), 1 - r)
    cy = min(max(v, r), 1 - r)
    if (u - cx) ** 2 + (v - cy) ** 2 > r * r:
        return None
    if 0.2 <= u <= 0.8 and 0.24 <= v <= 0.64:  # the screen with its frame
        return WHITE if 0.26 <= u <= 0.74 and 0.3 <= v <= 0.58 else DARK
    if 0.44 <= u <= 0.56 and 0.64 < v <= 0.74:  # the neck
        return DARK
    if 0.32 <= u <= 0.68 and 0.74 < v <= 0.8:  # the foot
        return DARK
    return BLUE


def image(n):
    rows = []
    mask_rows = []
    for y in range(n - 1, -1, -1):  # bottom-up, as BMP
        row = b""
        bits = 0
        mask = b""
        for x in range(n):
            c = pixel(n, x, y)
            row += bytes(c) + b"\xff" if c else b"\0\0\0\0"
            bits = (bits << 1) | (0 if c else 1)
            if x % 8 == 7:
                mask += bytes([bits])
                bits = 0
        mask += b"\0" * ((4 - len(mask) % 4) % 4)
        rows.append(row)
        mask_rows.append(mask)
    header = struct.pack("<IiiHHIIiiII", 40, n, n * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    return header + b"".join(rows) + b"".join(mask_rows)


def main(out):
    sizes = [16, 32, 48]
    images = [image(n) for n in sizes]
    data = struct.pack("<HHH", 0, 1, len(sizes))
    offset = 6 + 16 * len(sizes)
    for n, img in zip(sizes, images):
        data += struct.pack("<BBBBHHII", n, n, 0, 0, 1, 32, len(img), offset)
        offset += len(img)
    Path(out).write_bytes(data + b"".join(images))
    print("wrote", out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "src/app/app.ico")
