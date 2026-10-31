#!/usr/bin/env python3
"""Writes src/core/ui_icons.inc (3.0.11-beta.26): the line icons of the interface concept (its style A mock-ups, the
same drawings) as polylines in their 24 x 24 box - arcs, curves and circles made short straight pieces - for the
driver to stroke at any size (core/ui_icons).
Usage: python3 -I tools/ui_icons.py > src/core/ui_icons.inc
"""
import math
import re

# The concept's icons (uic mock-ups, 2026-10-28), by the names core/ui_icons.hpp gives them, in its order
ICONS = [
    ('Search', '<circle cx="11" cy="11" r="6.5"/><path d="M16 16l4.5 4.5"/>'),
    ('Monster', '<path d="M5 20V10a7 7 0 0 1 14 0v10l-2.4-1.6L14.2 20 12 18.4 9.8 20l-2.4-1.6z"/>'
                '<circle cx="9.5" cy="10.5" r=".8"/><circle cx="14.5" cy="10.5" r=".8"/>'),
    ('Item', '<path d="M10 3h4M10.5 3v5L6.6 14.2A4.6 4.6 0 0 0 10.6 21h2.8a4.6 4.6 0 0 0 4-6.8L13.5 8V3"/>'
             '<path d="M8 14h8"/>'),
    ('Location', '<path d="M9 4L3 6.5v13.5l6-2.5 6 2.5 6-2.5V4l-6 2.5z"/><path d="M9 4v13.5M15 6.5V20"/>'),
    ('Quest', '<path d="M6 5a2 2 0 0 1 2-2h10v15a3 3 0 0 1-3 3H8"/><path d="M6 5v13a3 3 0 0 0 3 3"/>'
              '<path d="M10 8h5M10 12h5"/>'),
    ('Npc', '<circle cx="12" cy="8" r="3.5"/><path d="M5 20a7 7 0 0 1 14 0"/>'),
    ('Mvp', '<path d="M4 18h16l-1.4-10-4.6 4-2-6-2 6-4.6-4z"/>'),
    ('Pet', '<circle cx="7" cy="10.5" r="1.7"/><circle cx="17" cy="10.5" r="1.7"/><circle cx="9.8" cy="6.3" r="1.7"/>'
            '<circle cx="14.2" cy="6.3" r="1.7"/>'
            '<path d="M8 17.2c0-2.6 1.8-4.7 4-4.7s4 2.1 4 4.7c0 1.6-1.7 2.3-4 2.3s-4-.7-4-2.3z"/>'),
    ('Craft', '<path d="M13.5 6.5l4 4"/><path d="M11 4l9 9-2.5 2.5-9-9z"/><path d="M9.8 11.2L3.5 17.5l3 3 6.3-6.3"/>'),
    ('World', '<circle cx="12" cy="12" r="8.5"/>'
              '<path d="M3.5 12h17M12 3.5c3 3.2 3 13.8 0 17M12 3.5c-3 3.2-3 13.8 0 17"/>'),
    ('Tables', '<rect x="4" y="4" width="16" height="16" rx="2"/><path d="M4 10h16M4 15h16M10 4v16"/>'),
    ('Back', '<path d="M15 5l-7 7 7 7"/>'),
    ('External', '<path d="M14 4h6v6"/><path d="M20 4l-9 9"/>'
                 '<path d="M19 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V6a1 1 0 0 1 1-1h5"/>'),
    ('Route', '<circle cx="6" cy="18" r="2.5"/><circle cx="18" cy="6" r="2.5"/>'
              '<path d="M8.5 18H15a3 3 0 0 0 0-6H9a3 3 0 0 1 0-6h6.5"/>'),
    # 3.0.11-beta.29, the world map's (v19_route): its dungeons' tab, its zoom's buttons, the panel's warning
    ('Dungeon', '<path d="M4 20V9l3-2 3 2 2-1.5L14 9l3-2 3 2v11z"/><path d="M10 20v-4.5a2 2 0 0 1 4 0V20"/>'),
    ('Minus', '<path d="M6 12h12"/>'),
    ('Plus', '<path d="M12 6v12M6 12h12"/>'),
    ('Expand', '<path d="M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5"/>'),
    ('Warn', '<path d="M12 4l9 16H3z"/><path d="M12 10v4.5M12 17.5v.3"/>'),
    # 3.0.11-beta.30: «Головна» - the first screen of «Довідник» (the header's button, the rail's first)
    ('Home', '<path d="M4 11.5L12 4.5l8 7"/><path d="M6.5 9.8V20h11V9.8"/><path d="M10 20v-5.5h4V20"/>'),
    # 3.0.11-beta.36: the character block's buttons - the game's Basic Info window's: its stats (Alt+A), its gear
    # (Alt+Q), its items (Alt+E), its skills (Alt+S), its friends (Alt+Z), the options (Alt+O)
    ('Stats', '<path d="M5 20v-7M10 20V5M15 20v-10M20 20V8"/>'),
    ('Equip', '<path d="M12 3.5l7 2.8v5.2c0 4.4-2.9 7.9-7 9.5-4.1-1.6-7-5.1-7-9.5V6.3z"/>'),
    ('Bag', '<path d="M5.5 8.5h13l-1.2 11.5H6.7z"/><path d="M9 8.5V7a3 3 0 0 1 6 0v1.5"/>'),
    ('Skill', '<path d="M13.5 3L5.5 13.5h6L10.5 21l8-10.5h-6z"/>'),
    ('Friends', '<circle cx="9" cy="8.5" r="3"/><path d="M3.5 19.5a5.5 5.5 0 0 1 11 0"/>'
                '<path d="M15.5 5.6a3 3 0 0 1 0 5.8M17 14a5.5 5.5 0 0 1 3.5 5.5"/>'),
    ('Gear', '<circle cx="12" cy="12" r="3"/>'
             '<path d="M12 3.5v3M12 17.5v3M3.5 12h3M17.5 12h3M6 6l2.1 2.1M15.9 15.9L18 18M6 18l2.1-2.1M15.9 8.1L18 6"/>'),
    # 3.0.11-beta.40: the hotbar's edit button (the game's own bar back to drag skills and items into it), its done
    ('Edit', '<path d="M4.5 19.5h4l10-10-4-4-10 10z"/><path d="M13 7l4 4"/>'),
    ('Done', '<path d="M5 12.5l4.5 4.5L19 7.5"/>'),
    # 3.0.11-beta.41: the chat's channel - a chevron down beside its name (a click goes round them)
    ('Down', '<path d="M6.5 9.5l5.5 5.5 5.5-5.5"/>'),
    # 3.0.11-beta.45: the hotbar's «не тут» - a crossed circle over a skill or an item the map forbids
    ('Blocked', '<circle cx="12" cy="12" r="8.5"/><path d="M6 6l12 12"/>'),
    # 3.0.11-beta.86: the controller's buttons on the interface (core/pad_glyphs) - PlayStation's four shapes, the
    # D-pad's four ways
    ('PadCross', '<path d="M7.6 7.6l8.8 8.8M16.4 7.6l-8.8 8.8"/>'),
    ('PadCircle', '<circle cx="12" cy="12" r="5.4"/>'),
    ('PadSquare', '<path d="M7 7h10v10H7z"/>'),
    ('PadTriangle', '<path d="M12 6.4l6 10.4H6z"/>'),
    ('PadUp', '<path d="M12 7.5l4.5 6h-9z"/>'),
    ('PadRight', '<path d="M16.5 12l-6 4.5v-9z"/>'),
    ('PadDown', '<path d="M12 16.5l-4.5-6h9z"/>'),
    ('PadLeft', '<path d="M7.5 12l6-4.5v9z"/>'),
    # 3.0.11-beta.90: the windows' tile's «Контролер» (core/pad_tiles)
    ('Gamepad', '<path d="M7 8h10a4 4 0 0 1 4 4l.6 4.2a2.4 2.4 0 0 1-4.3 1.8L15.5 16h-7l-1.8 2a2.4 2.4 0 0 1-4.3-1.8'
                'L3 12a4 4 0 0 1 4-4z"/><path d="M8 10.5v3.5M6.25 12.25h3.5"/>'
                '<circle cx="16" cy="11.4" r=".7"/><circle cx="17.8" cy="13.2" r=".7"/>'),
]

NUMBER = r'[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?'
STEP = 0.35  # the longest straight piece of a curve, in the box's units


def circle(cx, cy, r):
    n = max(8, int(math.ceil(2 * math.pi * r / STEP)))
    return [[(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)) for i in range(n)], True]


def arc(x1, y1, rx, ry, phi, large, sweep, x2, y2):
    """The SVG arc from (x1, y1) to (x2, y2) as points after the first (the spec's endpoint to centre conversion)."""
    if rx == 0 or ry == 0:
        return [(x2, y2)]
    rx, ry = abs(rx), abs(ry)
    c, s = math.cos(math.radians(phi)), math.sin(math.radians(phi))
    dx, dy = (x1 - x2) / 2, (y1 - y2) / 2
    x1p, y1p = c * dx + s * dy, -s * dx + c * dy
    lam = (x1p / rx) ** 2 + (y1p / ry) ** 2
    if lam > 1:
        rx, ry = rx * math.sqrt(lam), ry * math.sqrt(lam)
    num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
    den = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    k = math.sqrt(max(0.0, num / den)) if den else 0.0
    if large == sweep:
        k = -k
    cxp, cyp = k * rx * y1p / ry, -k * ry * x1p / rx
    cx, cy = c * cxp - s * cyp + (x1 + x2) / 2, s * cxp + c * cyp + (y1 + y2) / 2

    def angle(ux, uy, vx, vy):
        a = math.atan2(ux * vy - uy * vx, ux * vx + uy * vy)
        return a

    t1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dt = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not sweep and dt > 0:
        dt -= 2 * math.pi
    elif sweep and dt < 0:
        dt += 2 * math.pi
    n = max(2, int(math.ceil(abs(dt) * max(rx, ry) / STEP)))
    out = []
    for i in range(1, n + 1):
        t = t1 + dt * i / n
        px, py = rx * math.cos(t), ry * math.sin(t)
        out.append((c * px - s * py + cx, s * px + c * py + cy))
    out[-1] = (x2, y2)
    return out


def cubic(p0, p1, p2, p3):
    length = math.dist(p0, p1) + math.dist(p1, p2) + math.dist(p2, p3)
    n = max(2, int(math.ceil(length / STEP)))
    out = []
    for i in range(1, n + 1):
        t = i / n
        u = 1 - t
        out.append((u ** 3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t ** 3 * p3[0],
                    u ** 3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t ** 3 * p3[1]))
    return out


def path(d):
    """An SVG path's subpaths as [points, closed]."""
    tokens = re.findall(r'[MmLlHhVvAaCcSsZz]|' + NUMBER, d)
    lines, cur, start = [], None, (0.0, 0.0)
    x = y = 0.0
    last_ctrl = None
    i, cmd = 0, None

    def num():
        nonlocal i
        v = float(tokens[i])
        i += 1
        return v

    while i < len(tokens):
        if re.fullmatch(r'[A-Za-z]', tokens[i]):
            cmd = tokens[i]
            i += 1
            if cmd in 'Zz':
                if cur:
                    cur[1] = True
                    x, y = start
                cur, last_ctrl = None, None
                continue
        rel = cmd.islower()
        c = cmd.upper()
        if c == 'M':
            nx, ny = num(), num()
            x, y = (x + nx, y + ny) if rel else (nx, ny)
            cur = [[(x, y)], False]
            lines.append(cur)
            start = (x, y)
            cmd = 'l' if rel else 'L'  # the pairs after a move are lines
            last_ctrl = None
            continue
        if cur is None:  # drawing on after a close: from where it closed
            cur = [[(x, y)], False]
            lines.append(cur)
        if c == 'L':
            nx, ny = num(), num()
            x, y = (x + nx, y + ny) if rel else (nx, ny)
            cur[0].append((x, y))
            last_ctrl = None
        elif c == 'H':
            nx = num()
            x = x + nx if rel else nx
            cur[0].append((x, y))
            last_ctrl = None
        elif c == 'V':
            ny = num()
            y = y + ny if rel else ny
            cur[0].append((x, y))
            last_ctrl = None
        elif c == 'A':
            rx, ry, phi, large, sweep, nx, ny = num(), num(), num(), num(), num(), num(), num()
            ex, ey = (x + nx, y + ny) if rel else (nx, ny)
            cur[0].extend(arc(x, y, rx, ry, phi, int(large), int(sweep), ex, ey))
            x, y = ex, ey
            last_ctrl = None
        elif c == 'C':
            v = [num() for _ in range(6)]
            p1 = (x + v[0], y + v[1]) if rel else (v[0], v[1])
            p2 = (x + v[2], y + v[3]) if rel else (v[2], v[3])
            p3 = (x + v[4], y + v[5]) if rel else (v[4], v[5])
            cur[0].extend(cubic((x, y), p1, p2, p3))
            x, y = p3
            last_ctrl = p2
        elif c == 'S':
            v = [num() for _ in range(4)]
            p1 = (2 * x - last_ctrl[0], 2 * y - last_ctrl[1]) if last_ctrl else (x, y)
            p2 = (x + v[0], y + v[1]) if rel else (v[0], v[1])
            p3 = (x + v[2], y + v[3]) if rel else (v[2], v[3])
            cur[0].extend(cubic((x, y), p1, p2, p3))
            x, y = p3
            last_ctrl = p2
        else:
            raise ValueError('unknown path command ' + cmd)
    return lines


def rect(x, y, w, h, r):
    d = (f'M{x + r} {y}H{x + w - r}A{r} {r} 0 0 1 {x + w} {y + r}V{y + h - r}A{r} {r} 0 0 1 {x + w - r} {y + h}'
         f'H{x + r}A{r} {r} 0 0 1 {x} {y + h - r}V{y + r}A{r} {r} 0 0 1 {x + r} {y}z')
    return path(d)


def shapes(svg):
    out = []
    for tag, attrs in re.findall(r'<(path|circle|rect)\s([^>]*)/>', svg):
        a = dict(re.findall(r'([\w-]+)="([^"]*)"', attrs))
        if tag == 'path':
            out.extend(path(a['d']))
        elif tag == 'circle':
            out.append(circle(float(a['cx']), float(a['cy']), float(a['r'])))
        else:
            out.extend(rect(float(a['x']), float(a['y']), float(a['width']), float(a['height']), float(a.get('rx', 0))))
    return out


def main():
    points, lines, icons = [], [], []
    for name, svg in ICONS:
        first_line = len(lines)
        for pts, closed in shapes(svg):
            # one point less where a closed line ends where it began
            if closed and len(pts) > 2 and math.dist(pts[0], pts[-1]) < 1e-6:
                pts = pts[:-1]
            lines.append((len(points), len(pts), closed))
            points.extend(pts)
        icons.append((name, first_line, len(lines) - first_line))
    print('// Generated by tools/ui_icons.py - do not edit. The interface concept\'s line icons, polylines in their')
    print('// 24 x 24 box (core/ui_icons strokes them).')
    print('constexpr float kIconPoints[][2] = {')
    row = []
    for px, py in points:
        row.append(f'{{{px:.3f}f, {py:.3f}f}}')
        if len(row) == 5:
            print('    ' + ', '.join(row) + ',')
            row = []
    if row:
        print('    ' + ', '.join(row) + ',')
    print('};')
    print('struct IconLine {')
    print('    uint16_t first, count;  // in kIconPoints')
    print('    bool closed;')
    print('};')
    print('constexpr IconLine kIconLines[] = {')
    for first, count, closed in lines:
        print(f'    {{{first}, {count}, {"true" if closed else "false"}}},')
    print('};')
    print('constexpr struct {')
    print('    uint16_t first, count;  // in kIconLines')
    print('} kIconShapes[] = {')
    for name, first, count in icons:
        print(f'    {{{first}, {count}}},  // {name}')
    print('};')


if __name__ == '__main__':
    main()
