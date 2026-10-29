#!/usr/bin/env python3
"""Writes dist/walkable.bin (3.0.11-beta.8): the server's maps cell by cell - which cells one walks on and which parts
of a map are reachable on foot from each other - for the routes of «Довідник» (src/core/walk_grid, reference_route).

  walk_grids.py GAT_FOLDER [OUT]      (the server's maps: /root/uAthenaGRF/GRF/data; OUT dist/walkable.bin)
  walk_grids.py --self-test

A map's .gat: "GRAT", two version bytes, its width and height (int32), then 20 bytes a cell - four heights and the
type (uint32). Types 1 and 5 are not walked on (the server's map_gat2cell: 1 nothing, 5 a cliff one shoots over).
A part: walkable cells joined side by side (the server's paths cut no corners, so diagonals add nothing).

The file is gzip (no time in it) of: "UAWALK1\\n", the maps' count (uint32), then each map by name: its name's length
(uint8) and name, its width and height (uint16), its runs' count (uint32) and the runs row by row from y 0 - a run is
a length (uint16) and a part (uint8; 255 not walkable; parts past 254 counted as 254).
"""
import gzip
import os
import struct
import sys

WALL = 255
MAGIC = b"UAWALK1\n"


def parts(w, h, walkable):
    """Labels of the 4-connected walkable parts, -1 for a wall; and how many parts."""
    label = [-1] * (w * h)
    n = 0
    for start in range(w * h):
        if not walkable[start] or label[start] >= 0:
            continue
        label[start] = n
        stack = [start]
        while stack:
            c = stack.pop()
            x, y = c % w, c // w
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h:
                    j = ny * w + nx
                    if walkable[j] and label[j] < 0:
                        label[j] = n
                        stack.append(j)
        n += 1
    return label, n


def runs(w, h, label):
    out = []
    for y in range(h):
        row = label[y * w:(y + 1) * w]
        x = 0
        while x < w:
            v, s = row[x], x
            while x < w and row[x] == v:
                x += 1
            out.append((x - s, WALL if v < 0 else min(v, 254)))
    return out


def read_gat(data):
    if data[:4] != b"GRAT" or len(data) < 14:
        raise ValueError("not a .gat")
    w, h = struct.unpack("<ii", data[6:14])
    if w <= 0 or h <= 0 or w > 65535 or h > 65535 or len(data) < 14 + w * h * 20:
        raise ValueError("a .gat of %dx%d with %d bytes" % (w, h, len(data)))
    walkable = bytearray(w * h)
    for i in range(w * h):
        walkable[i] = 0 if data[14 + i * 20 + 16] in (1, 5) else 1
    return w, h, walkable


def payload(maps):
    """maps: name -> (w, h, runs)"""
    out = bytearray(MAGIC + struct.pack("<I", len(maps)))
    for name in sorted(maps):
        w, h, rs = maps[name]
        raw = name.encode("ascii")
        out += struct.pack("<B", len(raw)) + raw + struct.pack("<HHI", w, h, len(rs))
        for length, part in rs:
            out += struct.pack("<HB", length, part)
    return bytes(out)


def parse(data):
    """The payload back: name -> (w, h, runs)"""
    if data[:8] != MAGIC:
        raise ValueError("not a walkable payload")
    (count,) = struct.unpack_from("<I", data, 8)
    at, maps = 12, {}
    for _ in range(count):
        n = data[at]
        name = data[at + 1:at + 1 + n].decode("ascii")
        at += 1 + n
        w, h, k = struct.unpack_from("<HHI", data, at)
        at += 8
        maps[name] = (w, h, [struct.unpack_from("<HB", data, at + 3 * i) for i in range(k)])
        at += 3 * k
    return maps


def build(folder, out):
    maps = {}
    for f in sorted(os.listdir(folder)):
        if not f.lower().endswith(".gat"):
            continue
        with open(os.path.join(folder, f), "rb") as g:
            w, h, walkable = read_gat(g.read())
        label, n = parts(w, h, walkable)
        maps[f[:-4].lower()] = (w, h, runs(w, h, label))
    data = gzip.compress(payload(maps), 9, mtime=0)
    with open(out, "wb") as g:
        g.write(data)
    print("OK: %s: %d maps, %d bytes" % (out, len(maps), len(data)))


def self_test():
    # 5x3: a wall column at x 2 splits two parts; a cliff (5) at (4, 2)
    w, h = 5, 3
    types = [0, 0, 1, 0, 0,
             0, 0, 1, 0, 0,
             0, 3, 1, 0, 5]
    gat = b"GRAT\x01\x02" + struct.pack("<ii", w, h) + b"".join(struct.pack("<ffffI", 0, 0, 0, 0, t) for t in types)
    gw, gh, walkable = read_gat(gat)
    assert (gw, gh) == (5, 3) and walkable[11] == 1 and walkable[12] == 0 and walkable[14] == 0
    label, n = parts(gw, gh, walkable)
    assert n == 2 and label[0] == 0 and label[3] == 1 and label[2] == -1 and label[14] == -1
    rs = runs(gw, gh, label)
    assert rs == [(2, 0), (1, WALL), (2, 1), (2, 0), (1, WALL), (2, 1), (2, 0), (1, WALL), (1, 1), (1, WALL)], rs
    back = parse(payload({"tiny": (gw, gh, rs)}))
    assert back == {"tiny": (5, 3, rs)}, back
    print("OK: walk_grids self-test")


if __name__ == "__main__":
    if sys.argv[1:] == ["--self-test"]:
        self_test()
    elif len(sys.argv) in (2, 3):
        build(sys.argv[1], sys.argv[2] if len(sys.argv) == 3 else "dist/walkable.bin")
    else:
        print(__doc__)
        sys.exit(2)
