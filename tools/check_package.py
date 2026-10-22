#!/usr/bin/env python3
"""Checks the release zip: the layout install.bat expects and Windows line endings in the .bat files.

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
EXPECTED = {"install.bat", "uninstall.bat", "update.bat", "get-dlss.bat", "get-models.bat",
            "uaro-graphics/notes/TEST.en.md", "uaro-graphics/notes/TEST.uk.md", "uaro-graphics/notes/TEST.ru.md",
            "uaro-graphics/ddraw.dll", "uaro-graphics/uaro-graphics.exe",
            "uaro-graphics/defaults/uaro-graphics.ini", "uaro-graphics/version.txt", "uaro-graphics/uaro-render64.exe",
            "uaro-graphics/uaro-studio.exe",
            "uaro-graphics/dlss-version.txt", "uaro-graphics/models-version.txt", "uaro-graphics/remix/rtx.conf",
            "uaro-graphics/ai/realesr-general-x4v3.bin", "uaro-graphics/ai/realesr-general-wdn-x4v3.bin",
            "uaro-graphics/ai/realesr-animevideov3.bin", "uaro-graphics/ai/LICENSE-Real-ESRGAN.txt",
            "uaro-graphics/ai/realesrgan-x4plus-anime-6b.bin",
            "uaro-graphics/defaults/uaro-pad.ini"}  # 2.9: uaRO Pad inside ddraw.dll, its template here
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
    # 0.11.0: update.bat copied only the files of uaro-graphics\, never its folders, so the AI model (ai\) and
    # rtx.conf (remix\) did not arrive. It copies the folders now; install.bat does it too for an update.bat older
    # than that, which runs the new install.bat while the unpacked update is still there.
    if any(n.count("/") > 1 for n in names if n.startswith("uaro-graphics/")):
        if not re.search(rb'(?im)^\s*xcopy /e /i /y /q "%work%\\files\\uaro-graphics" "uaro-graphics"\s',
                         z.read("update.bat")):
            fail('update.bat does not copy the folders of uaro-graphics\\ (xcopy /e /i /y /q ...)')
        if not re.search(rb'(?im)^\s*xcopy /e /i /y /q "uaro-graphics-update\\files\\uaro-graphics" "uaro-graphics"\s',
                         z.read("install.bat")):
            fail("install.bat does not copy the folders an older update.bat leaves out")
    # 2.8: a fresh install has no root uaro-graphics.ini: install.bat's additions to a 2.x one must not make one, and
    # dgVoodoo is kept in uaro-graphics\dgvoodoo.
    install = z.read("install.bat").decode("ascii")
    guard = install.find('if not exist "uaro-graphics.ini" goto :old_ini_done')
    first_edit = install.find('>>"uaro-graphics.ini"')
    if guard < 0 or (first_edit >= 0 and guard > first_edit):
        fail('install.bat adds to a root uaro-graphics.ini that a fresh install does not have (no :old_ini_done guard)')
    # 2.9: uaRO Pad is inside ddraw.dll - install.bat takes a 2.x pad dinput.dll away
    if 'findstr /m /c:"uaro-pad.log" "dinput.dll"' not in install or 'del /f "dinput.dll"' not in install:
        fail("install.bat does not take an old uaRO Pad dinput.dll away")
    if 'move /y "ddraw.dll" "uaro-graphics\\dgvoodoo\\DDraw.dll"' not in install:
        fail("install.bat does not keep dgVoodoo's DDraw.dll in uaro-graphics\\dgvoodoo")
    # GitHub's raw link is cached for 5 minutes (a query does not change that): right after a release update.bat got
    # the one before (0.11.2). It takes the archive of the newest commit by its number first.
    update = z.read("update.bat").decode("ascii")
    for part in ('"%accept%" "%api%"', 'findstr /x /r "[0-9a-f]*"', '"%raw%/%fresh%/release/uaro-graphics.zip"',
                 'set "accept=Accept: application/vnd.github.sha"'):
        if part not in update:
            fail("update.bat does not take the newest commit's archive: no " + part)
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
