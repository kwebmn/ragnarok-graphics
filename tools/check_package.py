#!/usr/bin/env python3
"""Checks the release zip: the layout install.bat expects and Windows line endings in the .bat files.

The archive is unpacked into the uaRO client folder. Our ddraw.dll sits in uaro-graphics\ so unpacking never
overwrites dgVoodoo's DDraw.dll; install.bat moves the files into place.
"""
import re
import sys
import zipfile
from pathlib import Path

EXPECTED = {"install.bat", "uninstall.bat", "use-uaro.bat", "use-dgvoodoo.bat", "update.bat", "TEST.ru.md",
            "uaro-graphics/ddraw.dll", "uaro-graphics/uaro-graphics.exe", "uaro-graphics/uaro-graphics.ini",
            "uaro-graphics/version.txt"}
VERSION_CPP = Path(__file__).resolve().parent.parent / "src" / "core" / "version.cpp"


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def main(path):
    try:
        z = zipfile.ZipFile(path)
    except (OSError, zipfile.BadZipFile) as e:
        fail("%s: %s" % (path, e))
    names = {n for n in z.namelist() if not n.endswith("/")}
    if names != EXPECTED:
        fail("layout %s, expected %s" % (sorted(names), sorted(EXPECTED)))
    for bat in sorted(n for n in names if n.endswith(".bat")):
        if b"\n" in z.read(bat).replace(b"\r\n", b""):
            fail(bat + " has LF line endings: cmd.exe needs CRLF")
    # update.bat shows this line inside a ( ) block: no brackets or other cmd syntax, one CRLF line.
    stamp = z.read("uaro-graphics/version.txt")
    m = re.fullmatch(rb"(\d+\.\d+\.\d+) build \d{4}-\d{2}-\d{2} \d{2}:\d{2}\r\n", stamp)
    if not m:
        fail("uaro-graphics/version.txt is %r, expected '<version> build YYYY-MM-DD HH:MM' and CRLF" % stamp)
    version = re.search(r'kVersion = "([^"]+)"', VERSION_CPP.read_text()).group(1)
    if m.group(1).decode() != version:
        fail("uaro-graphics/version.txt says %s, the source is %s" % (m.group(1).decode(), version))
    print("OK: %s: %s" % (path, ", ".join(sorted(names))))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build-win/uaro-graphics.zip")
