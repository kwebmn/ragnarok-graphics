#!/usr/bin/env python3
"""Checks the cross-built ddraw.dll.

- It is a 32-bit DLL, like uaRO.exe.
- It exports everything the system ddraw.dll exports (22 functions), forwarding them to the real DirectDraw.
- DirectDrawCreateEx starts with the hot-patch prologue: ROExt inline-hooks it.
- It imports system DLLs only: d3d12.dll and dxgi.dll are loaded at run time, so the DLL loads on Windows 7.
"""
import struct
import sys

# All 22 exports of the system ddraw.dll: d3dim700.dll (system) and dgVoodoo's D3DImm.dll find the private ones
# by the name "ddraw.dll", which is us once we are loaded.
REQUIRED = ["AcquireDDThreadLock", "CompleteCreateSysmemSurface", "D3DParseUnknownCommand", "DDGetAttachedSurfaceLcl",
            "DDInternalLock", "DDInternalUnlock", "DSoundHelp", "DirectDrawCreate", "DirectDrawCreateClipper",
            "DirectDrawCreateEx", "DirectDrawEnumerateA", "DirectDrawEnumerateExA", "DirectDrawEnumerateExW",
            "DirectDrawEnumerateW", "DllCanUnloadNow", "DllGetClassObject", "GetDDSurfaceLocal", "GetOLEThunkData",
            "GetSurfaceFromDC", "RegisterSpecialCase", "ReleaseDDThreadLock", "SetAppCompatData"]
ALLOWED_IMPORTS = {"kernel32.dll", "user32.dll", "gdi32.dll", "msvcrt.dll"}
HOTPATCH = bytes([0x8B, 0xFF, 0x55, 0x8B, 0xEC])  # mov edi,edi / push ebp / mov ebp,esp


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
        fail("not a 32-bit (i386) DLL")
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] != 0x10B:
        fail("not PE32")
    exp_rva = struct.unpack_from("<I", data, opt + 96)[0]
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

    _, nnames, funcs, names, ords = struct.unpack_from("<IIIII", data, off(exp_rva) + 20)
    exports = {}
    for i in range(nnames):
        name = cstr(struct.unpack_from("<I", data, off(names) + 4 * i)[0])
        ordinal = struct.unpack_from("<H", data, off(ords) + 2 * i)[0]
        exports[name] = struct.unpack_from("<I", data, off(funcs) + 4 * ordinal)[0]
    missing = [n for n in REQUIRED if n not in exports]
    if missing:
        fail("missing exports: " + ", ".join(missing))
    start = off(exports["DirectDrawCreateEx"])
    if data[start:start + 5] != HOTPATCH:
        fail("DirectDrawCreateEx starts with %s, not the hot-patch prologue" % data[start:start + 5].hex())

    imports = []
    o = off(imp_rva)
    while struct.unpack_from("<I", data, o + 12)[0]:
        imports.append(cstr(struct.unpack_from("<I", data, o + 12)[0]).lower())
        o += 20
    bad = [d for d in imports if d not in ALLOWED_IMPORTS]
    if bad:
        fail("unexpected imports: " + ", ".join(bad))
    print("OK: %s: %d exports, hot-patchable DirectDrawCreateEx, imports %s" % (path, len(exports), " ".join(imports)))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build-win/ddraw.dll")
