#include "core/reference_window.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::refwin {
namespace {

// At scale 1 (1080p), the target's pixels
constexpr float kW = 1270, kH = 850, kHeader = 44, kSearch = 50, kFooter = 28, kPad = 18, kEdgeRoom = 20;
constexpr float kTitleRow = 46, kSub = 26, kHeading = 38, kRow = 26, kNote = 24, kTile = 78, kGap = 10;
constexpr uint32_t kGround = 0x12161E, kBand = 0x0D1016, kField = 0x1B212C, kFieldEdge = 0x4C8DFF;
constexpr uint32_t kRule = 0x2A3342, kHover = 0x1F2733, kTileFill = 0x1A2029, kTileHot = 0x232B37;
constexpr uint32_t kTrack = 0x161B23, kThumb = 0x3A4556;
constexpr float kWorldLeast = 300;                         // 3.0.11-beta.11: the world map's row at the least
constexpr float kPortrait = 84;                            // 3.0.11-beta.14: a card's picture row
constexpr float kTextLine = 19;  // 3.0.11-beta.16: a Text row's line (13.5 px words)
constexpr float kIcon = 24;                                // 3.0.11-beta.15: an item's icon in a row (1:1 at 1080p)
constexpr uint32_t kRoute = 0x5AC8FA, kBadge = 0x0D1016;  // the minimap's way colour; the players' badge

int px(float v, float s) { return int(std::lround(v * s)); }

int heightOf(refpages::Style st, float s) {
    switch (st) {
        case refpages::Style::Title: return px(kTitleRow, s);
        case refpages::Style::Sub: return px(kSub, s);
        case refpages::Style::Heading: return px(kHeading, s);
        case refpages::Style::Note: return px(kNote, s);
        case refpages::Style::Tile: return px(kTile, s);
        case refpages::Style::World: return px(kWorldLeast, s);  // place() gives it the body's rest
        case refpages::Style::Portrait: return px(kPortrait, s);
        case refpages::Style::Text: return px(kTextLine, s);  // place() gives it its lines
        case refpages::Style::Row: break;
    }
    return px(kRow, s);
}

panel::Box backBox(const Layout& L) { return panel::Box{float(L.pad) * 0.5f, 0, float(L.w) * 0.42f, float(L.header)}; }
panel::Box closeBox(const Layout& L) {
    return panel::Box{float(L.w - L.header), 0, float(L.header), float(L.header)};
}
panel::Box fieldBox(const Layout& L) {
    return panel::Box{float(L.pad), float(L.header) + 7 * L.s, float(L.w - 2 * L.pad), float(L.search) - 14 * L.s};
}

void fill(panel::Picture& p, const panel::Box& b, uint32_t rgb) { panel::roundRect(p, b, 0, rgb, 1.0f); }

}  // namespace

Layout layout(int targetW, int targetH, float scale) {
    Layout L;
    L.s = std::max(0.5f, scale);
    const int room = px(kEdgeRoom, L.s);
    L.w = std::max(200, std::min(px(kW, L.s), targetW - 2 * room));
    L.h = std::max(200, std::min(px(kH, L.s), targetH - 2 * room));
    L.x = std::max(0, (targetW - L.w) / 2);
    L.y = std::max(0, (targetH - L.h) / 2);
    L.header = px(kHeader, L.s);
    L.search = px(kSearch, L.s);
    L.footer = px(kFooter, L.s);
    L.pad = px(kPad, L.s);
    L.bodyY = L.header + L.search;
    L.bodyH = std::max(0, L.h - L.bodyY - L.footer);
    return L;
}

// 3.0.11-beta.16: a Text row's height - its lines guessed from its letters (GDI breaks them: a letter some 0.56 of
// the words' size, a line a little short for the words it breaks at)
int textHeight(const std::string& words, int width, float s) {
    size_t letters = 0;
    for (unsigned char c : words)
        if ((c & 0xC0) != 0x80) ++letters;
    const float perLine = std::max(10.0f, float(width) / (13.5f * s * 0.56f));
    const int lines = std::max(1, int(std::ceil(float(letters) * 1.08f / perLine)));
    return px(kTextLine, s) * lines + px(8, s);
}

std::vector<Placed> place(const Layout& L, const refpages::Page& p, int& total) {
    std::vector<Placed> out;
    out.reserve(p.rows.size());
    const int innerW = L.w - 2 * L.pad - px(10, L.s);  // room for the scroll bar
    const int gap = px(kGap, L.s);
    const int tileW = (innerW - 2 * gap) / 3;
    int y = px(6, L.s), tile = 0;
    int others = 0;  // 3.0.11-beta.11: the world map's row takes what the other rows leave of the body
    for (const refpages::Row& r : p.rows)
        if (r.style != refpages::Style::World) others += heightOf(r.style, L.s);
    for (const refpages::Row& r : p.rows) {
        int h = heightOf(r.style, L.s);
        if (r.style == refpages::Style::World) h = std::max(h, L.bodyH - others - px(20, L.s));
        if (r.style == refpages::Style::Text) h = textHeight(r.left, innerW, L.s);
        if (r.style == refpages::Style::Tile) {
            const int col = tile % 3;
            if (col == 0 && tile > 0) y += h + gap;
            out.push_back(Placed{L.pad + col * (tileW + gap), y, tileW, h});
            ++tile;
            continue;
        }
        if (tile > 0) {  // after the tiles' last line
            y += heightOf(refpages::Style::Tile, L.s) + gap;
            tile = 0;
        }
        out.push_back(Placed{L.pad, y, innerW, h});
        y += h;
    }
    if (tile > 0) y += heightOf(refpages::Style::Tile, L.s) + gap;
    total = y + px(8, L.s);
    return out;
}

int clampScroll(int scroll, int total, int bodyH) { return std::clamp(scroll, 0, std::max(0, total - bodyH)); }

bool shown(const Layout& L, const Placed& r, int scroll) { return r.y - scroll >= 0 && r.y - scroll + r.h <= L.bodyH; }

int portraitSide(const Layout& L) { return px(kPortrait, L.s); }
int iconSide(const Layout& L) { return px(kIcon, L.s); }

namespace {

const panel::Picture* pictureOf(const Look& look, const refpages::Row& row) {
    if (!look.pictures || row.picture.empty()) return nullptr;
    const auto it = look.pictures->find(row.picture);
    return it == look.pictures->end() || it->second.width <= 0 ? nullptr : &it->second;
}

const panel::Picture* iconOf(const Look& look, const refpages::Row& row) {
    if (!look.pictures || row.icon.empty()) return nullptr;
    const auto it = look.pictures->find("icon:" + row.icon);
    return it == look.pictures->end() || it->second.width <= 0 ? nullptr : &it->second;
}

// A premultiplied picture over the window's (opaque) one
void blit(panel::Picture& out, const panel::Picture& pic, int ox, int oy) {
    for (int y = 0; y < pic.height; ++y) {
        const int ty = oy + y;
        if (ty < 0 || ty >= out.height) continue;
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x;
            const uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (tx < 0 || tx >= out.width || (c >> 24) == 0) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = 0xFF000000u | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
    }
}

}  // namespace

worldmap::Fit worldFit(const worldmap::Sheet& sheet, const Placed& row) {
    return worldmap::fit(sheet.w, sheet.h, row.w, row.h);
}

Under under(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll, bool backShown,
            float x, float y, const worldmap::Sheet* sheet) {
    if (x < 0 || y < 0 || x >= float(L.w) || y >= float(L.h)) return Under{};
    if (closeBox(L).contains(x, y)) return Under{Part::Close, -1};
    if (backShown && backBox(L).contains(x, y)) return Under{Part::Back, -1};
    if (y >= float(L.header) && y < float(L.bodyY)) return Under{Part::Search, -1};
    if (y < float(L.bodyY) || y >= float(L.bodyY + L.bodyH)) return Under{};
    const float by = y - float(L.bodyY) + float(scroll);
    for (size_t i = 0; i < rows.size() && i < p.rows.size(); ++i) {
        const Placed& r = rows[i];
        const bool in = x >= float(r.x) && x < float(r.x + r.w) && by >= float(r.y) && by < float(r.y + r.h);
        if (in && sheet && p.rows[i].style == refpages::Style::World) {  // 3.0.11-beta.11: a map on the world map
            const int t = worldmap::tileAt(*sheet, worldFit(*sheet, r), x - float(r.x), by - float(r.y));
            return t >= 0 ? Under{Part::Tile, int(i), t} : Under{Part::Body, -1};
        }
        if (p.rows[i].link.go != refpages::Go::None && in && shown(L, r, scroll)) return Under{Part::Row, int(i)};
    }
    return Under{Part::Body, -1};
}

namespace {

// 3.0.11-beta.11: a smooth line `width` wide, a rectangle's outline - the world map's marks (the picture's pixels)
void line(panel::Picture& p, float x0, float y0, float x1, float y1, float width, uint32_t rgb, float a) {
    const float half = width / 2, dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    const int left = std::max(0, int(std::floor(std::min(x0, x1) - half - 1)));
    const int right = std::min(p.width - 1, int(std::ceil(std::max(x0, x1) + half + 1)));
    const int top = std::max(0, int(std::floor(std::min(y0, y1) - half - 1)));
    const int bottom = std::min(p.height - 1, int(std::ceil(std::max(y0, y1) + half + 1)));
    for (int y = top; y <= bottom; ++y)
        for (int x = left; x <= right; ++x) {
            const float px0 = float(x) + 0.5f - x0, py0 = float(y) + 0.5f - y0;
            const float t = len2 > 0 ? std::clamp((px0 * dx + py0 * dy) / len2, 0.0f, 1.0f) : 0.0f;
            const float ex = px0 - t * dx, ey = py0 - t * dy;
            const float cover = std::clamp(half + 0.5f - std::sqrt(ex * ex + ey * ey), 0.0f, 1.0f);
            if (cover > 0) panel::over(p.px[size_t(y) * size_t(p.width) + size_t(x)], rgb, cover * a);
        }
}

void outline(panel::Picture& p, const panel::Box& b, float width, uint32_t rgb, float a) {
    line(p, b.x, b.y, b.x + b.w, b.y, width, rgb, a);
    line(p, b.x + b.w, b.y, b.x + b.w, b.y + b.h, width, rgb, a);
    line(p, b.x + b.w, b.y + b.h, b.x, b.y + b.h, width, rgb, a);
    line(p, b.x, b.y + b.h, b.x, b.y, width, rgb, a);
}

panel::Box tileBox(const worldmap::Tile& t, const worldmap::Fit& f, float ox, float oy) {
    return panel::Box{ox + f.x + float(t.l) * f.s, oy + f.y + float(t.t) * f.s, float(t.w) * f.s, float(t.h) * f.s};
}

// The world map's row: its picture, the way followed, your map, the one under the pointer, the players' badges
void paintWorld(panel::Picture& out, const panel::Box& b, const WorldLook& w, float s) {
    if (!w.picture || !w.sheet || w.picture->width != int(b.w) || w.picture->height != int(b.h)) return;
    for (int y = 0; y < w.picture->height; ++y) {
        const int ty = int(b.y) + y;
        if (ty < 0 || ty >= out.height) continue;
        for (int x = 0; x < w.picture->width; ++x) {
            const int tx = int(b.x) + x;
            if (tx >= 0 && tx < out.width)
                out.px[size_t(ty) * size_t(out.width) + size_t(tx)] =
                    w.picture->px[size_t(y) * size_t(w.picture->width) + size_t(x)];
        }
    }
    const worldmap::Fit f = worldmap::fit(w.sheet->w, w.sheet->h, int(b.w), int(b.h));
    const float ox = float(int(b.x)), oy = float(int(b.y));
    auto centre = [&](const std::string& map, float& x, float& y) {
        const worldmap::Tile* t = worldmap::tileOf(*w.sheet, map);
        if (!t) return false;
        const panel::Box tb = tileBox(*t, f, ox, oy);
        x = tb.x + tb.w / 2;
        y = tb.y + tb.h / 2;
        return true;
    };
    float px0 = 0, py0 = 0, px1 = 0, py1 = 0;  // the way followed: from map to map (those on the world sheet)
    bool have = false;
    for (const std::string& m : w.route) {
        if (!centre(m, px1, py1)) continue;
        if (have && (px1 != px0 || py1 != py0)) {
            line(out, px0, py0, px1, py1, 3.5f * s + 2, 0x000000, 0.5f);
            line(out, px0, py0, px1, py1, 3.5f * s, kRoute, 1.0f);
        }
        px0 = px1;
        py0 = py1;
        have = true;
    }
    if (const worldmap::Tile* t = worldmap::tileOf(*w.sheet, w.here))
        outline(out, tileBox(*t, f, ox, oy), 2.5f * s, kGold, 1.0f);
    if (const worldmap::Tile* t = w.hover.empty() ? nullptr : worldmap::tileOf(*w.sheet, w.hover))
        outline(out, tileBox(*t, f, ox, oy), 2 * s, 0xFFFFFF, 0.9f);
    if (w.hereX >= 0 && w.hereY >= 0) {  // you
        const float x = ox + f.x + w.hereX * f.s, y = oy + f.y + w.hereY * f.s;
        panel::circle(out, x, y, 5 * s + 1.5f, 0x000000, 0.85f);
        panel::circle(out, x, y, 5 * s, 0xFFFFFF, 1.0f);
    }
    if (w.players)
        for (const worldmap::Tile& t : w.sheet->world) {
            const auto n = w.players->find(t.map);
            if (n == w.players->end() || n->second <= 0) continue;
            const panel::Box tb = tileBox(t, f, ox, oy);
            panel::circle(out, tb.x + tb.w - 2 * s, tb.y + 2 * s, 9 * s + 1, kRoute, 0.9f);
            panel::circle(out, tb.x + tb.w - 2 * s, tb.y + 2 * s, 9 * s, kBadge, 1.0f);
        }
}

}  // namespace

void paint(panel::Picture& out, const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
           int total, const Look& look) {
    out.width = L.w;
    out.height = L.h;
    out.px.assign(size_t(L.w) * size_t(L.h), 0xFF000000u | kGround);
    const float s = L.s;
    fill(out, panel::Box{0, 0, float(L.w), float(L.header)}, kBand);
    if (look.backShown && look.hover.part == Part::Back) fill(out, backBox(L), kHover);
    if (look.hover.part == Part::Close) fill(out, closeBox(L), 0x5A2330);
    panel::roundRect(out, fieldBox(L), 6 * s, kField, 1.0f, kFieldEdge, 0.9f, std::max(1.0f, 1.5f * s));
    for (size_t i = 0; i < rows.size() && i < p.rows.size(); ++i) {
        const Placed& r = rows[i];
        if (!shown(L, r, scroll)) continue;
        const panel::Box b{float(r.x), float(L.bodyY + r.y - scroll), float(r.w), float(r.h)};
        const refpages::Row& row = p.rows[i];
        const bool hot = look.hover.part == Part::Row && look.hover.row == int(i);
        if (row.style == refpages::Style::Tile) {
            panel::roundRect(out, b, 8 * s, hot ? kTileHot : kTileFill, 1.0f, hot ? kFieldEdge : kRule, 1.0f,
                             std::max(1.0f, s));
            continue;
        }
        if (row.style == refpages::Style::World) {
            paintWorld(out, b, look.world, s);
            continue;
        }
        if (row.style == refpages::Style::Portrait) {  // 3.0.11-beta.14: the picture over a soft square
            if (const panel::Picture* pic = pictureOf(look, row)) {
                panel::roundRect(out, panel::Box{b.x, b.y + 2 * s, b.h - 4 * s, b.h - 4 * s}, 8 * s, kTileFill, 1.0f,
                                 kRule, 1.0f, std::max(1.0f, s));
                blit(out, *pic, int(b.x + (b.h - 4 * s - float(pic->width)) / 2),
                     int(b.y + 2 * s + (b.h - 4 * s - float(pic->height)) / 2));
            }
            continue;
        }
        if (hot) panel::roundRect(out, panel::Box{b.x - 6 * s, b.y, b.w + 12 * s, b.h}, 4 * s, kHover, 1.0f);
        if (row.style == refpages::Style::Heading)
            fill(out, panel::Box{b.x, b.y + b.h - std::max(1.0f, s), b.w, std::max(1.0f, s)}, kRule);
        if (row.dot) panel::circle(out, b.x + 6 * s, b.y + b.h / 2, 4 * s, row.dot, 1.0f);
        if (const panel::Picture* icon = iconOf(look, row))  // 3.0.11-beta.15: an item's icon
            blit(out, *icon, int(b.x), int(b.y + (b.h - float(icon->height)) / 2));
    }
    fill(out, panel::Box{0, float(L.h - L.footer), float(L.w), float(L.footer)}, kBand);
    if (total > L.bodyH && L.bodyH > 0) {  // the scroll bar
        const float trackX = float(L.w) - 10 * s, trackY = float(L.bodyY) + 4 * s, trackH = float(L.bodyH) - 8 * s;
        panel::roundRect(out, panel::Box{trackX, trackY, 5 * s, trackH}, 2.5f * s, kTrack, 1.0f);
        const float thumbH = std::max(24 * s, trackH * float(L.bodyH) / float(total));
        const float at = trackY + (trackH - thumbH) * float(scroll) / float(std::max(1, total - L.bodyH));
        panel::roundRect(out, panel::Box{trackX, at, 5 * s, thumbH}, 2.5f * s, kThumb, 1.0f);
    }
    panel::roundRect(out, panel::Box{0, 0, float(L.w), float(L.h)}, 0, 0, 0.0f, kRule, 1.0f, std::max(1.0f, s));
}

std::vector<Words> words(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
                         const Texts& t, const Look& look) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{panel::Box{float(L.w) * 0.3f, 0, float(L.w) * 0.4f, float(L.header)}, t.title, panel::kText,
                        17 * s, true, Align::Center});
    if (look.backShown && !t.back.empty())
        out.push_back(Words{backBox(L), "‹ " + t.back, look.hover.part == Part::Back ? kAccent : panel::kMuted, 14 * s,
                            false, Align::Left});
    out.push_back(Words{closeBox(L), "×", look.hover.part == Part::Close ? panel::kText : panel::kMuted, 22 * s, false,
                        Align::Center});
    const panel::Box field = fieldBox(L);
    const panel::Box typed{field.x + 12 * s, field.y, field.w - 24 * s, field.h};
    if (t.query.empty())
        out.push_back(Words{typed, "| " + t.hint, panel::kMuted, 15.5f * s, false, Align::Left});
    else
        out.push_back(Words{typed, t.query + "|", panel::kText, 15.5f * s, false, Align::Left});
    for (size_t i = 0; i < rows.size() && i < p.rows.size(); ++i) {
        const Placed& r = rows[i];
        if (!shown(L, r, scroll)) continue;
        const refpages::Row& row = p.rows[i];
        const float x = float(r.x), y = float(L.bodyY + r.y - scroll), w = float(r.w), h = float(r.h);
        const bool hot = look.hover.part == Part::Row && look.hover.row == int(i);
        const bool link = row.link.go != refpages::Go::None;
        switch (row.style) {
            case refpages::Style::Tile:
                out.push_back(Words{panel::Box{x + 14 * s, y + 8 * s, w - 28 * s, h * 0.5f - 6 * s}, row.left,
                                    hot ? kAccent : panel::kText, 16 * s, true, Align::Left});
                out.push_back(Words{panel::Box{x + 14 * s, y + h * 0.5f, w - 28 * s, h * 0.5f - 8 * s}, row.right,
                                    panel::kMuted, 13 * s, false, Align::Left});
                break;
            case refpages::Style::Title:
                out.push_back(Words{panel::Box{x, y, w * 0.72f, h}, row.left, row.gold ? kGold : panel::kText, 24 * s,
                                    true, Align::Left});
                out.push_back(Words{panel::Box{x + w * 0.72f, y, w * 0.28f, h}, row.right, panel::kMuted, 15 * s, false,
                                    Align::Right});
                break;
            case refpages::Style::Sub:
                out.push_back(Words{panel::Box{x, y, w, h}, row.left, panel::kMuted, 13.5f * s, false, Align::Left});
                break;
            case refpages::Style::Heading:
                out.push_back(Words{panel::Box{x, y, w * 0.8f, h}, row.left, kAccent, 15.5f * s, true, Align::Left});
                out.push_back(Words{panel::Box{x + w * 0.8f, y, w * 0.2f, h}, row.right, panel::kMuted, 13 * s, false,
                                    Align::Right});
                break;
            case refpages::Style::Note:
                out.push_back(Words{panel::Box{x, y, w, h}, row.left, panel::kMuted, 13.5f * s, false, Align::Left});
                break;
            case refpages::Style::Text: {  // 3.0.11-beta.16
                Words t{panel::Box{x, y + 3 * s, w, h - 6 * s}, row.left, panel::kText, 13.5f * s, false, Align::Left};
                t.wrap = true;
                out.push_back(t);
                break;
            }
            case refpages::Style::Portrait: {  // two lines beside the picture (from the start without one)
                const float tx = pictureOf(look, row) ? x + h + 10 * s : x;
                out.push_back(Words{panel::Box{tx, y + h * 0.18f, w - (tx - x), h * 0.32f}, row.left, panel::kText,
                                    14 * s, false, Align::Left});
                out.push_back(Words{panel::Box{tx, y + h * 0.52f, w - (tx - x), h * 0.3f}, row.right, panel::kMuted,
                                    13 * s, false, Align::Left});
                break;
            }
            case refpages::Style::World:  // the players' numbers in their badges
                if (look.world.sheet && look.world.players) {
                    const worldmap::Fit f = worldmap::fit(look.world.sheet->w, look.world.sheet->h, int(w), int(h));
                    for (const worldmap::Tile& t : look.world.sheet->world) {
                        const auto n = look.world.players->find(t.map);
                        if (n == look.world.players->end() || n->second <= 0) continue;
                        const panel::Box tb = tileBox(t, f, float(int(x)), float(int(y)));
                        out.push_back(Words{panel::Box{tb.x + tb.w - 11 * s, tb.y - 7 * s, 18 * s, 18 * s},
                                            std::to_string(n->second), panel::kText, 11 * s, true, Align::Center});
                    }
                }
                break;
            case refpages::Style::Row: {
                const float indent = !row.icon.empty() ? float(iconSide(L)) + 6 * s : row.dot ? 16 * s : 0;
                const float split = w * 0.6f;
                const uint32_t rgb = row.gold ? kGold : hot ? kAccent : link ? kLink : panel::kText;
                out.push_back(Words{panel::Box{x + indent, y, split - indent - 8 * s, h}, row.left, rgb, 14 * s,
                                    row.gold, Align::Left});
                out.push_back(Words{panel::Box{x + split, y, w - split, h}, row.right, panel::kMuted, 13 * s, false,
                                    Align::Right});
                break;
            }
        }
    }
    out.push_back(Words{panel::Box{float(L.pad), float(L.h - L.footer), float(L.w - 2 * L.pad), float(L.footer)},
                        t.footer, panel::kMuted, 12.5f * s, false, Align::Center});
    return out;
}

}  // namespace uagfx::refwin
