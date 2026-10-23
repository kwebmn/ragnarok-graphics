#!/usr/bin/env python3
"""Checks the release zip: its layout, install.bat (2.10: the settings app installs) and its Windows line endings.

The archive is unpacked into the uaRO client folder. Our ddraw.dll sits in uaro-graphics\ so unpacking never
overwrites dgVoodoo's DDraw.dll; install.bat moves the files into place.
"""
import re
import sys
import zipfile
from pathlib import Path

# 2.8: the notes and the template ini in uaro-graphics\ (core/layout); never uaro-graphics/uaro-graphics.ini - an
# older update.bat copies uaro-graphics\ of the archive over the installed one, and that path holds the player's
# settings now. uaro-flags.grf is the client's file.
EXPECTED = {"install.bat",  # 2.10: the only .bat - for an older update.bat, which runs it once
            "uaro-graphics/notes/TEST.en.md", "uaro-graphics/notes/TEST.uk.md", "uaro-graphics/notes/TEST.ru.md",
            "uaro-graphics/ddraw.dll", "uaro-graphics/uaro-graphics.exe",
            "uaro-graphics/defaults/uaro-graphics.ini", "uaro-graphics/version.txt", "uaro-graphics/uaro-render64.exe",
            "uaro-graphics/uaro-studio.exe",
            "uaro-graphics/dlss-version.txt", "uaro-graphics/models-version.txt", "uaro-graphics/remix/rtx.conf",
            "uaro-graphics/defaults/uaro-pad.ini",  # 2.9: uaRO Pad inside ddraw.dll, its template here
            # 3.0.1-beta.5: our own libraries in place of the client's old ones - put in the game's root by the app
            "uaro-graphics/clientlibs/cps.dll", "uaro-graphics/clientlibs/binkw32.dll"}
# 2.15: the AI models are in release/uaro-graphics-models.zip, not here
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
    # 2.10: install.bat hands the install to the settings app (app/modes --install); an update.bat of 0.11.0 or older
    # copied only the files of uaro-graphics\, so it still copies the folders such an update leaves out.
    install = z.read("install.bat").decode("ascii")
    if '"uaro-graphics\\uaro-graphics.exe" --install %1' not in install:
        fail("install.bat does not hand the install to uaro-graphics\\uaro-graphics.exe --install")
    if 'xcopy /e /i /y /q "uaro-graphics-update\\files\\uaro-graphics" "uaro-graphics"' not in install:
        fail("install.bat does not copy the folders an older update.bat leaves out")
    if len(install) > 1500:
        fail("install.bat is %d bytes: the install is the settings app's now" % len(install))
    # update.bat shows this line inside a ( ) block: no brackets or other cmd syntax, one CRLF line.
    stamp = z.read("uaro-graphics/version.txt")
    m = re.fullmatch(rb"(\d+\.\d+\.\d+(?:-[0-9A-Za-z.]+)?) build \d{4}-\d{2}-\d{2} \d{2}:\d{2}\r\n", stamp)
    if not m:
        fail("uaro-graphics/version.txt is %r, expected '<version> build YYYY-MM-DD HH:MM' and CRLF" % stamp)
    version = re.search(r'kVersion = "([^"]+)"', VERSION_CPP.read_text()).group(1)
    if m.group(1).decode() != version:
        fail("uaro-graphics/version.txt says %s, the source is %s" % (m.group(1).decode(), version))
    print("OK: %s: %s" % (path, ", ".join(sorted(names))))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build-win/uaro-graphics.zip")
