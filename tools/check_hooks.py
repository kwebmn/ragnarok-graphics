#!/usr/bin/env python3
"""Checks that every crisp text hook pops the stack like the game function it replaces.

The game's window picture methods are MSVC __thiscall and its GDI imports __stdcall: the callee pops its stack
arguments, and a hook that pops a different number of bytes breaks the game's stack at once. The byte counts are the
"ret N" of the game's methods in uaRO.exe (docs/research/text.md §1.1) and the arguments of the GDI functions.
Usage: check_hooks.py OBJECT NAME=BYTES...
"""
import re
import subprocess
import sys


def main(path, wanted):
    try:
        out = subprocess.run(["i686-w64-mingw32-objdump", "-d", path], capture_output=True, text=True,
                             check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        print("FAIL: cannot disassemble " + path)
        sys.exit(1)
    rets = {}
    current = None
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if m:
            current = m.group(1)
            rets.setdefault(current, [])
        elif current and re.search(r"\sret\b", line):
            n = re.search(r"\$0x([0-9a-f]+)", line)
            rets[current].append(int(n.group(1), 16) if n else 0)
    failed = False
    for item in wanted:
        name, count = item.split("=")
        # its body and cold part; not the file's static initializer, which GCC names after the first function
        found = [f for f in rets if not f.startswith("__GLOBAL__") and re.search(r"\d%s(E|@|$)" % re.escape(name), f)]
        pops = sorted({n for f in found for n in rets[f]})
        if pops != [int(count, 0)]:
            print("FAIL: %s pops %s, the game's function pops %s" % (name, pops or "nothing found", count))
            failed = True
        else:
            print("OK: %s pops %s" % (name, count))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
