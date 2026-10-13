#!/usr/bin/env python3
"""Checks the release zip: the layout install.bat expects and Windows line endings in the .bat files.

The archive is unpacked into the uaRO client folder. Our ddraw.dll sits in uaro-graphics\ so unpacking never
overwrites dgVoodoo's DDraw.dll; install.bat moves the files into place.
"""
import sys
import zipfile

EXPECTED = {"install.bat", "uninstall.bat", "TEST.ru.md", "uaro-graphics/ddraw.dll", "uaro-graphics/uaro-graphics.ini"}


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
    for bat in ("install.bat", "uninstall.bat"):
        if b"\n" in z.read(bat).replace(b"\r\n", b""):
            fail(bat + " has LF line endings: cmd.exe needs CRLF")
    print("OK: %s: %s" % (path, ", ".join(sorted(names))))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build-win/uaro-graphics-0.1.1.zip")
