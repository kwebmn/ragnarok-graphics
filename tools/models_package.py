#!/usr/bin/env python3
"""Builds build-models/uaro-graphics-models.zip (2.5.27): the AI models that come apart from the release - too large
for it. 2.15: all of them - Real-ESRGAN's of dist/ai (the AI textures in the game) and Game AI 2.0 (making pictures);
the settings app brings it into uaro-graphics/ai/ (an update with the AI textures on, or the AI textures page).

Game AI 2.0 (4x ESRGAN, 23 blocks; by Tal, WTFPL - openmodeldb.info/models/4x-GameAI-2-0, the file
drive.google.com/uc?export=download&confirm=1&id=15KhApV05F8nEqTa5ZjLE821rhe1k8VOg), checked against the hash below
and written by tools/ai_rrdb.py --half: 33 MB instead of 67, the 8-bit pictures within 2 levels.
Usage: python3 -I tools/models_package.py <4x_GameAI_2.0.pth | an earlier uaro-graphics-models.zip>
"""
import hashlib
import os
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ai_rrdb  # noqa: E402

VERSION = "models-2 1"  # also in dist/models-version.txt: the settings app downloads when the installed one differs
REAL_ESRGAN = ["realesr-general-x4v3.bin", "realesr-general-wdn-x4v3.bin", "realesr-animevideov3.bin",
               "realesrgan-x4plus-anime-6b.bin", "LICENSE-Real-ESRGAN.txt"]
GAMEAI = "3e21c3cebb50e647644e9d1dd6bab4d49cbdaa4d4b14c4eaafa1670d8a9f15ef"  # the original .pth (66,961,958 bytes)
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build-models"


def main(source):
    if (ROOT / "dist" / "models-version.txt").read_bytes() != VERSION.encode() + b"\r\n":
        sys.exit("FAIL: dist/models-version.txt is not \"%s\"" % VERSION)
    stage = OUT / "package" / "uaro-graphics" / "ai"
    stage.mkdir(parents=True, exist_ok=True)
    if source.endswith(".zip"):  # 2.15: Game AI as an earlier models' archive made it
        with zipfile.ZipFile(source) as z:
            (stage / "4x-gameai-2.0.bin").write_bytes(z.read("uaro-graphics/ai/4x-gameai-2.0.bin"))
    else:
        if hashlib.sha256(Path(source).read_bytes()).hexdigest() != GAMEAI:
            sys.exit("FAIL: %s is not Game AI 2.0's file this package was made from" % source)
        ai_rrdb.main(source, str(stage / "4x-gameai-2.0.bin"), half=True)
    for name in REAL_ESRGAN:
        (stage / name).write_bytes((ROOT / "dist" / "ai" / name).read_bytes())
    (stage / "LICENSE-GameAI.txt").write_bytes((ROOT / "dist" / "ai" / "LICENSE-GameAI.txt").read_bytes())
    (stage / "models-version.txt").write_bytes(VERSION.encode() + b"\r\n")
    out = OUT / "uaro-graphics-models.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for f in sorted(stage.iterdir()):
            z.write(f, "uaro-graphics/ai/" + f.name)
    print("packed %s (%d MB): %s" % (out, out.stat().st_size >> 20, VERSION))


if __name__ == "__main__":
    main(sys.argv[1])
