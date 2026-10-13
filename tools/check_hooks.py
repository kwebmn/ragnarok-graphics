#!/usr/bin/env python3
"""Checks that every crisp text hook pops the stack and returns like the game function it replaces.

The game's window picture methods are MSVC __thiscall and its GDI imports __stdcall: the callee pops its stack
arguments, and a hook that pops a different number of bytes breaks the game's stack at once. The byte counts are the
"ret N" of the game's methods in uaRO.exe (docs/research/text.md §1.1) and the arguments of the GDI functions.
NAME=BYTES:ret also checks that the hook sets eax after its last call on every return path - it returns the value
it got from the game's function, not what its own last call (the lock) left there: the game reads GetDC's result.
Usage: check_hooks.py OBJECT NAME=BYTES[:ret]...
"""
import re
import subprocess
import sys

WRITES_EAX = re.compile(r"^(mov|movzx|movsx|lea|xor|or|and|add|sub|pop|xchg|cmov\w+|set\w+)\s+(eax|ax|al),")


def main(path, wanted):
    try:
        out = subprocess.run(["i686-w64-mingw32-objdump", "-d", "-M", "intel", path], capture_output=True,
                             text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        print("FAIL: cannot disassemble " + path)
        sys.exit(1)
    code = {}  # function -> its instructions
    current = None
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if m:
            current = m.group(1)
            code.setdefault(current, [])
            continue
        parts = line.split("\t")
        if current and len(parts) >= 3 and parts[0].strip().endswith(":"):
            code[current].append(parts[2].strip())
    failed = False
    for item in wanted:
        name, _, rule = item.partition("=")
        count, _, returns = rule.partition(":")
        # its body and cold part; not the file's static initializer, which GCC names after the first function
        found = [f for f in code if not f.startswith("__GLOBAL__") and re.search(r"\d%s(E|@|$)" % re.escape(name), f)]
        pops = set()
        unset = 0
        for f in found:
            ins = code[f]
            for i, op in enumerate(ins):
                if not op.startswith("ret"):
                    continue
                arg = op.split()
                pops.add(int(arg[1], 16) if len(arg) > 1 else 0)
                j = i - 1
                while j >= 0 and not ins[j].startswith("call") and not WRITES_EAX.match(ins[j]):
                    j -= 1
                if returns == "ret" and (j < 0 or ins[j].startswith("call")):
                    unset += 1
        if sorted(pops) != [int(count, 0)]:
            print("FAIL: %s pops %s, the game's function pops %s" % (name, sorted(pops) or "nothing found", count))
            failed = True
        elif unset:
            print("FAIL: %s returns what its last call left in eax on %d path(s), not the game's result"
                  % (name, unset))
            failed = True
        else:
            print("OK: %s pops %s%s" % (name, count, " and returns a value" if returns == "ret" else ""))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
