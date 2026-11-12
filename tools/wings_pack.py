#!/usr/bin/env python3
"""wings_pack.py HDGRF_DIR OUT.grf: the wings' garment sets (3.0.18-beta.230, the owner's wings on the characters -
docs/superpowers/specs/2026-11-13-wings-design.md) out of the uaRO HD client's data.grf into one GRF 0x200 for
uaro-graphics\\wings.grf.

HDGRF_DIR: /root/uagfx-tools/hdgrf (it reads the HD client's zip on Google Drive by ranges; its tables cached there).
Taken: data\\sprite\\로브\\천사날개\\<sex>\\<body>.spr/.act (Angel Wings), 로브\\타락천사의날개\\... (Wings of Fallen
Angel) and data\\sprite\\robe\\darkwing.spr/.act (Dark Wing, one for every body). Named in the pack
angel\\<sex>\\<body>.<ext>, fallen\\..., dark.<ext> (core wings::garmentEntry); identical files share one block
(palettes_grf.write_grf), so the many bodies' copies of a picture cost it once.
"""
import os
import sys

if len(sys.argv) != 3:
    sys.exit(__doc__)
HD = sys.argv[1]
OUT = sys.argv[2]
sys.argv = [sys.argv[0], HD]  # grfany reads its folder from argv[1]
sys.path.insert(0, HD)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grfany  # noqa: E402 - the HD client's archives by HTTP ranges
import palettes_grf as grf  # noqa: E402 - the GRF writer of Palettes.grf

SETS = {"data\\sprite\\로브\\천사날개\\": "angel", "data\\sprite\\로브\\타락천사의날개\\": "fallen"}
DARK = "data\\sprite\\robe\\darkwing."


def pack_name(name):
    low = name.lower()
    if low.startswith(DARK) and low.endswith((".spr", ".act")):
        return "dark." + low[-3:]
    for prefix, short in SETS.items():
        if low.startswith(prefix):
            rest = name[len(prefix):]
            if rest.count("\\") == 1 and low.endswith((".spr", ".act")):  # <sex>\<body>.<ext>
                return short + "\\" + rest[:-4] + "." + low[-3:]
    return None


def main():
    base, entries = grfany.table("uaRO-HD/data.grf")
    files = {}
    for e in entries:
        name = pack_name(grfany.name(e))
        if name:
            files[name] = grfany.read(base, e)
    for need in ("dark.spr", "dark.act"):
        if need not in files:
            sys.exit("the HD client has no " + need)
    blocks = grf.write_grf(OUT, sorted(files.items()))
    raw = sum(len(d) for d in files.values())
    print("names %d, blocks %d (%d bytes before packing), %s %d bytes" % (len(files), blocks, raw, OUT,
                                                                           os.path.getsize(OUT)))


main()
