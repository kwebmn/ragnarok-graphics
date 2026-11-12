#!/usr/bin/env python3
"""Puts the version headings back in a notes file (dist/TEST.<lang>.md) - 3.0.18-beta.191 (the launcher's «Журнал
змін», docs/superpowers/plans/2026-11-12-launcher.md Task 1).

From 3.0.18-beta.56 the ship script prepended each beta's points without «# uaRO Graphics X — що нового»: the text
above the file's first heading is those betas' blocks (separated by blank lines), the newest first. Each gets its
heading back, with its date when known:

  notes_headings.py FILE LANG FIRST LAST DATES   (DATES: a file of «version YYYY-MM-DD» lines)
  notes_headings.py --self-test
"""
import re
import sys

WORDS = {"uk": "що нового", "ru": "что нового", "en": "what's new"}


def heading(version, lang, date):
    """The heading line the ship script writes: «# uaRO Graphics X — <word> · YYYY-MM-DD» (no date: without it)."""
    line = "# uaRO Graphics %s — %s" % (version, WORDS[lang])
    return line + " · " + date if date else line


def betas(first, last):
    """The versions FIRST..LAST of one release's betas, the newest first: 3.0.18-beta.56..3.0.18-beta.58 ->
    [beta.58, beta.57, beta.56]."""
    base, a = first.rsplit(".", 1)
    base2, b = last.rsplit(".", 1)
    if base != base2 or not base.endswith("-beta") or int(b) < int(a):
        raise ValueError("not one release's betas: %s %s" % (first, last))
    return ["%s.%d" % (base, n) for n in range(int(b), int(a) - 1, -1)]


def headed(text, lang, versions, dates):
    """The file with a heading before each bare block above its first heading - `versions` newest first, as many as
    there are blocks; None when the count differs (nothing written)."""
    nl = "\r\n" if "\r\n" in text else "\n"
    lines = text.split(nl)
    first = next((i for i, l in enumerate(lines) if l.startswith("# ")), len(lines))
    blocks, block = [], []
    for line in lines[:first]:
        if line.strip():
            block.append(line)
        elif block:
            blocks.append(block)
            block = []
    if block:
        blocks.append(block)
    if not blocks or len(blocks) != len(versions):
        return None
    out = []
    for version, block in zip(versions, blocks):
        out += [heading(version, lang, dates.get(version, "")), ""] + block + [""]
    return nl.join(out + lines[first:])


def self_test():
    text = "- c\n\n- b1\n- b2\n\n- a\n\n# uaRO Graphics 3.0.18-beta.0 — що нового\n\n- old\n"
    vs = betas("3.0.18-beta.1", "3.0.18-beta.3")
    assert vs == ["3.0.18-beta.3", "3.0.18-beta.2", "3.0.18-beta.1"], vs
    out = headed(text, "uk", vs, {"3.0.18-beta.3": "2026-11-12", "3.0.18-beta.1": "2026-11-10"})
    want = ("# uaRO Graphics 3.0.18-beta.3 — що нового · 2026-11-12\n\n- c\n\n"
            "# uaRO Graphics 3.0.18-beta.2 — що нового\n\n- b1\n- b2\n\n"
            "# uaRO Graphics 3.0.18-beta.1 — що нового · 2026-11-10\n\n- a\n\n"
            "# uaRO Graphics 3.0.18-beta.0 — що нового\n\n- old\n")
    assert out == want, repr(out)
    assert headed(text, "uk", betas("3.0.18-beta.1", "3.0.18-beta.2"), {}) is None  # 3 blocks, 2 versions
    assert headed(want, "uk", vs, {}) is None  # already headed: no bare blocks
    assert heading("3.0.18", "en", "") == "# uaRO Graphics 3.0.18 — what's new"
    crlf = text.replace("\n", "\r\n")
    assert headed(crlf, "ru", vs, {}).startswith("# uaRO Graphics 3.0.18-beta.3 — что нового\r\n\r\n- c\r\n")
    print("OK")


def main(argv):
    if argv[1:] == ["--self-test"]:
        self_test()
        return 0
    if len(argv) != 6:
        print(__doc__)
        return 2
    path, lang, first, last, dates_file = argv[1:]
    dates = {}
    for line in open(dates_file, encoding="utf-8"):
        parts = line.split()
        if len(parts) == 2 and re.fullmatch(r"\d{4}-\d{2}-\d{2}", parts[1]):
            dates[parts[0]] = parts[1]
    with open(path, encoding="utf-8", newline="") as f:
        text = f.read()
    out = headed(text, lang, betas(first, last), dates)
    if out is None:
        print("FAIL: %s - the bare blocks are not %s..%s" % (path, first, last))
        return 1
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(out)
    print("OK: %s - %d headings" % (path, len(betas(first, last))))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
