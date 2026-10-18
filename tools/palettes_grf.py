#!/usr/bin/env python3
"""palettes_grf.py OUT.grf SOURCE [SOURCE...]: the client's Palettes.grf with the palettes the server lets be worn.

uaRO's server dyes hair 0-20 on hair styles 0-23 and clothes 0-77 (its stylist npc/custom/dye.txt; uAthena's
pc_changelook clamps every look change to its limits, the char server makes new characters inside them). The client
(uaRO.exe of 2007) asks for data\\palette\\머리\\머리<style>_<sex>_<colour>.pal and
data\\palette\\몸\\<job>_<sex>_<colour>.pal; a palette it does not find draws the sprite's own colours, no error.
Kamishi's palette pack has 219,293 of them (700 clothes colours a job, 128 hair colours on 120 styles, costumes, the
Doram race - neither known to the 2007 client); about 18,500 can be worn.

SOURCE: a folder (with a palette folder in it, or the palette folder itself), a .zip or a GRF 0x200 (not encrypted);
the first source that has a palette gives it. The GRF is written as uaRO.exe reads one (FUN_00510fd0: each entry by
its own offset and sizes), each palette zlib-compressed once - entries of the same palette share its data - and read
back whole to check it before the script ends.
"""
import hashlib
import os
import re
import struct
import sys
import time
import zipfile
import zlib

# The server's limits (uAthena: the stylist's ranges; its conf/battle/client.conf says hair colours 0-8 in the
# repository only). Raised there, raise them here and run this again.
MAX_HAIR_STYLE = 23
MAX_HAIR_COLOUR = 20
MAX_CLOTH_COLOUR = 77

HAIR = re.compile(r'^data\\palette\\머리\\머리(\d+)_([^_\\]+)_(\d+)\.pal$', re.IGNORECASE)
CLOTHES = re.compile(r'^data\\palette\\몸\\([^\\]+)_([^_\\]+)_(\d+)\.pal$', re.IGNORECASE)
GRF_MAGIC = b'Master of Magic\0'
GRF_HEADER = 46


def canonical(path):
    """data\\palette\\<rest> for a path with a palette folder in it, else None."""
    parts = [p for p in re.split(r'[\\/]+', path) if p]
    for i, p in enumerate(parts):
        if p.lower() == 'palette' and i + 1 < len(parts):
            return 'data\\palette\\' + '\\'.join(parts[i + 1:])
    return None


def wanted(name):
    """Why a palette is left out ('' to keep it)."""
    m = HAIR.match(name)
    if m:
        ok = int(m.group(1)) <= MAX_HAIR_STYLE and int(m.group(3)) <= MAX_HAIR_COLOUR
        return '' if ok else 'hair past styles 0-%d or colours 0-%d' % (MAX_HAIR_STYLE, MAX_HAIR_COLOUR)
    m = CLOTHES.match(name)
    if m:
        return '' if int(m.group(3)) <= MAX_CLOTH_COLOUR else 'clothes past colours 0-%d' % MAX_CLOTH_COLOUR
    rest = name[len('data\\palette\\'):].split('\\')
    if len(rest) > 2 or (len(rest) == 2 and rest[0] not in ('머리', '몸')):  # costumes, the Doram race
        return 'unknown to the 2007 client: ' + '\\'.join(rest[:-1])
    return 'names the client never asks for'


def zip_name(info):
    if info.flag_bits & 0x800:  # the name in UTF-8
        return info.filename
    try:  # Python read the bytes as cp437: Windows' zips name Korean files in cp949
        return info.filename.encode('cp437').decode('cp949')
    except (UnicodeEncodeError, UnicodeDecodeError):
        return info.filename


def grf_entries(path):
    """name -> (offset, compressed, aligned, size, flags) of a GRF 0x200."""
    with open(path, 'rb') as f:
        h = f.read(GRF_HEADER)
        if len(h) < GRF_HEADER or h[:15] != GRF_MAGIC[:15] or struct.unpack_from('<I', h, 42)[0] != 0x200:
            raise ValueError(path + ' is no GRF 0x200')
        f.seek(GRF_HEADER + struct.unpack_from('<I', h, 30)[0])
        packed, unpacked = struct.unpack('<II', f.read(8))
        table = zlib.decompress(f.read(packed))
    if len(table) != unpacked:
        raise ValueError(path + ': damaged file table')
    out, p = {}, 0
    while p < len(table):
        end = table.index(b'\0', p)
        name = table[p:end].decode('cp949', 'replace')
        compressed, aligned, size, flags, offset = struct.unpack_from('<IIIBI', table, end + 1)
        out[name] = (offset, compressed, aligned, size, flags)
        p = end + 1 + 17
    return out


def grf_read(f, entry):
    offset, compressed, aligned, size, flags = entry
    if not flags & 1 or flags & 6:
        raise ValueError('a folder or an encrypted entry')
    f.seek(GRF_HEADER + offset)
    data = zlib.decompress(f.read(aligned)[:compressed])
    if len(data) != size:
        raise ValueError('damaged data')
    return data


def read_sources(sources, others):
    """canonical name -> palette bytes; the first source's wins. Names compared without case, as the client does.
    others gets the files that are no palettes (an old Palettes.grf may hold more): they are not written."""
    out, seen = {}, set()

    def take(path, read):
        name = canonical(path)
        if not name or not name.lower().endswith('.pal'):
            others.append(path)
            return
        if name.lower() in seen:
            return
        seen.add(name.lower())
        out[name] = read()

    for src in sources:
        if os.path.isdir(src):
            base = os.path.abspath(src)
            inside = base.lower().replace('/', '\\').rstrip('\\').endswith('\\palette')
            for root, _, files in os.walk(base):
                for fn in sorted(files):
                    full = os.path.join(root, fn)
                    rel = os.path.relpath(full, base)
                    take(('palette\\' if inside else '') + rel, lambda full=full: open(full, 'rb').read())
        elif src.lower().endswith('.zip'):
            with zipfile.ZipFile(src) as z:
                for info in z.infolist():
                    if not info.is_dir():
                        take(zip_name(info), lambda info=info: z.read(info))
        elif src.lower().endswith(('.grf', '.gpf')):
            entries = grf_entries(src)
            with open(src, 'rb') as f:
                for name, e in sorted(entries.items()):
                    take(name, lambda e=e: grf_read(f, e))
        else:
            raise ValueError(src + ': neither a folder, a .zip nor a .grf')
    return out


def write_grf(path, palettes):
    """palettes: [(name, bytes)]; the same bytes are written once. Returns the number of data blocks."""
    blocks, data, table = {}, bytearray(), bytearray()
    for name, raw in palettes:
        key = hashlib.sha1(raw).digest()
        if key not in blocks:
            z = zlib.compress(raw, 9)
            aligned = (len(z) + 7) & ~7  # as GRF tools write it; the client reads this many bytes
            blocks[key] = (len(data), len(z), aligned, len(raw))
            data += z + bytes(aligned - len(z))
        offset, compressed, aligned, size = blocks[key]
        table += name.encode('cp949') + b'\0' + struct.pack('<IIIBI', compressed, aligned, size, 1, offset)
    packed = zlib.compress(bytes(table), 9)
    header = GRF_MAGIC + bytes(14) + struct.pack('<IIII', len(data), 0, len(palettes) + 7, 0x200)
    tmp = path + '.new'
    with open(tmp, 'wb') as f:
        f.write(header + data + struct.pack('<II', len(packed), len(table)) + packed)
    os.replace(tmp, path)
    return len(blocks)


def main(argv):
    if len(argv) < 3:
        print(__doc__.strip())
        return 2
    out, sources = argv[1], argv[2:]
    started = time.time()
    others = []
    found = read_sources(sources, others)
    kept, left = [], {}
    for name in sorted(found):
        why = wanted(name) if found[name] else 'empty'
        if why:
            left[why] = left.get(why, 0) + 1
        else:
            kept.append((name, found[name]))
    if not kept:
        print('no palette to keep in ' + ', '.join(sources))
        return 1
    blocks = write_grf(out, kept)
    # Read back as the client does: every name, its data whole.
    entries = grf_entries(out)
    with open(out, 'rb') as f:
        bad = sum(1 for name, raw in kept if name not in entries or grf_read(f, entries[name]) != raw)
    if bad or len(entries) != len(kept):
        print('%s: %d palettes do not read back as written' % (out, bad))
        return 1
    hair = sum(1 for name, _ in kept if HAIR.match(name))
    print('%s: %d palettes (hair %d, clothes %d), %d of them different, %.1f MB, in %.1f s' %
          (out, len(kept), hair, len(kept) - hair, blocks, os.path.getsize(out) / 1e6, time.time() - started))
    print('limits: hair styles 0-%d, hair colours 0-%d, clothes colours 0-%d; read %d palettes from %s' %
          (MAX_HAIR_STYLE, MAX_HAIR_COLOUR, MAX_CLOTH_COLOUR, len(found), ', '.join(sources)))
    for why, n in sorted(left.items(), key=lambda x: -x[1]):
        print('  left out %d: %s' % (n, why))
    if others:  # what an old Palettes.grf held besides palettes: it is not in the new one
        print('  left out %d files that are no palettes, e.g. %s' % (len(others), ', '.join(others[:5])))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
