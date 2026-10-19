#!/usr/bin/env python3
"""Builds build-models/uaro-graphics-models.zip (2.5.27): the AI models that come apart from the release - too large
for it, and wanted only for making pictures (the texture pack, [ai] generate). get-models.bat unpacks it into the
uaRO client folder: uaro-graphics/ai/.

Game AI 2.0 (4x ESRGAN, 23 blocks; by Tal, WTFPL - openmodeldb.info/models/4x-GameAI-2-0, the file
drive.google.com/uc?export=download&confirm=1&id=15KhApV05F8nEqTa5ZjLE821rhe1k8VOg), checked against the hash below
and written by tools/ai_rrdb.py --half: 33 MB instead of 67, the 8-bit pictures within 2 levels.
Usage: python3 -I tools/models_package.py <4x_GameAI_2.0.pth>
"""
import hashlib
import os
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ai_rrdb  # noqa: E402

VERSION = "gameai-2.0 1"  # also in dist/models-version.txt: get-models.bat downloads when it differs
GAMEAI = "3e21c3cebb50e647644e9d1dd6bab4d49cbdaa4d4b14c4eaafa1670d8a9f15ef"  # the original .pth (66,961,958 bytes)
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build-models"


def main(pth):
    if hashlib.sha256(Path(pth).read_bytes()).hexdigest() != GAMEAI:
        sys.exit("FAIL: %s is not Game AI 2.0's file this package was made from" % pth)
    if (ROOT / "dist" / "models-version.txt").read_bytes() != VERSION.encode() + b"\r\n":
        sys.exit("FAIL: dist/models-version.txt is not \"%s\"" % VERSION)
    stage = OUT / "package" / "uaro-graphics" / "ai"
    stage.mkdir(parents=True, exist_ok=True)
    ai_rrdb.main(pth, str(stage / "4x-gameai-2.0.bin"), half=True)
    (stage / "LICENSE-GameAI.txt").write_bytes((ROOT / "dist" / "ai" / "LICENSE-GameAI.txt").read_bytes())
    (stage / "models-version.txt").write_bytes(VERSION.encode() + b"\r\n")
    out = OUT / "uaro-graphics-models.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for f in sorted(stage.iterdir()):
            z.write(f, "uaro-graphics/ai/" + f.name)
    print("packed %s (%d MB): %s" % (out, out.stat().st_size >> 20, VERSION))


if __name__ == "__main__":
    main(sys.argv[1])
