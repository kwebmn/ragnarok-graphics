#!/usr/bin/env python3
"""Checks the cross-built uaro-studio.exe, uaRO Studio (2.3): a 64-bit Windows GUI program linked statically - it
imports from Windows' own DLLs alone (a MinGW runtime DLL would be missing on a player's computer) - whose manifest
asks for no administrator rights and per-monitor DPI."""
import struct
import sys

SYSTEM = {"kernel32.dll", "user32.dll", "gdi32.dll", "comctl32.dll", "comdlg32.dll", "shell32.dll", "ole32.dll",
          "advapi32.dll", "msvcrt.dll", "uxtheme.dll"}


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def main(path):
    try:
        data = open(path, "rb").read()
    except OSError:
        fail(path + " not found")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        fail("not a PE file")
    machine, nsec = struct.unpack_from("<HH", data, pe + 4)
    if machine != 0x8664:
        fail("not a 64-bit (x64) program")
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] != 0x20B:
        fail("not PE32+")
    if struct.unpack_from("<H", data, opt + 68)[0] != 2:
        fail("not a GUI program")
    imp_rva = struct.unpack_from("<I", data, opt + 120)[0]
    sections = [struct.unpack_from("<8sIIII", data, opt + optsize + i * 40) for i in range(nsec)]

    def off(rva):
        for _, vsize, va, rsize, raw in sections:
            if va <= rva < va + max(vsize, rsize):
                return rva - va + raw
        fail("rva %#x is outside the sections" % rva)

    dlls = []
    o = off(imp_rva)
    while struct.unpack_from("<I", data, o + 12)[0]:
        name = struct.unpack_from("<I", data, o + 12)[0]
        at = off(name)
        dlls.append(data[at:data.index(b"\0", at)].decode().lower())
        o += 20
    bad = [d for d in dlls if d not in SYSTEM]
    if bad:
        fail("imports from DLLs that are not Windows' own: " + ", ".join(bad))
    for needed in (b"asInvoker", b"PerMonitorV2", b"Microsoft.Windows.Common-Controls"):
        if needed not in data:
            fail("the manifest has no " + needed.decode())
    print("OK: %s: 64-bit GUI, imports %s, asInvoker, per-monitor DPI" % (path, " ".join(sorted(dlls))))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build-win64/uaro-studio.exe")
