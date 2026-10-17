#!/usr/bin/env python3
"""Every text of the settings (settings::Text, src/core/settings/strings.hpp) is used somewhere besides its table.

2.2.2: removing dgVoodoo, the old shadows and crowns left eighteen texts (times three languages) behind that nothing
showed. This check fails on a text no code names as Text::Name, so that the next removal takes its texts along.
"""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def names():
    hpp = open(os.path.join(ROOT, 'src/core/settings/strings.hpp'), encoding='utf-8').read()
    body = hpp[hpp.index('enum class Text'):]
    body = body[body.index('{') + 1:body.index('};')]
    body = re.sub(r'//[^\n]*', '', body)
    return [n.strip() for n in body.split(',') if n.strip() and n.strip() != 'Count']


def sources():
    for top in ('src', 'tests'):
        for folder, _, files in os.walk(os.path.join(ROOT, top)):
            for f in files:
                if f.endswith(('.cpp', '.hpp')) and f not in ('strings.cpp', 'strings.hpp'):
                    yield open(os.path.join(folder, f), encoding='utf-8').read()


def main():
    text = '\n'.join(sources())
    used = set(re.findall(r'Text::([A-Za-z0-9_]+)', text))
    unused = [n for n in names() if n not in used]
    if unused:
        print('UNUSED TEXTS: ' + ', '.join(unused))
        return 1
    print('OK: every settings text is used (%d)' % len(names()))
    return 0


if __name__ == '__main__':
    sys.exit(main())
