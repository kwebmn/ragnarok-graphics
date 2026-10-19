#!/usr/bin/env python3
"""Writes src/core/ai/boss_sprites.inc (2.5.28): the sprite names of the server's bosses, for the texture pack's group
"bosses and MVPs" (core/ai/bosses.hpp) - the client's files cannot tell a boss from any monster.
A boss: an MVP (its MVP experience above 0), or a monster of the boss mode (0x20) that moves or fights (0x01, 0x80)
and is no plant (0x40) - not the Emperium, the treasure chests, the barricades, the flowers.
Usage: python3 -I tools/boss_sprites.py <uAthena>/db/mob_db.txt <uAthena>/db/mob_db2.txt > src/core/ai/boss_sprites.inc
"""
import sys


def bosses(paths):
    names = set()
    for path in paths:
        for line in open(path, encoding='latin-1'):
            line = line.strip()
            if not line or line.startswith('//'):
                continue
            p = line.split(',')
            if len(p) < 33:
                continue
            try:
                mode, mvp = int(p[25], 0), int(p[30])
            except ValueError:
                continue
            if mvp > 0 or (mode & 0x20 and not mode & 0x40 and mode & 0x81):
                names.add(p[1].strip().lower())
    return sorted(names)


def main(paths):
    names = bosses(paths)
    out = ['// Made by tools/boss_sprites.py from uAthena\'s db/mob_db.txt and db/mob_db2.txt: %d sprite names, '
           'sorted.' % len(names)]
    line = ''
    for n in names:
        item = '"%s",' % n
        if len(line) + len(item) + 1 > 116:
            out.append(line.rstrip())
            line = ''
        line += item + ' '
    if line:
        out.append(line.rstrip())
    sys.stdout.write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main(sys.argv[1:])
