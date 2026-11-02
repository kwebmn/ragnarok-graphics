#!/usr/bin/env python3
"""class_art.py SITE_ART_DIR OUT_DIR: the login screen's class pictures (3.0.11-beta.178, the login concept's cards).

The site's 1280 x 1024 class pictures (uaro.kiev.ua docs/design/assets/art/*.png: the character at the right, the
site's words at the left) cropped to 360 x 360 portraits - each its own square around the face (the face 38 % under
the square's top: the shelf's cards show its upper part), JPEG of 40 KB at most. They go into ddraw.dll as resources
(src/win/ui_font.rc); core/class_art names them by the job.
"""
import io
import sys
from pathlib import Path

from PIL import Image

SIDE = 360
MOST = 40 * 1024
# name: the face's centre in the site's picture and the square's side (its pixels)
FACES = {
    "assassin-cross": (960, 330, 640), "champion": (960, 300, 760), "clown": (920, 210, 600),
    "creator": (900, 280, 600), "gunslinger": (930, 210, 600), "gypsy": (930, 220, 600),
    "high-priest": (1075, 240, 600), "high-wizard": (1060, 300, 640), "lord-knight": (900, 280, 620),
    "ninja": (900, 200, 600), "paladin": (830, 280, 640), "professor": (1080, 130, 560),
    "sniper": (900, 340, 640), "soul-linker": (1100, 300, 600), "stalker": (930, 220, 600),
    "star-gladiator": (980, 250, 620), "whitesmith": (1050, 600, 300),
}


def portrait(path, face):
    fx, fy, side = face
    im = Image.open(path).convert("RGB")
    left = min(max(0, fx - side // 2), im.width - side)
    top = min(max(0, fy - int(side * 0.38)), im.height - side)
    return im.crop((left, top, left + side, top + side)).resize((SIDE, SIDE), Image.LANCZOS)


def jpeg(im):
    for q in range(88, 40, -4):
        b = io.BytesIO()
        im.save(b, "JPEG", quality=q, optimize=True, progressive=False)
        if b.tell() <= MOST:
            return b.getvalue()
    raise SystemExit("a portrait over %d bytes" % MOST)


def main(src, out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    for name, face in sorted(FACES.items()):
        data = jpeg(portrait(Path(src) / (name + ".png"), face))
        (out / (name + ".jpg")).write_bytes(data)
        print("%s.jpg %d bytes" % (name, len(data)))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
