#!/usr/bin/env python3
"""check_pure_calls.py BUILD_DIR... - no code of ours calls __cxa_pure_virtual directly.

GCC turns a virtual call into a direct call of __cxa_pure_virtual when it decides the object can only be of an
abstract class: a class in an anonymous namespace (every derived class in sight) with none derived from it - which
is what a COM interface declared there by hand looks like. 2.5.0-2.5.7 had DRED's settings so (device.cpp): the
call went to address 0 and every start of Direct3D 12 with the runtime's DRED crashed. A vtable's entry for a pure
function is a pointer (dir32, ADDR64) and is fine; a call (DISP32, REL32) is the bug.
"""
import os
import subprocess
import sys

CALLS = ("DISP32", "R_X86_64_PC32", "IMAGE_REL_AMD64_REL32", "IMAGE_REL_I386_REL32")


def objdump_for(path):
    with open(path, "rb") as f:
        machine = f.read(2)
    return "x86_64-w64-mingw32-objdump" if machine == b"\x64\x86" else "i686-w64-mingw32-objdump"


def main(dirs):
    bad = []
    checked = 0
    for top in dirs:
        for root, _, files in os.walk(top):
            if "CMakeFiles" not in root:
                continue
            for name in files:
                if not name.endswith(".obj"):
                    continue
                path = os.path.join(root, name)
                out = subprocess.run([objdump_for(path), "-r", path], capture_output=True, text=True).stdout
                checked += 1
                for line in out.splitlines():
                    parts = line.split()
                    if len(parts) >= 3 and parts[1] in CALLS and parts[2].lstrip("_") == "cxa_pure_virtual":
                        bad.append("%s: %s" % (path, line.strip()))
    if not checked:
        print("FAIL: no object files under " + " ".join(dirs))
        return 1
    if bad:
        print("FAIL: calls of a pure virtual function (an interface in an anonymous namespace?):")
        for b in bad:
            print("  " + b)
        return 1
    print("OK: %d object files, no direct call of __cxa_pure_virtual" % checked)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:] or ["build-win"]))
