#!/usr/bin/env python3
"""Checks the settings app's texts (src/core/settings/strings.cpp, 2.15): every «name» quoted in a text names a part
of the interface in that text's language - the value of a text of the same language (the mnemonic "&", a trailing
"…", "..." or ":" and the "{1}" placeholders aside) - or is on the allow list (Windows', the game's and the site's
names). A button renamed later fails the build where its old name is still quoted.

Usage: python3 -I tools/check_text_names.py [strings.cpp]
"""
import re
import sys
from pathlib import Path

SOURCE = Path(__file__).resolve().parent.parent / "src" / "core" / "settings" / "strings.cpp"

# Names that are not the settings app's own: Windows', the game's, the site's admin and the files' kinds
ALLOW = {
    # Windows
    "Знімки екрана", "Скріншоти", "Параметри", "Пуск", "Дисплей", "Графіка", "Висока продуктивність",
    "Windows захистила ваш ПК", "Докладніше", "Усе одно запустити", "Зберегти", "Виконати", "Так", "Ні",
    # the site and its admin
    "Звіти графіки", "Оновлення клієнта", "Нові", "Чекають на відповідь", "Роздається",
    # the game and the words of a text, not names
    "Miss", "проявлення", "проявления",
    # 3.0.11-beta.28: what to type in «Довідник»'s search - the game's names
    "wing fly", "poring",
}


def _unescape(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            n = s[i + 1]
            out.append({"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\"}.get(n, n))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def entries(source):
    """[(en, uk, ru)] of kTexts, adjacent literals joined (an entry's Text id before them is no literal)."""
    head = "Entry kTexts[] = {"  # const, constexpr since 3.0.2-beta.6
    i = source.index(head) + len(head)
    out, values, current, depth = [], [], None, 0
    while i < len(source):
        c = source[i]
        if c == "/" and source.startswith("//", i):
            i = source.index("\n", i)
            continue
        if c == '"':
            j = i + 1
            while source[j] != '"':
                j += 2 if source[j] == "\\" else 1
            current = (current or "") + _unescape(source[i + 1:j])
            i = j + 1
            continue
        if c == "{":
            depth += 1
            values, current = [], None
        elif c == "," and depth == 1 and current is not None:
            values.append(current)
            current = None
        elif c == "}":
            if depth == 0:
                break
            depth -= 1
            if current is not None:
                values.append(current)
            out.append(tuple(values))
            current = None
        i += 1
    return out


def _bare(name):
    name = name.replace("&", "").strip()
    for end in ("…", "...", ":"):
        if name.endswith(end):
            name = name[: -len(end)].rstrip()
    return name


def _patterns(labels):
    exact, wild = set(), []
    for label in labels:
        bare = _bare(label).lower()
        if re.search(r"\{\d\}", bare):
            wild.append(re.compile("^" + re.sub(r"\\\{\d\\\}", ".+", re.escape(bare)) + "$"))
        else:
            exact.add(bare)
    return exact, wild


def _word_like(a, b):
    """The same word, its ending aside: Ukrainian and Russian decline names («Якості» of «Якість», «Опадами» of
    «Опади»); і and о alternate in Ukrainian."""
    if a == b:
        return True
    a, b = a.replace("і", "о"), b.replace("і", "о")
    common = 0
    while common < min(len(a), len(b)) and a[common] == b[common]:
        common += 1
    return common >= max(3, min(len(a), len(b)) - 4)


def _phrase_like(words, parts):
    return len(words) == len(parts) and all(_word_like(w, p) for w, p in zip(words, parts))


def _segments(label):
    """A label and its names within it: the head before ":", "(" or "—", and what follows ": "."""
    out = [label]
    for cut in (":", " (", " —"):
        if cut in label:
            out.append(label.split(cut, 1)[0].strip())
    if ": " in label:
        out.append(label.split(": ", 1)[1].strip())
    return out


def _names(name, exact, wild):
    low = name.lower()
    if low in exact or any(p.match(low) for p in wild):
        return True
    words = low.split()
    for label in exact:
        if len(label) > 60:  # a hint, not a name
            continue
        for segment in _segments(label):
            parts = segment.split()
            if _phrase_like(words, parts):
                return True
            # a name of two words or more: the start or the end of a longer one («Стики трави й доріг без сходинок»)
            if len(words) >= 2 and (_phrase_like(words, parts[:len(words)]) or
                                    _phrase_like(words, parts[-len(words):])):
                return True
    return False


def check(texts, allow=ALLOW):
    """[(index, name, text)] of the «names» that name nothing of the interface in their text's language."""
    languages = [_patterns(t[lang] for t in texts if len(t) > lang) for lang in range(3)]
    allowed = {a.lower() for a in allow}
    problems = []
    for index, values in enumerate(texts):
        for lang, text in enumerate(values[:3]):
            exact, wild = languages[lang]
            for name in re.findall(r"«([^«»]+)»", text):
                bare = _bare(name)
                if re.fullmatch(r"(\{\d\})+", bare) or bare.lower() in allowed or _names(bare, exact, wild):
                    continue  # a value put in at run time: a profile's name, what was searched
                problems.append((index, name, text))
    return problems


def main(path):
    texts = entries(Path(path).read_text(encoding="utf-8"))
    problems = check(texts)
    for index, name, text in problems:
        print("FAIL: «%s» names nothing of the interface - text %d: %s" % (name, index, text[:100]))
    if problems:
        sys.exit(1)
    print("OK: %d texts, every «name» is the interface's" % len(texts))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else SOURCE)
