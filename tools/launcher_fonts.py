# 3.0.18-beta.193 (the launcher, docs/superpowers/specs/2026-11-12-launcher-design.md): the launcher's fonts -
# static instances of Google Fonts' variable Exo 2 and Cinzel, subset for the launcher's words
# (third_party/launcher_fonts). Run: python3 -I tools/launcher_fonts.py <Exo2[wght].ttf | Cinzel[wght].ttf> <dir>
import sys
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools import subset

src, out = sys.argv[1], sys.argv[2]
RANGES = [(0x20, 0x7E), (0xA0, 0x17F), (0x400, 0x45F), (0x490, 0x491), (0x2010, 0x2027), (0x2030, 0x203A),
          (0x20B4, 0x20B4), (0x2116, 0x2116), (0x2190, 0x2199), (0x25B2, 0x25BC), (0x2605, 0x2605),
          (0x2713, 0x2715)]
SPECIAL = "←→›‹▾▴·«»—–…★✓✕№’“”"

def make(path, weight, dest):
    f = TTFont(path)
    inst = instancer.instantiateVariableFont(f, {"wght": weight}, updateFontNames=True)
    opts = subset.Options()
    opts.layout_features = ["kern", "liga", "calt", "locl", "case", "mark", "mkmk"]
    opts.name_IDs = ["*"]
    opts.name_languages = ["*"]
    opts.notdef_outline = True
    opts.hinting = True
    sub = subset.Subsetter(opts)
    unis = [u for a, b in RANGES for u in range(a, b + 1)]
    sub.populate(unicodes=unis)
    sub.subset(inst)
    if "DSIG" in inst: del inst["DSIG"]
    inst.save(dest)
    t = TTFont(dest)
    names = {i: t["name"].getDebugName(i) for i in (1, 2, 4, 6, 16, 17)}
    cmap = t.getBestCmap()
    missing = [c for c in SPECIAL if ord(c) not in cmap]
    print(dest.split("/")[-1], t["OS/2"].usWeightClass, names, "missing:", "".join(missing))

if src.endswith("Exo2[wght].ttf"):
    for w, n in ((500, "Medium"), (600, "SemiBold"), (700, "Bold"), (800, "ExtraBold")):
        make(src, w, f"{out}/Exo2-{n}.ttf")
else:
    make(src, 700, f"{out}/Cinzel-Bold.ttf")
