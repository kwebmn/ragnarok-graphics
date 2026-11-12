#!/usr/bin/env python3
"""3.0.18-beta.203 (the owner: «лого на картинке с заставки самого клиента взять»): the launcher's logo cut out of
the client's own login picture - data\\texture\\유저인터페이스\\bgi_temp.bmp in uaRO HD's uaro.grf (1672 x 941).

The sky behind the logo is filled in from the pixels around it (a harmonic fill at a quarter of the size), then each
pixel gets the least alpha that explains it over that sky (a light logo on a dark sky); stars and clouds outside the
logo's parts are dropped. Out: dist/launcher/logo.webp (491 x 206, straight alpha).

    python3 tools/launcher_logo.py bgi_temp.bmp dist/launcher/logo.webp
"""
import sys
from collections import deque

from PIL import Image, ImageFilter

CROP = (850, 36, 1352, 252)  # the logo in bgi_temp.bmp
# its parts in the crop: the crystal's ornament, the wings, the letters, the line and «RAGNAROK ONLINE»
PARTS = [(140, 4, 378, 98), (4, 52, 112, 194), (424, 52, 500, 194), (66, 70, 440, 186), (80, 176, 425, 215)]
GROW, FEATHER, HAZE, Q = 11, 2, 0.14, 4


def lum(c):
    return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]


def components(on, w, h):
    seen = [0] * (w * h)
    for start in range(w * h):
        if not on[start] or seen[start]:
            continue
        comp, q = [], deque([start])
        seen[start] = 1
        while q:
            i = q.popleft()
            comp.append(i)
            x, y = i % w, i // w
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and on[ny * w + nx] and not seen[ny * w + nx]:
                    seen[ny * w + nx] = 1
                    q.append(ny * w + nx)
        yield comp


def main(src, out):
    im = Image.open(src).convert("RGB").crop(CROP)
    W, H = im.size
    px = list(im.get_flattened_data())
    # a first sky: the opening at a quarter, wide enough to drop the letters and the crystal
    small = im.resize((W // Q, H // Q), Image.BOX).filter(ImageFilter.MinFilter(15)).filter(ImageFilter.MaxFilter(15))
    sky0 = list(small.resize((W, H), Image.BILINEAR).filter(ImageFilter.GaussianBlur(6)).get_flattened_data())
    # the logo's core: clearly lighter than that sky, in the logo's parts, not touching the crop's edges
    lit = [1 if lum(px[i]) - lum(sky0[i]) > 38 else 0 for i in range(W * H)]
    keep = [0] * (W * H)
    for comp in components(lit, W, H):
        xs, ys = [i % W for i in comp], [i // W for i in comp]
        if min(xs) <= 1 or max(xs) >= W - 2 or min(ys) <= 1 or max(ys) >= H - 2 or len(comp) < 14:
            continue
        cx, cy = sum(xs) / len(xs), sum(ys) / len(ys)
        if any(r[0] <= cx <= r[2] and r[1] <= cy <= r[3] for r in PARTS):
            for i in comp:
                keep[i] = 1
    core = Image.new("L", (W, H))
    core.putdata([255 * k for k in keep])
    # the sky filled in over the core grown by its glow, from the sky around it
    region = core.filter(ImageFilter.MaxFilter(GROW))
    rq = list(region.resize((W // Q, H // Q), Image.BOX).filter(ImageFilter.MaxFilter(3)).get_flattened_data())
    skyQ = im.filter(ImageFilter.MedianFilter(5)).resize((W // Q, H // Q), Image.BOX)
    w, h = skyQ.size
    sq = [list(c) for c in skyQ.get_flattened_data()]
    inside = [i for i in range(w * h) if rq[i] > 0]
    outside = [i for i in range(w * h) if rq[i] == 0]
    mean = [sum(sq[i][c] for i in outside) / max(1, len(outside)) for c in range(3)]
    for i in inside:
        sq[i] = list(mean)
    for _ in range(1500):
        for i in inside:
            x, y = i % w, i // w
            acc, n = [0.0, 0.0, 0.0], 0
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h:
                    c = sq[ny * w + nx]
                    acc = [acc[0] + c[0], acc[1] + c[1], acc[2] + c[2]]
                    n += 1
            sq[i] = [a / n for a in acc]
    skyImg = Image.new("RGB", (w, h))
    skyImg.putdata([tuple(int(round(v)) for v in c) for c in sq])
    sky = list(skyImg.resize((W, H), Image.BILINEAR).filter(ImageFilter.GaussianBlur(3)).get_flattened_data())
    # the least alpha over that sky, the sky's grain cut, only in the region
    feather = list(region.filter(ImageFilter.GaussianBlur(FEATHER)).get_flattened_data())
    alpha, colour = [0] * (W * H), [(0, 0, 0)] * (W * H)
    for i in range(W * H):
        I, B = px[i], sky[i]
        a = 0.0
        for c in range(3):
            if I[c] > B[c]:
                a = max(a, (I[c] - B[c]) / max(1.0, 255.0 - B[c]))
            elif I[c] < B[c]:
                a = max(a, (B[c] - I[c]) / max(1.0, B[c]))
        a = min(1.0, max(0.0, (a - HAZE) / (1 - HAZE))) * (feather[i] / 255.0)
        if a >= 1.0 / 255:
            alpha[i] = int(round(a * 255))
            colour[i] = tuple(int(round(min(255.0, max(0.0, B[c] + (I[c] - B[c]) / a)))) for c in range(3))
    # only what hangs together with the core: lone stars and cloud bits dropped; the faint rest is the sky's grain
    for comp in components([1 if a > 6 else 0 for a in alpha], W, H):
        if not any(keep[i] for i in comp):
            for i in comp:
                alpha[i] = 0
    rgba = Image.new("RGBA", (W, H))
    rgba.putdata([colour[i] + (alpha[i],) if alpha[i] > 6 else (0, 0, 0, 0) for i in range(W * H)])
    x0, y0, x1, y1 = rgba.getchannel("A").getbbox()
    rgba = rgba.crop((x0 - 2, y0 - 2, x1 + 2, y1 + 2))
    rgba.save(out, "WEBP", quality=92, alpha_quality=100, method=6)
    print(out, rgba.size)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
