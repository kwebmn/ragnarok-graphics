#!/usr/bin/env python3
"""emotion_flags.py OUT.grf SOURCE: uaro-flags.grf - the flags of Ctrl+1 and Ctrl+2 in the game's emotions drawn as
Ukraine's and the Right Sector's.

uaRO.exe (2007-05-07) under langtype 14 (sclientinfo.xml's connection to play.uaro.kiev.ua) sends emotion 66 for
Ctrl+1 and 67 for Ctrl+2 (its key handler, decomp line ~182640), and draws them with emotion.act's actions 65 and 66
(an emotion's action is its number less one from 35 on): a white-black-red flag, frames 169-177 of emotion.spr, and
the double-headed eagle's, frames 178-186 - the same nine poses of the waving cloth, in the same places. This script
draws the first as Ukraine's (blue over yellow) and the second as the Right Sector's (red and black, divided from the
hoist's foot to the fly's top), both on the first one's folds and shades, and writes emotion.spr alone into a GRF for
DATA.INI, above the client's own. Every other frame keeps its bytes. Where the cloth turns away from the viewer the
game paints its back in the flag's main colour (white here, green on Brazil's); these flags show their colours there
too, laid out along the pole.

SOURCE: the client's GRF 0x200 (UaRO.grf) or emotion.spr itself.
"""
import math
import os
import struct
import sys

sys.dont_write_bytecode = True  # no __pycache__ beside the tools
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import palettes_grf as grf  # noqa: E402 - the GRF reader and writer of Palettes.grf

SPR_NAME = 'data\\sprite\\이팩트\\emotion.spr'
POSES = [(39, 33), (45, 30), (25, 38), (22, 41), (32, 40), (36, 32), (37, 25), (31, 29), (31, 36)]
FIRST_UA, FIRST_RS = 169, 178  # Ctrl+1's and Ctrl+2's cloth frames

# The white-black-red flag's palette: its stripes, the pole and the finial (kept), anything else kept too.
WHITE = {1, 120, 121, 122, 123, 124, 125, 126, 127}
BLACK = {2, 164, 165, 167}
RED = {149, 150, 151, 53, 54, 95}
POLE = {70, 137, 138, 139, 140, 141, 72, 74, 76, 77, 78, 79, 199}
BASE = {'w': 121, 'r': 149}  # a stripe's plain colour: the others are its shades

UA_BLUE, UA_YELLOW = (0, 87, 183), (255, 215, 0)
RS_RED, RS_BLACK = (204, 20, 32), (26, 24, 26)
LEVELS = (0.5, 0.62, 0.74, 0.86, 1.0, 1.08, 1.16)  # the shades kept of each colour


def decode(raw, n):
    """A frame of SPR 2.1: zeros as runs (0, count), the rest as they are."""
    out, i = bytearray(), 0
    while i < len(raw):
        if raw[i] == 0:
            out += bytes(raw[i + 1])
            i += 2
        else:
            out.append(raw[i])
            i += 1
    if len(out) != n:
        raise ValueError('a damaged frame')
    return bytes(out)


def encode(px):
    out, i = bytearray(), 0
    while i < len(px):
        if px[i] == 0:
            run = 1
            while i + run < len(px) and px[i + run] == 0 and run < 255:
                run += 1
            out += bytes((0, run))
            i += run
        else:
            out.append(px[i])
            i += 1
    return bytes(out)


def read_spr(data):
    if data[:4] != b'SP\x01\x02':
        raise ValueError('no SPR 2.1')
    count, rgba = struct.unpack_from('<HH', data, 4)
    pos, frames = 8, []
    for _ in range(count):
        w, h, size = struct.unpack_from('<HHH', data, pos)
        frames.append([w, h, data[pos + 6:pos + 6 + size]])  # the bytes as written: kept where unchanged
        pos += 6 + size
    return frames, data[pos:-1024], rgba, bytearray(data[-1024:])


def write_spr(frames, rest, rgba, palette):
    out = bytearray(b'SP\x01\x02' + struct.pack('<HH', len(frames), rgba))
    for w, h, raw in frames:
        out += struct.pack('<HHH', w, h, len(raw)) + raw
    return bytes(out + rest + palette)


def lum(rgb):
    return 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2]


def shade(rgb, f):
    if f <= 1:
        return tuple(round(c * f) for c in rgb)
    return tuple(min(255, round(c + (255 - c) * (f - 1) * 1.5)) for c in rgb)


class Cloth:
    """One pose of the white-black-red flag: each cloth pixel's shade and place on the cloth - v from the top edge (0)
    to the foot (1), u from the pole (0) to the farthest pixel (1). Facing the viewer (its black stripe a fifth of the
    cloth at least) v comes of the stripes, following the folds; turned away (the back in one colour, bits of the
    stripes by the pole) v is the place along the pole."""

    def __init__(self, w, h, px, colour):
        self.w, self.h, self.px = w, h, px
        kind = {}
        for i, c in enumerate(px):
            kind[i] = 'w' if c in WHITE else 'k' if c in BLACK else 'r' if c in RED else 'p' if c in POLE else None
        self.cloth = [i for i, k in kind.items() if k in ('w', 'k', 'r')]
        at = {k: [(i % w, i // w) for i, kk in kind.items() if kk == k] for k in ('w', 'k', 'r', 'p')}
        outside = [(i % w, i // w) for i, c in enumerate(px) if c == 0 or kind[i] == 'p']
        self.front = len(at['k']) * 5 >= len(self.cloth)

        def near(x, y, pts, edge=False):
            d = min((((x - a) ** 2 + (y - b) ** 2) ** 0.5 for a, b in pts), default=1e9)
            return min(d, x + 1, y + 1, w - x, h - y) if edge else d

        # the pole's line: its pixels' middle and main direction, pointing down from the finial
        n = len(at['p']) or 1
        cx = sum(x for x, _ in at['p']) / n
        cy = sum(y for _, y in at['p']) / n
        sxx = sum((x - cx) ** 2 for x, _ in at['p'])
        syy = sum((y - cy) ** 2 for _, y in at['p'])
        sxy = sum((x - cx) * (y - cy) for x, y in at['p'])
        angle = 0.5 * math.atan2(2 * sxy, sxx - syy)
        ex, ey = math.cos(angle), math.sin(angle)
        if ey < 0:
            ex, ey = -ex, -ey
        along = {i: (i % w - cx) * ex + (i // w - cy) * ey for i in self.cloth}
        across = {i: abs((i % w - cx) * ey - (i // w - cy) * ex) for i in self.cloth}
        lo, hi = min(along.values(), default=0), max(along.values(), default=1)
        wide = max(across.values(), default=1) or 1

        self.v, self.u, self.f = {}, {}, {}
        for i in self.cloth:
            x, y = i % w, i // w
            k = kind[i]
            self.u[i] = across[i] / wide
            self.f[i] = 1.0 if k == 'k' else lum(colour[px[i]]) / lum(colour[BASE[k]])
            if not self.front:
                self.v[i] = (along[i] - lo) / ((hi - lo) or 1)
                continue
            out = near(x, y, outside, True)
            dw, dk, dr = near(x, y, at['w']), near(x, y, at['k']), near(x, y, at['r'])
            if k == 'w':
                v = out / (out + (dk if at['k'] else dr)) / 3
            elif k == 'k':
                v = 1 / 3 + (dw / (dw + (dr if at['r'] else out))) / 3
            else:
                above = dk if at['k'] else dw
                v = 2 / 3 + (above / (above + out)) / 3
            self.v[i] = min(1.0, max(0.0, v))


def level(f):
    return min(LEVELS, key=lambda x: abs(x - f))


def redraw(cloth, pick, slot):
    """The pose's pixels with each cloth pixel's new colour (pick: index -> base colour) at its shade."""
    out = bytearray(cloth.px)
    for i in cloth.cloth:
        out[i] = slot[(pick(i), level(cloth.f[i]))]
    return bytes(out)


def main(argv):
    if len(argv) != 3:
        sys.exit(__doc__)
    out_path, source = argv[1], argv[2]
    if source.lower().endswith('.grf'):
        entries = grf.grf_entries(source)
        name = next((n for n in entries if n.lower() == SPR_NAME.lower()), None)
        if not name:
            sys.exit(source + ' has no ' + SPR_NAME)
        with open(source, 'rb') as f:
            data = grf.grf_read(f, entries[name])
    else:
        with open(source, 'rb') as f:
            data = f.read()
    frames, rest, rgba, palette = read_spr(data)
    for k, size in enumerate(POSES):
        for first in (FIRST_UA, FIRST_RS):
            if tuple(frames[first + k][:2]) != size:
                sys.exit('another emotion.spr: frame %d is %dx%d, not %dx%d' % ((first + k,) + tuple(frames[first + k][:2]) + size))
    colour = [tuple(palette[i * 4:i * 4 + 3]) for i in range(256)]
    used = set()
    for w, h, raw in frames:
        used |= set(decode(raw, w * h))
    free = [i for i in range(1, 256) if i not in used]
    wanted = [(c, f) for c in (UA_BLUE, UA_YELLOW, RS_RED, RS_BLACK) for f in LEVELS]
    if len(free) < len(wanted):
        sys.exit('the palette has %d free colours, %d wanted' % (len(free), len(wanted)))
    slot = {}
    for index, (c, f) in zip(free, wanted):
        slot[(c, f)] = index
        palette[index * 4:index * 4 + 4] = bytes(shade(c, f) + (0,))
    for k in range(len(POSES)):
        w, h, raw = frames[FIRST_UA + k]
        cloth = Cloth(w, h, decode(raw, w * h), colour)
        ua = redraw(cloth, lambda i: UA_BLUE if cloth.v[i] < 0.5 else UA_YELLOW, slot)
        rs = redraw(cloth, lambda i: RS_RED if cloth.u[i] + cloth.v[i] < 1 else RS_BLACK, slot)
        frames[FIRST_UA + k][2] = encode(ua)
        frames[FIRST_RS + k][2] = encode(rs)
    spr = write_spr(frames, rest, rgba, palette)
    grf.write_grf(out_path, [(SPR_NAME, spr)])
    # read back: the GRF's entry is this emotion.spr, each frame whole, the other frames as they were
    entries = grf.grf_entries(out_path)
    with open(out_path, 'rb') as f:
        back = grf.grf_read(f, entries[SPR_NAME])
    if back != spr:
        sys.exit('the GRF does not read back')
    again, _, _, _ = read_spr(back)
    old, _, _, _ = read_spr(data)
    for i, (w, h, raw) in enumerate(again):
        decode(raw, w * h)
        if not FIRST_UA <= i < FIRST_RS + len(POSES) and raw != old[i][2]:
            sys.exit('frame %d changed' % i)
    print('%s: emotion.spr with Ctrl+1 Ukraine, Ctrl+2 the Right Sector; %d of %d free colours taken' %
          (out_path, len(wanted), len(free)))


if __name__ == '__main__':
    main(sys.argv)
