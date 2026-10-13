#!/usr/bin/env python3
"""Checks the cross-built uaro-graphics.exe, the settings app (stage 2 spec §3.1).

- It is a 32-bit Windows GUI program.
- It imports system DLLs of Windows 7 only, and no function Windows 7 lacks: those are found at run time.
- Its manifest asks for no administrator rights (asInvoker) and per-monitor DPI.
"""
import struct
import sys

ALLOWED_DLLS = {"kernel32.dll", "user32.dll", "gdi32.dll", "comctl32.dll", "shell32.dll", "ole32.dll", "advapi32.dll",
                "msvcrt.dll"}
# Newer than Windows 7: a static import of any of them would keep the program from starting there.
NOT_ON_WINDOWS_7 = {"GetDpiForWindow", "GetDpiForSystem", "SystemParametersInfoForDpi", "AdjustWindowRectExForDpi",
                    "SetThreadDpiAwarenessContext", "SetProcessDpiAwarenessContext", "GetDpiForMonitor",
                    "EnableNonClientDpiScaling", "GetSystemMetricsForDpi", "D3D12CreateDevice", "CreateDXGIFactory1",
                    "CreateDXGIFactory2"}


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
    if machine != 0x14C:
        fail("not a 32-bit (i386) program")
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] != 0x10B:
        fail("not PE32")
    if struct.unpack_from("<H", data, opt + 68)[0] != 2:
        fail("not a GUI program (subsystem %d)" % struct.unpack_from("<H", data, opt + 68)[0])
    imp_rva = struct.unpack_from("<I", data, opt + 104)[0]
    sections = [struct.unpack_from("<8sIIII", data, opt + optsize + i * 40) for i in range(nsec)]

    def off(rva):
        for _, vsize, va, rsize, raw in sections:
            if va <= rva < va + max(vsize, rsize):
                return rva - va + raw
        fail("rva %#x is outside the sections" % rva)

    def cstr(rva):
        o = off(rva)
        return data[o:data.index(b"\0", o)].decode()

    dlls, functions = [], []
    o = off(imp_rva)
    while struct.unpack_from("<I", data, o + 12)[0]:
        lookup, _, _, name, first = struct.unpack_from("<IIIII", data, o)
        dlls.append(cstr(name).lower())
        t = off(lookup or first)
        while True:
            entry = struct.unpack_from("<I", data, t)[0]
            if entry == 0:
                break
            if not entry & 0x80000000:
                functions.append(cstr(entry + 2))
            t += 4
        o += 20
    bad = [d for d in dlls if d not in ALLOWED_DLLS]
    if bad:
        fail("unexpected imports: " + ", ".join(bad))
    late = sorted(set(functions) & NOT_ON_WINDOWS_7)
    if late:
        fail("imports functions Windows 7 does not have: " + ", ".join(late))
    for needed in (b"asInvoker", b"PerMonitorV2", b"Microsoft.Windows.Common-Controls"):
        if needed not in data:
            fail("the manifest has no " + needed.decode())
    print("OK: %s: GUI, %d functions from %s, asInvoker, per-monitor DPI" % (path, len(functions), " ".join(dlls)))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build-win/uaro-graphics.exe")
