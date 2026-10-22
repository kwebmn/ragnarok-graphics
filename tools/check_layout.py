#!/usr/bin/env python3
"""Checks that the places of our files in the game's folder are named in src/core/layout.cpp only (2.8).

A path joined by hand ("\\uaro-graphics.ini", "\\uaro-textures.zip", ...) would keep a file in the game's root
after 2.8 moved it into uaro-graphics\\ - every program asks core/layout instead.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TABLE = ROOT / "src" / "core" / "layout.cpp"
OURS = re.compile(r'"\\\\(uaro-graphics\.ini|uaro-graphics\.log|uaro-render64\.log|uaro-pack\.log|'
                  r'uaro-studio-engine\.log|uaro-graphics-pipelines\.txt|uaro-graphics-report\.txt|'
                  r'uaro-textures\.zip|uaro-graphics-backup|TEST\.)'
                  r'|uaro-graphics\\\\(shaders-|studio-shaders-)')


def main():
    found = []
    for path in sorted((ROOT / "src").rglob("*")):
        if path.suffix not in (".cpp", ".hpp") or path == TABLE:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if OURS.search(line):
                found.append("%s:%d: %s" % (path.relative_to(ROOT), number, line.strip()))
    if found:
        print("FAIL: a path to a file of ours outside src/core/layout.cpp:\n" + "\n".join(found))
        return 1
    print("OK: no path to a file of ours outside src/core/layout.cpp")
    return 0


if __name__ == "__main__":
    sys.exit(main())
