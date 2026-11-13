#include "core/reference_world_paint.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/ui_icons.hpp"

namespace uagfx::refwin {
namespace {

// The concept's colours (its kit): the window's glass (--card), white and black over it, its lines (--line2 .15), the
// words (--text, --muted), the gold (--accent2; a boss's words), the players' green dot
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000;
constexpr float kLine2A = 0.15f;
constexpr uint32_t kText = 0xECEFF5, kMuted = 0x9AA3B5, kAccent2 = 0xE6CB8A, kGold = kAccent2, kLiveDot = 0x3CCF7C;

int px(float v, float s) { return int(std::lround(v * s)); }

// UTF-8's code points (the blocks' names are the concept's capitals: Latin, Cyrillic with Є, І, Ї, Ґ)
uint32_t next(const std::string& s, size_t& i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    int more = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    uint32_t cp = more == 3 ? c & 0x07u : more == 2 ? c & 0x0Fu : more == 1 ? c & 0x1Fu : c;
    ++i;
    for (; more > 0 && i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80; --more, ++i)
        cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3Fu);
    return cp;
}

}  // namespace

float guessWidth(const std::string& utf8, float px, bool bold) {
    float em = 0;
    for (size_t i = 0; i < utf8.size();) {
        const uint32_t cp = next(utf8, i);
        if (cp == ' ') em += 0.27f;
        else if (cp >= '0' && cp <= '9') em += 0.56f;
        else if (cp >= 'A' && cp <= 'Z') em += 0.63f;
        else if (cp >= 'a' && cp <= 'z') em += 0.52f;
        else if (cp >= 0x410 && cp <= 0x42F) em += 0.66f;  // the Cyrillic capitals
        else if (cp >= 0x400 && cp <= 0x4FF) em += 0.56f;
        else if (cp < 0x80) em += 0.32f;  // signs
        else em += 0.62f;
    }
    return em * px * (bold ? 1.12f : 1.0f) * 1.1f;  // rather wider: a preview's font is no narrower
}

int chainSide(const Layout& L) { return px(64, L.s); }  // 3.0.11-beta.64: the concept's .dunin

namespace {

// 3.0.11-beta.64 (the concept's v19_route, its .dunin): the dungeon box at the map's bottom left - its ground, its
// title's line, each map's picture with its name under it, the arrows between them; the first maps left out when
// they do not fit across the map (two at the least); w 0: none
struct ChainBox {
    panel::Box box, title;
    std::vector<size_t> maps;  // their indices in WorldLook::chain
    std::vector<panel::Box> pictures, names;
    std::vector<panel::Box> arrows;
};
ChainBox chainBox(const Layout& L, const WorldLook& w, const panel::Box& b) {
    ChainBox c;
    if (w.chain.size() < 2) return c;
    const float s = L.s, side = float(chainSide(L)), pad = 12 * s, gap = 8 * s, arrow = 14 * s, titleH = 15 * s;
    std::vector<float> widths;
    for (const ChainMap& m : w.chain)
        widths.push_back(std::min(side * 2.2f, std::max(side, std::ceil(L.measure(m.name, 12 * s, true)) + 4 * s)));
    size_t from = 0;
    const auto across = [&] {
        float x = 2 * pad;
        for (size_t i = from; i < widths.size(); ++i) x += widths[i] + (i > from ? 2 * gap + arrow : 0);
        return std::max(x, std::ceil(L.measure(w.chainTitle, 12 * s, true)) + 2 * pad);
    };
    while (from + 2 < widths.size() && across() > b.w - 32 * s) ++from;
    const float height = 10 * s + titleH + 8 * s + side + 4 * s + titleH + 10 * s;
    if (across() > b.w - 32 * s || height > b.h - 32 * s) return c;
    c.box = panel::Box{b.x + 16 * s, b.y + b.h - 16 * s - height, across(), height};
    c.title = panel::Box{c.box.x + pad, c.box.y + 10 * s, c.box.w - 2 * pad, titleH};
    float x = c.box.x + pad;
    const float py = c.title.y + titleH + 8 * s;
    for (size_t i = from; i < w.chain.size(); ++i) {
        if (i > from) {
            c.arrows.push_back(panel::Box{x + gap, py, arrow, side});
            x += 2 * gap + arrow;
        }
        c.maps.push_back(i);
        c.pictures.push_back(panel::Box{x + (widths[i] - side) / 2, py, side, side});
        c.names.push_back(panel::Box{x, py + side + 4 * s, widths[i], titleH});
        x += widths[i];
    }
    return c;
}

}  // namespace

namespace {

// 3.0.11-beta.11: a smooth line `width` wide, a rectangle's outline - the world map's marks (the picture's pixels)
void line(panel::Picture& p, float x0, float y0, float x1, float y1, float width, uint32_t rgb, float a) {
    const float half = width / 2, dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    const int left = std::max({0, p.clipX0, int(std::floor(std::min(x0, x1) - half - 1))});
    const int right = std::min({p.width, p.clipX1, int(std::ceil(std::max(x0, x1) + half + 1)) + 1}) - 1;
    const int top = std::max({0, p.clipY0, int(std::floor(std::min(y0, y1) - half - 1))});
    const int bottom = std::min({p.height, p.clipY1, int(std::ceil(std::max(y0, y1) + half + 1)) + 1}) - 1;
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

// 3.0.11-beta.29: a teleport's arc (the concept's dashed one) - between two maps' middles, bowed up by a fifth of it
struct Arc {
    float ax, ay, cx, cy, bx, by;
    void at(float t, float& x, float& y) const {
        const float u = 1 - t;
        x = u * u * ax + 2 * u * t * cx + t * t * bx;
        y = u * u * ay + 2 * u * t * cy + t * t * by;
    }
};
Arc arcOf(float ax, float ay, float bx, float by) {
    const float dx = bx - ax, dy = by - ay, len = std::sqrt(dx * dx + dy * dy);
    float nx = len > 0 ? -dy / len : 0, ny = len > 0 ? dx / len : 0;
    if (ny > 0) {  // up the picture
        nx = -nx;
        ny = -ny;
    }
    return Arc{ax, ay, (ax + bx) / 2 + nx * len * 0.22f, (ay + by) / 2 + ny * len * 0.22f, bx, by};
}

// 3.0.11-beta.29: what goes over the world map with words - the players' pills, the towns' names, «Ви тут», the
// pin's name, the teleports' prices, the card of the map under the pointer: shapes() draws their grounds, words()
// their words (one place for both)
enum class Over : uint8_t { Players, Town, Here, Pin, Jump, Mark, Tip, Chain };
struct OverItem {
    Over kind;
    panel::Box box;
    std::vector<std::string> words;
};

struct WorldPlace {  // a look's sheet in its row: where a map's middle is
    const WorldLook& w;
    worldmap::Fit f;
    float ox, oy;
    bool centre(const std::string& map, float& x, float& y) const {
        const worldmap::Tile* t = map.empty() || !w.sheet ? nullptr : worldmap::tileOf(*w.sheet, map);
        if (!t) return false;
        const panel::Box tb = tileBox(*t, f, ox, oy);
        x = tb.x + tb.w / 2;
        y = tb.y + tb.h / 2;
        return true;
    }
    // the pin's map: the target, else the way's last map on the sheet (a dungeon's way in)
    std::string pinMap() const {
        if (w.target.empty()) return "";
        float x = 0, y = 0;
        if (centre(w.target, x, y)) return w.target;
        for (auto h = w.hops.rbegin(); h != w.hops.rend(); ++h) {
            if (centre(h->to, x, y)) return h->to;
            if (centre(h->from, x, y)) return h->from;
        }
        return "";
    }
};

// A line of the card of the map under the pointer: from its top (its name 19 high, the others 18; an empty second
// line left out)
float tipLine(const std::vector<std::string>& lines, size_t k, float s) {
    float y = 10 * s;
    for (size_t j = 0; j < k && j < lines.size(); ++j)
        if (j == 0 || !lines[j].empty()) y += j == 0 ? 19 * s : 18 * s;
    return y;
}

panel::Box keptIn(panel::Box b, const panel::Box& in) {
    b.x = std::clamp(b.x, in.x + 4, std::max(in.x + 4, in.x + in.w - b.w - 4));
    b.y = std::clamp(b.y, in.y + 4, std::max(in.y + 4, in.y + in.h - b.h - 4));
    return b;
}

std::vector<OverItem> overItems(const Layout& L, const WorldLook& w, const panel::Box& b) {
    std::vector<OverItem> out;
    if (!w.sheet) return out;
    const float s = L.s;
    const WorldPlace at{w, worldmap::view(w.sheet->w, w.sheet->h, int(b.w), int(b.h), w.zoom, w.cx, w.cy),
                        float(int(b.x)), float(int(b.y))};
    const auto wide = [&](const std::string& words, float size, bool bold) {
        return std::ceil(L.measure(words, size * s, bold)) + 2;
    };
    const std::string pin = at.pinMap();
    if (w.players && w.marks.empty())  // the players now: a green pill at a tile's corner (a dot when it is small)
        for (const worldmap::Tile& t : w.sheet->world) {
            const auto n = w.players->find(t.map);
            if (n == w.players->end() || n->second <= 0) continue;
            const panel::Box tb = tileBox(t, at.f, at.ox, at.oy);
            if (tb.w < 30 * s) {
                out.push_back(OverItem{Over::Players, panel::Box{tb.x + tb.w - 7 * s, tb.y + 1 * s, 6 * s, 6 * s}, {}});
                continue;
            }
            const std::string count = std::to_string(n->second);
            const float pw = 21 * s + wide(count, 11, true), ph = 17 * s;
            out.push_back(OverItem{Over::Players, panel::Box{tb.x + tb.w - pw - 3 * s, tb.y + 3 * s, pw, ph}, {count}});
        }
    for (const auto& [map, n] : w.marks) {  // 3.0.11-beta.19: a monster's numbers - gold pills in the middle
        float x = 0, y = 0;
        if (!at.centre(map, x, y)) continue;
        const std::string count = std::to_string(n);
        const float pw = 14 * s + wide(count, 11, true), ph = 18 * s;
        out.push_back(OverItem{Over::Mark, panel::Box{x - pw / 2, y - ph / 2, pw, ph}, {count}});
    }
    for (const auto& [map, name] : w.labels) {  // the towns' names on their tiles, big enough to hold them
        const worldmap::Tile* t = map == w.here || map == pin ? nullptr : worldmap::tileOf(*w.sheet, map);
        if (!t) continue;
        const panel::Box tb = tileBox(*t, at.f, at.ox, at.oy);
        const float lw = wide(name, 12, true) + 14 * s, lh = 21 * s;
        if (tb.w < 64 * s || lw > tb.w + 30 * s) continue;
        const panel::Box label{tb.x + (tb.w - lw) / 2, tb.y + tb.h - lh - 4 * s, lw, lh};
        out.push_back(OverItem{Over::Town, label, {name}});
    }
    for (const refpages::Hop& h : w.hops) {  // a teleport's price at its arc's top
        float ax = 0, ay = 0, bx = 0, by = 0;
        if (!h.jump || h.label.empty() || !at.centre(h.from, ax, ay) || !at.centre(h.to, bx, by)) continue;
        float mx = 0, my = 0;
        arcOf(ax, ay, bx, by).at(0.5f, mx, my);
        const float lw = wide(h.label, 12, true) + 16 * s, lh = 22 * s;
        out.push_back(OverItem{Over::Jump, keptIn(panel::Box{mx - lw / 2, my - lh - 6 * s, lw, lh}, b), {h.label}});
    }
    if (!w.hereLabel.empty()) {  // «Ви тут · …» under your place
        float x = 0, y = 0;
        const bool cell = w.hereX >= 0 && w.hereY >= 0;
        if (cell) {
            x = at.ox + at.f.x + w.hereX * at.f.s;
            y = at.oy + at.f.y + w.hereY * at.f.s;
        }
        if (cell || at.centre(w.here, x, y)) {
            const float lw = wide(w.hereLabel, 12, true) + 18 * s, lh = 23 * s;
            out.push_back(OverItem{Over::Here, keptIn(panel::Box{x - lw / 2, y + 12 * s, lw, lh}, b), {w.hereLabel}});
        }
    }
    float px0 = 0, py0 = 0;
    if (!pin.empty() && at.centre(pin, px0, py0)) {  // the pin's words over it (a dungeon's: «… → підземелля»)
        const std::string words = !w.targetLabel.empty() ? w.targetLabel : "";
        if (!words.empty()) {
            const float lw = wide(words, 12, true) + 18 * s, lh = 23 * s;
            out.push_back(OverItem{Over::Pin, keptIn(panel::Box{px0 - lw / 2, py0 - 26 * s - lh - 6 * s, lw, lh}, b),
                                   {words}});
        }
    }
    if (!w.hover.empty() && !w.tip.empty()) {  // the card of the map under the pointer, by its tile
        if (const worldmap::Tile* t = worldmap::tileOf(*w.sheet, w.hover)) {
            const panel::Box tb = tileBox(*t, at.f, at.ox, at.oy);
            float cw = 0;
            for (size_t k = 0; k < w.tip.size(); ++k)
                cw = std::max(cw, wide((k == 2 ? "● " : "") + w.tip[k], k == 0 ? 13.5f : 12.5f, k != 1));
            const float ch = tipLine(w.tip, w.tip.size(), s) + 10 * s;
            panel::Box card{tb.x + 8 * s, tb.y + tb.h + 6 * s, cw + 24 * s, ch};
            if (card.y + card.h > b.y + b.h - 4) card.y = tb.y - card.h - 6 * s;
            out.push_back(OverItem{Over::Tip, keptIn(card, b), w.tip});
        }
    }
    // 3.0.11-beta.64: the dungeon box - nothing of the rest under it, the card of the map under the pointer neither
    const ChainBox chain = chainBox(L, w, b);
    if (chain.box.w > 0) {
        const panel::Box& cb = chain.box;
        out.erase(std::remove_if(out.begin(), out.end(),
                                 [&](const OverItem& o) {
                                     const panel::Box& ob = o.box;
                                     return ob.x < cb.x + cb.w && cb.x < ob.x + ob.w && ob.y < cb.y + cb.h &&
                                            cb.y < ob.y + ob.h;
                                 }),
                  out.end());
        out.push_back(OverItem{Over::Chain, cb, {w.chainTitle}});
    }
    // only what is inside the map's row whole (its words are not cut at the edge as the map's shapes are), and
    // nothing under the card of the map under the pointer (their words would show through it)
    const auto tip = std::find_if(out.begin(), out.end(), [](const OverItem& o) { return o.kind == Over::Tip; });
    const panel::Box card = tip == out.end() ? panel::Box{} : tip->box;
    out.erase(std::remove_if(out.begin(), out.end(), [&](const OverItem& o) {
                  const panel::Box& ob = o.box;
                  const bool under = o.kind != Over::Tip && card.w > 0 && ob.x < card.x + card.w &&
                                     card.x < ob.x + ob.w && ob.y < card.y + card.h && card.y < ob.y + ob.h;
                  return under || ob.x < b.x || ob.y < b.y || ob.x + ob.w > b.x + b.w || ob.y + ob.h > b.y + b.h;
              }),
              out.end());
    return out;
}

constexpr uint32_t kWay = 0xF2C14E, kJump = 0xFF7EB6, kPill = 0x2FA562, kDark = 0x0B0E14;

// 3.0.11-beta.64: a premultiplied picture over another at (ox, oy), inside its clip (the dungeon box's minimaps)
void putOver(panel::Picture& out, const panel::Picture& pic, int ox, int oy) {
    if (out.scene) {  // 3.0.18-beta.12: recorded for uaUI
        panel::recordImage(out, pic, float(ox), float(oy), 1.0f);
        return;
    }
    for (int y = 0; y < pic.height; ++y) {
        const int ty = oy + y;
        if (ty < std::max(0, out.clipY0) || ty >= std::min(out.height, out.clipY1)) continue;
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x;
            if (tx < std::max(0, out.clipX0) || tx >= std::min(out.width, out.clipX1)) continue;
            const uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            const auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
    }
}
constexpr uint32_t kWarn = 0xF0A050, kWarnWords = 0xFFD3A1;  // the concept's warning (its --warn)

// The world map's row: its picture at the view, the way (gold; its teleports pink dashed arcs), the maps marked,
// your map, the one under the pointer, then the grounds of what has words (overItems)
void paintWorld(panel::Picture& out, const Layout& L, const panel::Box& b, const WorldLook& w) {
    const float s = L.s;
    if (!w.picture || !w.sheet || w.picture->width != int(b.w) || w.picture->height != int(b.h)) return;
    const int top = std::max(0, out.clipY0), bottom = std::min(out.height, out.clipY1);
    const int left = std::max(0, out.clipX0), right = std::min(out.width, out.clipX1);
    for (int y = 0; y < w.picture->height; ++y) {
        const int ty = int(b.y) + y;
        if (ty < top || ty >= bottom) continue;
        const int x0 = std::max(0, left - int(b.x)), x1 = std::min(w.picture->width, right - int(b.x));
        if (x1 > x0)
            std::copy(w.picture->px.begin() + std::ptrdiff_t(size_t(y) * size_t(w.picture->width) + size_t(x0)),
                      w.picture->px.begin() + std::ptrdiff_t(size_t(y) * size_t(w.picture->width) + size_t(x1)),
                      out.px.begin() + std::ptrdiff_t(size_t(ty) * size_t(out.width) + size_t(int(b.x) + x0)));
    }
    const int clipX0 = out.clipX0, clipY0 = out.clipY0, clipX1 = out.clipX1, clipY1 = out.clipY1;
    out.clipX0 = std::max(out.clipX0, int(b.x));  // nothing of the map's past its row
    out.clipY0 = std::max(out.clipY0, int(b.y));
    out.clipX1 = std::min(out.clipX1, int(b.x + b.w));
    out.clipY1 = std::min(out.clipY1, int(b.y + b.h));
    const WorldPlace at{w, worldmap::view(w.sheet->w, w.sheet->h, int(b.w), int(b.h), w.zoom, w.cx, w.cy),
                        float(int(b.x)), float(int(b.y))};
    for (const auto& [map, n] : w.marks)  // 3.0.11-beta.19: the monster's maps framed in gold
        if (const worldmap::Tile* t = worldmap::tileOf(*w.sheet, map))
            outline(out, tileBox(*t, at.f, at.ox, at.oy), 2 * s, kGold, 1.0f);
    if (const worldmap::Tile* t = worldmap::tileOf(*w.sheet, w.here))
        outline(out, tileBox(*t, at.f, at.ox, at.oy), 2.5f * s, kGold, 1.0f);
    for (const refpages::Hop& h : w.hops) {  // the way: gold over a dark edge; a teleport a dashed pink arc
        float ax = 0, ay = 0, bx = 0, by = 0;
        if (!at.centre(h.from, ax, ay) || !at.centre(h.to, bx, by) || (ax == bx && ay == by)) continue;
        if (!h.jump) {
            line(out, ax, ay, bx, by, 6.5f * s, kBlack, 0.55f);
            line(out, ax, ay, bx, by, 4 * s, kWay, 1.0f);
            continue;
        }
        const Arc arc = arcOf(ax, ay, bx, by);
        const float len = std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay)) * 1.1f;
        const int steps = std::max(8, int(len / (3 * s)));
        float walked = 0, x0 = ax, y0 = ay;
        for (int k = 1; k <= steps; ++k) {
            float x1 = 0, y1 = 0;
            arc.at(float(k) / float(steps), x1, y1);
            const float piece = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
            if (std::fmod(walked, 15 * s) < 9 * s) {  // dashes 9 on, 6 off
                line(out, x0, y0, x1, y1, 5 * s, kBlack, 0.35f);
                line(out, x0, y0, x1, y1, 3 * s, kJump, 1.0f);
            }
            walked += piece;
            x0 = x1;
            y0 = y1;
        }
    }
    if (const worldmap::Tile* t = w.hover.empty() ? nullptr : worldmap::tileOf(*w.sheet, w.hover))
        outline(out, tileBox(*t, at.f, at.ox, at.oy), 2 * s, kWhite, 0.9f);
    float pinX = 0, pinY = 0;
    if (at.centre(at.pinMap(), pinX, pinY)) {  // the way's end: a gold pin
        line(out, pinX, pinY - 12 * s, pinX, pinY - 1 * s, 5 * s, kWay, 1.0f);
        panel::circle(out, pinX, pinY - 19 * s, 10 * s + 1.5f, kBlack, 0.6f);
        panel::circle(out, pinX, pinY - 19 * s, 10 * s, kWay, 1.0f);
        panel::circle(out, pinX, pinY - 19 * s, 3.5f * s, kDark, 1.0f);
    }
    if (w.hereX >= 0 && w.hereY >= 0) {  // you
        const float x = at.ox + at.f.x + w.hereX * at.f.s, y = at.oy + at.f.y + w.hereY * at.f.s;
        panel::circle(out, x, y, 5 * s + 1.5f, kBlack, 0.85f);
        panel::circle(out, x, y, 5 * s, kWhite, 1.0f);
    }
    for (const OverItem& o : overItems(L, w, b)) {
        const panel::Box& ob = o.box;
        switch (o.kind) {
            case Over::Players:
                if (o.words.empty()) {
                    panel::circle(out, ob.x + ob.w / 2, ob.y + ob.h / 2, ob.w / 2, kLiveDot, 1.0f);
                    break;
                }
                panel::roundRect(out, ob, ob.h / 2, kPill, 0.94f);
                uiicons::draw(out, uiicons::Icon::Npc, ob.x + 4 * s, ob.y + (ob.h - 11 * s) / 2, 11 * s, kWhite, 1.0f,
                              2.4f);
                break;
            case Over::Mark: panel::roundRect(out, ob, ob.h / 2, kWay, 0.96f, kBlack, 0.5f, 1.0f); break;
            case Over::Town: panel::roundRect(out, ob, 6 * s, kDark, 0.82f); break;
            case Over::Here:
            case Over::Pin: panel::roundRect(out, ob, 7 * s, kDark, 0.9f, kWay, 0.9f, 1.5f * s); break;
            case Over::Jump: panel::roundRect(out, ob, 7 * s, kDark, 0.9f, kJump, 0.8f, 1.2f * s); break;
            case Over::Tip:
                panel::roundRect(out, ob, 9 * s, kCard, 0.97f, kWhite, kLine2A, std::max(1.0f, s));
                if (o.words.size() > 2)  // the players' line: its green dot
                    panel::circle(out, ob.x + 15 * s, ob.y + tipLine(o.words, 2, s) + 9 * s, 3.5f * s, kLiveDot, 1.0f);
                break;
            case Over::Chain: {  // 3.0.11-beta.64: its ground, the maps' pictures ringed in gold (the end glowing)
                panel::roundRect(out, ob, 10 * s, kDark, 0.93f, kWhite, kLine2A, std::max(1.0f, s));
                const ChainBox chain = chainBox(L, w, b);
                for (size_t k = 0; k < chain.maps.size(); ++k) {
                    const panel::Box& pb = chain.pictures[k];
                    const ChainMap& m = w.chain[chain.maps[k]];
                    const bool end = chain.maps[k] + 1 == w.chain.size();
                    if (end) outline(out, pb, 7 * s, kWay, 0.25f);  // its glow
                    panel::roundRect(out, pb, 6 * s, worldmap::kGround & 0xFFFFFF, 1.0f);
                    if (m.picture && m.picture->width == int(pb.w + 0.5f))
                        putOver(out, *m.picture, int(std::lround(pb.x)), int(std::lround(pb.y)));
                    outline(out, pb, (end ? 2.5f : 2) * s, kWay, 1.0f);
                }
                break;
            }
        }
    }
    out.clipX0 = clipX0;
    out.clipY0 = clipY0;
    out.clipX1 = clipX1;
    out.clipY1 = clipY1;
}

// The words of what goes over the world map (overItems), a row's at (x, y)
void worldWords(std::vector<Words>& out, const Layout& L, const WorldLook& w, const panel::Box& b) {
    const float s = L.s;
    for (const OverItem& o : overItems(L, w, b)) {
        const panel::Box& ob = o.box;
        if (o.words.empty()) continue;
        switch (o.kind) {
            case Over::Players:
                out.push_back(Words{panel::Box{ob.x + 17 * s, ob.y, ob.w - 20 * s, ob.h}, o.words[0], kWhite, 11 * s,
                                    true, Align::Left});
                break;
            case Over::Mark:
                out.push_back(Words{ob, o.words[0], 0x17120A, 11 * s, true, Align::Center});
                break;
            case Over::Town: out.push_back(Words{ob, o.words[0], kWhite, 12 * s, true, Align::Center}); break;
            case Over::Here:
            case Over::Pin: out.push_back(Words{ob, o.words[0], kAccent2, 12 * s, true, Align::Center}); break;
            case Over::Jump: out.push_back(Words{ob, o.words[0], kJump, 12 * s, true, Align::Center}); break;
            case Over::Tip:
                for (size_t k = 0; k < o.words.size(); ++k) {
                    if (o.words[k].empty()) continue;
                    const panel::Box line{ob.x + 12 * s + (k == 2 ? 12 * s : 0), ob.y + tipLine(o.words, k, s),
                                          ob.w - 24 * s, k == 0 ? 19 * s : 18 * s};
                    out.push_back(Words{line, o.words[k], k == 0 ? kText : k == 1 ? kMuted : kLiveDot,
                                        k == 0 ? 13.5f * s : 12.5f * s, k != 1, Align::Left});
                }
                break;
            case Over::Chain: {  // 3.0.11-beta.64: its title, the maps' names, the gold arrows
                const ChainBox chain = chainBox(L, w, b);
                out.push_back(Words{chain.title, o.words[0], kMuted, 12 * s, true, Align::Left});
                for (size_t k = 0; k < chain.maps.size(); ++k)
                    out.push_back(Words{chain.names[k], w.chain[chain.maps[k]].name, kText, 12 * s, true,
                                        Align::Center});
                for (const panel::Box& a : chain.arrows)
                    out.push_back(Words{a, "\xE2\x86\x92", kWay, 16 * s, true, Align::Center});
                break;
            }
        }
    }
}

}  // namespace

// 3.0.18-beta.239: the world map's box alone for the live window - its dungeon box first, then the tiles
Under worldUnder(const Layout& L, const panel::Box& b, const WorldLook& w, float x, float y) {
    if (!w.sheet) return Under{};
    const ChainBox chain = chainBox(L, w, panel::Box{0, 0, b.w, b.h});
    if (chain.box.w > 0 && chain.box.contains(x, y)) {
        for (size_t k = 0; k < chain.maps.size(); ++k) {
            const panel::Box& pb = chain.pictures[k];
            const panel::Box& nb = chain.names[k];
            if (x >= nb.x && x < nb.x + nb.w && y >= pb.y && y < nb.y + nb.h)
                return Under{Part::Chain, 0, int(chain.maps[k])};
        }
        return Under{Part::Body, -1};
    }
    const worldmap::Fit f = worldmap::view(w.sheet->w, w.sheet->h, int(b.w), int(b.h), w.zoom, w.cx, w.cy);
    const int t = worldmap::tileAt(*w.sheet, f, x, y);
    return t >= 0 ? Under{Part::Tile, 0, t} : Under{Part::Body, -1};
}

std::vector<Words> worldWordsIn(const Layout& L, const WorldLook& w, const panel::Box& b) {
    std::vector<Words> out;
    worldWords(out, L, w, b);
    return out;
}

void paintWorldPicture(panel::Picture& out, const Layout& L, const panel::Box& b, const WorldLook& w) {
    panel::blank(out, int(b.w), int(b.h));
    paintWorld(out, L, panel::Box{0, 0, b.w, b.h}, w);
}

}  // namespace uagfx::refwin
