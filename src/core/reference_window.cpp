#include "core/reference_window.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_icons.hpp"

namespace uagfx::refwin {
namespace {

// At scale 1 (1080p), the target's pixels. 3.0.11-beta.26: the concept's style A (its kit's sizes)
constexpr float kW = 1270, kH = 850, kHeader = 62, kFooter = 44, kPadX = 30, kTop = 22, kEdgeRoom = 20, kRadius = 10;
constexpr float kTitleRow = 46, kSub = 26, kHeading = 38, kRow = 26, kNote = 24, kGroupGap = 18;
constexpr float kSearchH = 54, kTileH = 92, kTileGap = 12, kChipH = 38, kChipGap = 8, kChipPic = 26;
constexpr float kBlockPadX = 16, kBlockPadY = 14, kBlockTitle = 18, kBlockTitleGap = 10;
constexpr float kWorldLeast = 300;  // 3.0.11-beta.11: the world map's row at the least
constexpr float kPortrait = 112;    // 3.0.11-beta.14: a card's picture (3.0.11-beta.26: the concept's head, 112)
constexpr float kTextLine = 19;     // 3.0.11-beta.16: a Text row's line (13.5 px words)
constexpr float kIcon = 24;         // 3.0.11-beta.15: an item's icon in a row (1:1 at 1080p)
// 3.0.11-beta.28: the concept's search results (v21_search) - a result's line, the gap under it, a group's and
// «ще N ›»'s lines, nothing found's; the query's marks (its <mark>: gold .28)
constexpr float kResultH = 50, kResultGap = 4, kGroupH = 28, kMoreH = 28, kEmptyH = 150;
constexpr uint32_t kMark = 0xE9C47C;
constexpr float kMarkA = 0.28f;
// The concept's colours: the window's glass (--card), white over it (--cell .06, --line .09, --line2 .15, the tiles
// .04, the blocks .035), the gold (--accent) of the picked (--sel .16) and its ring (.55)
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000;
constexpr float kCardA = 0.90f, kCellA = 0.06f, kLineA = 0.09f, kLine2A = 0.15f, kTileA = 0.04f, kBlockA = 0.035f;
constexpr float kSelA = 0.16f, kRingA = 0.55f;
constexpr uint32_t kPackDot = 0x9AA3B5, kLiveDot = 0x3CCF7C, kOffDot = 0x5D6475;  // the bottom's chips' dots
constexpr uint32_t kRoute = 0x5AC8FA, kBadge = 0x0D1016;  // the minimap's way colour; the players' badge

int px(float v, float s) { return int(std::lround(v * s)); }

int heightOf(refpages::Style st, float s) {
    switch (st) {
        case refpages::Style::Title: return px(kTitleRow, s);
        case refpages::Style::Sub: return px(kSub, s);
        case refpages::Style::Heading: return px(kHeading, s);
        case refpages::Style::Note: return px(kNote, s);
        case refpages::Style::Tile: return px(kTileH, s);
        case refpages::Style::World: return px(kWorldLeast, s);  // place() gives it the body's rest
        case refpages::Style::Portrait: return px(kPortrait, s);
        case refpages::Style::Text: return px(kTextLine, s);  // place() gives it its lines
        case refpages::Style::Search: return px(kSearchH, s);
        case refpages::Style::Hint: return px(20, s);
        case refpages::Style::Chip: return px(kChipH, s);
        case refpages::Style::Result: return px(kResultH, s);  // 3.0.11-beta.28
        case refpages::Style::Group: return px(kGroupH, s);
        case refpages::Style::More: return px(kMoreH, s);
        case refpages::Style::Empty: return px(kEmptyH, s);
        case refpages::Style::Block:  // place() gives it its rows
        case refpages::Style::Row: break;
    }
    return px(kRow, s);
}

void fill(panel::Picture& p, const panel::Box& b, uint32_t rgb, float a = 1.0f) { panel::roundRect(p, b, 0, rgb, a); }

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

void put(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out += char(cp);
    } else if (cp < 0x800) {
        out += char(0xC0 | (cp >> 6));
        out += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += char(0xE0 | (cp >> 12));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    } else {
        out += char(0xF0 | (cp >> 18));
        out += char(0x80 | ((cp >> 12) & 0x3F));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
}

std::string capitals(const std::string& utf8) {
    std::string out;
    for (size_t i = 0; i < utf8.size();) {
        uint32_t cp = next(utf8, i);
        if (cp >= 'a' && cp <= 'z') cp -= 32;
        else if (cp >= 0x430 && cp <= 0x44F) cp -= 0x20;  // а-я
        else if (cp >= 0x450 && cp <= 0x45F) cp -= 0x50;  // ѐ-џ: ё, є, і, ї
        else if (cp == 0x491) cp = 0x490;                  // ґ
        put(out, cp);
    }
    return out;
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

// 3.0.11-beta.26: a frame's heading - a block, a card's section (a main one has a share; one that folds a part)
bool framed(const refpages::Row& r) {
    return r.style == refpages::Style::Block || (r.style == refpages::Style::Heading && (r.share > 0 || r.fold >= 0));
}

// A card's section that folds: its own line (▸/▾), its rows under it when open
bool folds(const refpages::Row& r) { return r.style == refpages::Style::Heading && r.share == 0 && r.fold >= 0; }

// 3.0.11-beta.28: the results' last line - how the search finds (a hint not right under the search: a line above it)
bool closing(const refpages::Page& p, size_t i) {
    return p.rows[i].style == refpages::Style::Hint && i > 0 && p.rows[i - 1].style != refpages::Style::Search;
}

// The rows a frame holds: those after it up to the next frame, heading, tile, field, title or picture
size_t blockEnd(const refpages::Page& p, size_t block) {
    size_t j = block + 1;
    for (; j < p.rows.size(); ++j) {
        const refpages::Style st = p.rows[j].style;
        if (st == refpages::Style::Block || st == refpages::Style::Tile || st == refpages::Style::Search ||
            st == refpages::Style::Title || st == refpages::Style::Heading || st == refpages::Style::World ||
            st == refpages::Style::Portrait)
            break;
    }
    return j;
}

// A chip's width: its picture, its name, «· its kind», within the block; 3.0.11-beta.26 an element's: its dot, its
// name, its per cent
float chipWidth(const Layout& L, const refpages::Row& r, float most) {
    const float s = L.s;
    float w = 0;
    if (r.dot) {
        w = 8 * s + 9 * s + 6 * s + L.measure(r.left, 13.5f * s, true) + 6 * s + L.measure(r.right, 13.5f * s, true) +
            9 * s;
    } else {
        w = 6 * s + kChipPic * s + 8 * s + L.measure(r.left, 13.5f * s, false) + 10 * s;
        if (!r.sub.empty()) w += 5 * s + L.measure("· " + r.sub, 12 * s, false);
    }
    return std::min(std::ceil(w), most);
}

// 3.0.11-beta.26: a row's height inside a frame - with its icon one line, with a note at its right two (the
// concept's name and its muted line under it), a link alone («усі N ›») a short one
int innerHeight(const refpages::Row& r, int width, float s) {
    switch (r.style) {
        case refpages::Style::Text: return textHeight(r.left, width, s);
        case refpages::Style::Note: return px(kNote, s);
        case refpages::Style::Row:
            if (!r.icon.empty()) return px(34, s);
            return r.right.empty() ? px(28, s) : px(44, s);
        default: return heightOf(r.style, s);
    }
}

// 3.0.11-beta.26: the rail at the window's left - narrow (its icons alone) in a narrow window
int railWidth(const Layout& L) { return px(L.w < px(1150, L.s) ? 58 : 204, L.s); }

struct Column {
    int x = 0, w = 0;
};
// The body's column - right of the rail when the page has one; the scroll bar in its right margin
Column columnOf(const Layout& L, const refpages::Page& p) {
    if (p.rail.empty()) return Column{L.pad, L.w - 2 * L.pad};
    const int x = railWidth(L) + px(24, L.s);
    return Column{x, L.w - x - L.pad};
}

// 3.0.11-beta.26: a card's picture row's tags - each a chip after the other from (x, y); «elN:», «gold:» their
// tints
std::string tagWords(const std::string& tag) {
    if (tag.rfind("gold:", 0) == 0) return tag.substr(5);
    if (tag.size() > 4 && tag.rfind("el", 0) == 0 && tag[3] == ':') return tag.substr(4);
    return tag;
}
std::vector<panel::Box> tagBoxes(const Layout& L, const refpages::Row& r, float x, float y, float right) {
    const float s = L.s;
    std::vector<panel::Box> out;
    for (const std::string& tag : r.tags) {
        const float w = std::ceil(L.measure(tagWords(tag), 13 * s, true)) + 2 + 18 * s;
        if (!out.empty() && x + w > right) break;  // the rest does not fit
        out.push_back(panel::Box{x, y, std::min(w, right - x), 24 * s});
        x += w + 6 * s;
    }
    return out;
}

// A Search row's key at its right («Ctrl+F», «Esc»)
panel::Box searchKey(const Layout& L, const panel::Box& field, const std::string& key) {
    const float s = L.s, w = std::max(24 * s, std::ceil(L.measure(key, 13 * s, true)) + 12 * s), h = 24 * s;
    return panel::Box{field.x + field.w - 14 * s - w, field.y + (field.h - h) / 2, w, h};
}

// A row's «ui:<name>» icon (the tiles', the chips' of a location and a quest)
bool iconNamed(const std::string& key, uiicons::Icon& icon) {
    static const std::pair<const char*, uiicons::Icon> kNames[] = {
        {"ui:monster", uiicons::Icon::Monster}, {"ui:item", uiicons::Icon::Item},
        {"ui:location", uiicons::Icon::Location}, {"ui:quest", uiicons::Icon::Quest},
        {"ui:npc", uiicons::Icon::Npc}, {"ui:mvp", uiicons::Icon::Mvp},
        {"ui:pet", uiicons::Icon::Pet}, {"ui:craft", uiicons::Icon::Craft},
        {"ui:world", uiicons::Icon::World}, {"ui:tables", uiicons::Icon::Tables},
        {"ui:search", uiicons::Icon::Search},
    };
    for (const auto& [name, i] : kNames)
        if (key == name) {
            icon = i;
            return true;
        }
    return false;
}

// A block at (x, y), `w` wide, and its rows inside it: the chips flowing, the others one under another; its height
int placeBlock(const Layout& L, const refpages::Page& p, size_t block, size_t end, int x, int y, int w,
               std::vector<Placed>& out) {
    // 3.0.11-beta.26: a frame at (x, y), `w` wide, and its rows inside it: the chips flowing, the others one under
    // another; a block's name its first line, a section that folds its own line (the rows under a line when open)
    const float s = L.s;
    const refpages::Row& head = p.rows[block];
    const bool fold = folds(head);
    const int padX = px(kBlockPadX, s), padY = px(kBlockPadY, s), innerW = w - 2 * padX;
    int cy = 0;
    if (fold) {
        const int line = px(46, s);
        out[block] = Placed{x, y, w, line, line, false};
        if (head.closed || end == block + 1) return line;
        cy = y + line + px(12, s);
    } else {
        out[block] = Placed{x, y, w, padY + px(kBlockTitle, s), 0, false};
        cy = y + padY + px(kBlockTitle, s) + px(kBlockTitleGap, s);
    }
    int cx = x + padX, chipH = 0, chipGap = 0;
    bool chips = false;  // a line of chips going on
    for (size_t j = block + 1; j < end; ++j) {
        const refpages::Row& r = p.rows[j];
        if (r.style == refpages::Style::Chip) {
            chipH = px(r.dot ? 28 : kChipH, s);
            chipGap = px(r.dot ? 6 : kChipGap, s);
            const int cw = int(chipWidth(L, r, float(innerW)));
            if (chips && cx + cw > x + padX + innerW) {
                cy += chipH + chipGap;
                cx = x + padX;
            }
            out[j] = Placed{cx, cy, cw, chipH, 0, true};
            cx += cw + chipGap;
            chips = true;
            continue;
        }
        if (chips) {
            cy += chipH + chipGap;
            cx = x + padX;
            chips = false;
        }
        const int h = innerHeight(r, innerW, s);
        out[j] = Placed{x + padX, cy, innerW, h, 0, true};
        cy += h;
    }
    if (chips) cy += chipH;
    const int h = cy + padY - y;
    out[block].boxH = h;
    return h;
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

Layout layout(int targetW, int targetH, float scale, Measure measure) {
    Layout L;
    L.s = std::max(0.5f, scale);
    L.measure = measure ? measure : guessWidth;
    const int room = px(kEdgeRoom, L.s);
    L.w = std::max(200, std::min(px(kW, L.s), targetW - 2 * room));
    L.h = std::max(200, std::min(px(kH, L.s), targetH - 2 * room));
    L.x = std::max(0, (targetW - L.w) / 2);
    L.y = std::max(0, (targetH - L.h) / 2);
    L.header = px(kHeader, L.s);
    L.footer = px(kFooter, L.s);
    L.pad = px(kPadX, L.s);
    L.bodyY = L.header;
    L.bodyH = std::max(0, L.h - L.bodyY - L.footer);
    return L;
}

Chrome chrome(const Layout& L, const refpages::Page& p, const Texts& t) {
    Chrome c;
    const float s = L.s, H = float(L.header), W = float(L.w), side = 34 * s;
    const auto width = [&](const std::string& words, float size, bool bold) {
        return words.empty() ? 0.0f : std::ceil(L.measure(words, size * s, bold)) + 2;
    };
    float x = 18 * s;
    c.title = panel::Box{x, 0, width(t.title, 16, true), H};
    x += c.title.w + 12 * s;
    c.close = panel::Box{W - 10 * s - side, (H - side) / 2, side, side};
    float right = c.close.x - 8 * s;
    c.starred = p.star;
    if (p.star >= 0) {
        c.star = panel::Box{right - side, (H - side) / 2, side, side};
        right = c.star.x - 10 * s;
    }
    const bool field = std::any_of(p.rows.begin(), p.rows.end(),
                                   [](const refpages::Row& r) { return r.style == refpages::Style::Search; });
    if (!field) {  // the cards' and the lists' search, in the header (v23_ref_monster)
        const float w = std::min(360 * s, std::max(160 * s, (right - x) * 0.45f)), h = 36 * s;
        c.search = panel::Box{right - w, (H - h) / 2, w, h};
        const float kw = std::max(24 * s, width("Ctrl+F", 13, true) + 8 * s);
        c.kbd = panel::Box{c.search.x + c.search.w - 6 * s - kw, c.search.y + (h - 24 * s) / 2, kw, 24 * s};
        right = c.search.x - 12 * s;
    }
    if (!t.back.empty()) {
        const float w = std::min(8 * s + 16 * s + 6 * s + width(t.back, 14, true) + 12 * s,
                                 std::max(0.0f, right - x) * 0.55f);
        c.back = panel::Box{x, (H - 32 * s) / 2, w, 32 * s};
        x += w + 12 * s;
    }
    // the way here: its last parts that fit, a «›» between each two
    const float sep = width("›", 14.5f, false);
    const auto wide = [&](size_t from) {
        float w = 0;
        for (size_t i = from; i < t.crumbs.size(); ++i)
            w += (i > from ? 16 * s + sep : 0) + width(t.crumbs[i], 14.5f, i + 1 == t.crumbs.size());
        return w;
    };
    while (c.crumbFirst + 1 < t.crumbs.size() && x + wide(c.crumbFirst) > right) ++c.crumbFirst;
    for (size_t i = c.crumbFirst; i < t.crumbs.size(); ++i) {
        if (i > c.crumbFirst) {
            c.crumbs.push_back(panel::Box{x + 8 * s, 0, sep, H});
            x += 16 * s + sep;
        }
        const float w = std::min(width(t.crumbs[i], 14.5f, i + 1 == t.crumbs.size()), std::max(0.0f, right - x));
        c.crumbs.push_back(panel::Box{x, 0, w, H});
        x += w;
    }
    // 3.0.11-beta.26: the rail - «Усе», a line, the sections; narrow: their icons alone
    if (!p.rail.empty()) {
        c.railW = float(railWidth(L));
        c.narrow = L.w < px(1150, s);
        c.railOn = p.railOn;
        const float inset = (c.narrow ? 6 : 10) * s, h = 38 * s;
        float ry = float(L.bodyY) + 12 * s;
        for (size_t i = 0; i < p.rail.size(); ++i) {
            c.rail.push_back(panel::Box{inset, ry, c.railW - 2 * inset, h});
            ry += h + 3 * s + (i == 0 ? 15 * s : 0);
        }
    }
    // the bottom: the data's chips at its left, the keys at its right
    const float fy = float(L.h - L.footer), ch = 22 * s;
    float cx = 18 * s;
    for (const std::string* chip : {&t.pack, &t.live}) {
        if (chip->empty()) {
            c.chips.push_back(panel::Box{});
            continue;
        }
        const float w = 9 * s + 7 * s + 6 * s + width(*chip, 12, true) + 9 * s;
        c.chips.push_back(panel::Box{cx, fy + (float(L.footer) - ch) / 2, w, ch});
        cx += w + 12 * s;
    }
    const float kw = width(t.keys, 12.5f, false);
    c.keys = panel::Box{W - 18 * s - kw, fy, kw, float(L.footer)};
    c.live = t.liveState;
    c.caret = 18 * s + 20 * s + 12 * s + (t.query.empty() ? -4 * s : width(t.query, 17, false));
    return c;
}

std::vector<Placed> place(const Layout& L, const refpages::Page& p, int& total) {
    const float s = L.s;
    const size_t n = p.rows.size();
    std::vector<Placed> out(n);
    const Column col = columnOf(L, p);  // 3.0.11-beta.26: right of the rail
    int others = px(kTop, s);  // 3.0.11-beta.11: the world map's row takes what the other rows leave of the body
    for (const refpages::Row& r : p.rows)
        if (r.style != refpages::Style::World) others += heightOf(r.style, s);
    int y = px(kTop, s);
    for (size_t i = 0; i < n;) {
        const refpages::Row& r = p.rows[i];
        if (r.style == refpages::Style::Tile) {  // 3.0.11-beta.26: five to a line
            const int gap = px(kTileGap, s), h = px(kTileH, s), w = (col.w - 4 * gap) / 5;
            int k = 0;
            for (; i < n && p.rows[i].style == refpages::Style::Tile; ++i, ++k)
                out[i] = Placed{col.x + (k % 5) * (w + gap), y + (k / 5) * (h + gap), w, h};
            y += (k + 4) / 5 * (h + gap) - gap + px(kGroupGap, s);
            continue;
        }
        if (framed(r)) {  // 3.0.11-beta.26: a frame, and the next one beside it (both a share of the line)
            const size_t end = blockEnd(p, i);
            const auto halves = [](const refpages::Row& a) { return a.share > 0 && a.share < 100; };
            const bool pair = halves(r) && end < n && framed(p.rows[end]) && halves(p.rows[end]);
            const int gap = px(r.style == refpages::Style::Block ? kTileGap : 16, s);
            const int w1 = pair ? (col.w - gap) * r.share / (r.share + p.rows[end].share) : col.w;
            int h = placeBlock(L, p, i, end, col.x, y, w1, out);
            size_t next = end;
            if (pair) {
                next = blockEnd(p, end);
                h = std::max(h, placeBlock(L, p, end, next, col.x + w1 + gap, y, col.w - gap - w1, out));
                out[i].boxH = out[end].boxH = h;
            }
            const bool tight = folds(r) && next < n && folds(p.rows[next]);  // the concept's sections close together
            y += h + px(tight ? 8 : 16, s);
            i = next;
            continue;
        }
        if (r.style == refpages::Style::Title && i + 1 < n && p.rows[i + 1].style == refpages::Style::Portrait) {
            // 3.0.11-beta.26: a card's head - its picture, its name, its tags, its line (the concept's)
            const int pic = p.rows[i + 1].picture.empty() ? 0 : px(112, s);
            const int tx = col.x + (pic ? pic + px(18, s) : 0);
            out[i] = Placed{tx, y + px(6, s), col.x + col.w - tx, px(36, s)};
            out[i + 1] = Placed{col.x, y, col.w, std::max(pic, px(106, s))};
            y += out[i + 1].h + px(16, s);
            i += 2;
            continue;
        }
        int h = heightOf(r.style, s);
        if (r.style == refpages::Style::World) h = std::max(h, L.bodyH - others - px(20, s));
        if (r.style == refpages::Style::Text) h = textHeight(r.left, col.w, s);
        if (r.style == refpages::Style::Group && i > 0 && p.rows[i - 1].style != refpages::Style::Search)
            y += px(10, s);  // 3.0.11-beta.28: the next group apart
        if (closing(p, i)) {  // its line, then its words' lines
            const float wide = L.measure(r.left, 13 * s, false) * 1.08f;
            h = px(14, s) + px(19, s) * std::max(1, int(std::ceil(wide / float(std::max(1, col.w)))));
            y += px(6, s);
        }
        out[i] = Placed{col.x, y, col.w, h};
        int gap = 0;
        if (r.style == refpages::Style::Search) gap = px(10, s);
        else if (r.style == refpages::Style::Hint) gap = px(kGroupGap, s);
        else if (r.style == refpages::Style::Result || r.style == refpages::Style::Group ||
                 r.style == refpages::Style::More)
            gap = px(kResultGap, s);
        y += h + gap;
        ++i;
    }
    total = y + px(8, s);
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

// A premultiplied picture over the window's one (3.0.11-beta.25: inside its clip; 3.0.11-beta.26: the glass under
// it keeps showing where the picture lets it)
void blit(panel::Picture& out, const panel::Picture& pic, int ox, int oy) {
    const int top = std::max(0, out.clipY0), bottom = std::min(out.height, out.clipY1);
    const int left = std::max(0, out.clipX0), right = std::min(out.width, out.clipX1);
    for (int y = 0; y < pic.height; ++y) {
        const int ty = oy + y;
        if (ty < top || ty >= bottom) continue;
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x;
            const uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (tx < left || tx >= right || (c >> 24) == 0) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
    }
}

}  // namespace

worldmap::Fit worldFit(const worldmap::Sheet& sheet, const Placed& row) {
    return worldmap::fit(sheet.w, sheet.h, row.w, row.h);
}

Under under(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll, const Chrome& c,
            float x, float y, const worldmap::Sheet* sheet) {
    if (x < 0 || y < 0 || x >= float(L.w) || y >= float(L.h)) return Under{};
    if (c.close.w > 0 && c.close.contains(x, y)) return Under{Part::Close, -1};
    if (c.back.w > 0 && c.back.contains(x, y)) return Under{Part::Back, -1};
    if (c.star.w > 0 && c.star.contains(x, y)) return Under{Part::Star, -1};
    if (c.search.w > 0 && c.search.contains(x, y)) return Under{Part::Search, -1};
    for (size_t i = 0; i < c.rail.size(); ++i)  // 3.0.11-beta.26: the rail's sections
        if (c.rail[i].contains(x, y)) return Under{Part::Rail, int(i)};
    if (y < float(L.bodyY) || y >= float(L.bodyY + L.bodyH) || (c.railW > 0 && x < c.railW)) return Under{};
    const float by = y - float(L.bodyY) + float(scroll);
    for (size_t i = 0; i < rows.size() && i < p.rows.size(); ++i) {
        const Placed& r = rows[i];
        const bool in = x >= float(r.x) && x < float(r.x + r.w) && by >= float(r.y) && by < float(r.y + r.h);
        if (!in) continue;
        if (sheet && p.rows[i].style == refpages::Style::World) {  // 3.0.11-beta.11: a map on the world map
            const int t = worldmap::tileAt(*sheet, worldFit(*sheet, r), x - float(r.x), by - float(r.y));
            return t >= 0 ? Under{Part::Tile, int(i), t} : Under{Part::Body, -1};
        }
        if (p.rows[i].style == refpages::Style::Search) return Under{Part::Search, -1};
        if (p.rows[i].link.go != refpages::Go::None && shown(L, r, scroll)) return Under{Part::Row, int(i)};
        // a block's ground: its chips are the rows after it
    }
    return Under{Part::Body, -1};
}

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

// The world map's row: its picture, the way followed, your map, the one under the pointer, the players' badges
void paintWorld(panel::Picture& out, const panel::Box& b, const WorldLook& w, float s) {
    if (!w.picture || !w.sheet || w.picture->width != int(b.w) || w.picture->height != int(b.h)) return;
    const int top = std::max(0, out.clipY0), bottom = std::min(out.height, out.clipY1);
    const int left = std::max(0, out.clipX0), right = std::min(out.width, out.clipX1);
    for (int y = 0; y < w.picture->height; ++y) {
        const int ty = int(b.y) + y;
        if (ty < top || ty >= bottom) continue;
        for (int x = 0; x < w.picture->width; ++x) {
            const int tx = int(b.x) + x;
            if (tx >= left && tx < right)
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
    if (w.players && w.marks.empty())  // a monster's marks instead of the players while it is shown
        for (const worldmap::Tile& t : w.sheet->world) {
            const auto n = w.players->find(t.map);
            if (n == w.players->end() || n->second <= 0) continue;
            const panel::Box tb = tileBox(t, f, ox, oy);
            panel::circle(out, tb.x + tb.w - 2 * s, tb.y + 2 * s, 9 * s + 1, kRoute, 0.9f);
            panel::circle(out, tb.x + tb.w - 2 * s, tb.y + 2 * s, 9 * s, kBadge, 1.0f);
        }
    for (const auto& [map, n] : w.marks) {  // 3.0.11-beta.19: the monster's maps - gold frames, its numbers
        const worldmap::Tile* t = worldmap::tileOf(*w.sheet, map);
        if (!t) continue;
        const panel::Box tb = tileBox(*t, f, ox, oy);
        outline(out, tb, 2 * s, kGold, 1.0f);
        panel::circle(out, tb.x + tb.w / 2, tb.y + tb.h / 2, 11 * s + 1, kGold, 0.95f);
        panel::circle(out, tb.x + tb.w / 2, tb.y + tb.h / 2, 11 * s, kBadge, 1.0f);
    }
}

constexpr int kAnywhere = 1 << 30;  // panel::Picture's clip by default

// A row under the pointer or picked (3.0.11-beta.21)
bool hot(const Look& look, size_t i) {
    return (look.hover.part == Part::Row && look.hover.row == int(i)) || look.selected == int(i);
}

// 3.0.11-beta.28: a result's parts (the concept's v21_search: its icon's 28 px, its name 14.5 px over its line
// 12.5 px, its kind at its right)
struct ResultBoxes {
    panel::Box icon, name, sub, kind;
};
ResultBoxes resultBoxes(const Layout& L, const refpages::Row& row, const panel::Box& b) {
    const float s = L.s;
    const float kindW = row.right.empty() ? 0 : std::ceil(L.measure(row.right, 12 * s, false)) + 2;
    const float tx = b.x + 48 * s, right = b.x + b.w - 8 * s - (kindW > 0 ? kindW + 12 * s : 0);
    ResultBoxes r;
    r.icon = panel::Box{b.x + 8 * s, b.y + (b.h - 28 * s) / 2, 28 * s, 28 * s};
    r.kind = panel::Box{b.x + b.w - 8 * s - kindW, b.y, kindW, b.h};
    if (row.sub.empty()) {
        r.name = panel::Box{tx, b.y, right - tx, b.h};
    } else {
        r.name = panel::Box{tx, b.y + 7 * s, right - tx, 19 * s};
        r.sub = panel::Box{tx, b.y + 28 * s, right - tx, 16 * s};
    }
    return r;
}

// The query's words marked in a line of words (`box`: where they are drawn, at its left, in its middle)
void marked(panel::Picture& out, const Layout& L, const std::string& text, const refpages::Marks& marks,
            const panel::Box& box, float size, bool bold) {
    const float s = L.s, middle = box.y + box.h / 2;
    for (const auto& [from, to] : marks) {
        if (from >= to || to > text.size()) continue;
        const float x0 = box.x + (from ? L.measure(text.substr(0, from), size, bold) : 0) - 1 * s;
        const float x1 = std::min(box.x + box.w, box.x + L.measure(text.substr(0, to), size, bold) + 1 * s);
        if (x1 > x0) panel::roundRect(out, panel::Box{x0, middle - size * 0.62f, x1 - x0, size * 1.24f}, 3 * s, kMark,
                                      kMarkA);
    }
}

// The header's and the bottom's shapes (3.0.11-beta.26)
void chromeShapes(panel::Picture& out, const Layout& L, const Look& look) {
    const float s = L.s, edge = std::max(1.0f, s);
    const Chrome& c = look.chrome;
    const Part hover = look.hover.part;
    fill(out, panel::Box{0, float(L.header) - edge, float(L.w), edge}, kWhite, kLineA);
    fill(out, panel::Box{0, float(L.h - L.footer), float(L.w), edge}, kWhite, kLineA);
    if (c.back.w > 0) {
        panel::roundRect(out, c.back, 9 * s, kWhite, hover == Part::Back ? 0.12f : kCellA);
        uiicons::draw(out, uiicons::Icon::Back, c.back.x + 8 * s, c.back.y + (c.back.h - 16 * s) / 2, 16 * s, kText,
                      1.0f, 2.4f);
    }
    if (c.search.w > 0) {
        panel::roundRect(out, c.search, 9 * s, kBlack, 0.35f, kWhite, hover == Part::Search ? 0.30f : kLine2A, edge);
        uiicons::draw(out, uiicons::Icon::Search, c.search.x + 12 * s, c.search.y + (c.search.h - 16 * s) / 2,
                      16 * s, kMuted, 1.0f, 2.2f);
        panel::roundRect(out, c.kbd, 5 * s, kWhite, kCellA);
    }
    if (c.star.w > 0) panel::roundRect(out, c.star, 9 * s, kWhite, hover == Part::Star ? 0.12f : kCellA);
    if (hover == Part::Close) panel::roundRect(out, c.close, 9 * s, kWhite, kCellA);
    for (size_t k = 0; k < c.chips.size(); ++k) {
        const panel::Box& b = c.chips[k];
        if (b.w <= 0) continue;
        panel::roundRect(out, b, b.h / 2, kWhite, kCellA);
        const uint32_t dot = k == 0 ? kPackDot : c.live == Live::Fresh ? kLiveDot : kOffDot;
        panel::circle(out, b.x + 12.5f * s, b.y + b.h / 2, 3.5f * s, dot, 1.0f);
    }
}

// Everything over the transparent picture - inside its clip (paint: all of it; repaint: a part)
void shapes(panel::Picture& out, const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
            int total, const Look& look) {
    const float s = L.s, edge = std::max(1.0f, s);
    const Chrome& c = look.chrome;
    // 3.0.11-beta.26: the concept's dark glass, its line around
    panel::roundRect(out, panel::Box{0, 0, float(L.w), float(L.h)}, kRadius * s, kCard, kCardA, kWhite, kLineA, edge);
    chromeShapes(out, L, look);
    if (!c.rail.empty()) {  // the rail: its line at its right, its sections (the page's lit), «Усе»'s line
        fill(out, panel::Box{c.railW - edge, float(L.bodyY), edge, float(L.bodyH)}, kWhite, kLineA);
        for (size_t i = 0; i < c.rail.size() && i < p.rail.size(); ++i) {
            const panel::Box& b = c.rail[i];
            const bool on = int(i) == c.railOn, over = look.hover.part == Part::Rail && look.hover.row == int(i);
            if (on) {
                panel::roundRect(out, b, 9 * s, kAccent, kSelA);
                fill(out, panel::Box{b.x, b.y, 3 * s, b.h}, kAccent);
            } else if (over) {
                panel::roundRect(out, b, 9 * s, kWhite, 0.05f);
            }
            uiicons::Icon icon = uiicons::Icon::Search;
            if (iconNamed(p.rail[i].icon, icon))
                uiicons::draw(out, icon, c.narrow ? b.x + (b.w - 19 * s) / 2 : b.x + 10 * s,
                              b.y + (b.h - 19 * s) / 2, 19 * s, on || over ? kText : kMuted, 1.0f, 2.0f);
            if (i == 0) fill(out, panel::Box{b.x + 4 * s, b.y + b.h + 9 * s, b.w - 8 * s, edge}, kWhite, kLineA);
        }
    }
    // the frames under their rows (3.0.11-beta.26): a block's, a section's - clipped to the body when scrolled in part
    for (size_t i = 0; i < rows.size() && i < p.rows.size(); ++i) {
        const Placed& r = rows[i];
        const refpages::Row& row = p.rows[i];
        if (!framed(row)) continue;
        const float top = float(L.bodyY + r.y - scroll), h = float(r.boxH > 0 ? r.boxH : r.h);
        if (top + h <= float(L.bodyY) || top >= float(L.bodyY + L.bodyH)) continue;
        const int clip0 = out.clipY0, clip1 = out.clipY1;
        out.clipY0 = std::max(out.clipY0, L.bodyY);
        out.clipY1 = std::min(out.clipY1, L.bodyY + L.bodyH);
        const panel::Box b{float(r.x), top, float(r.w), h};
        if (folds(row)) {
            const bool open = !row.closed && r.boxH > r.h;
            panel::roundRect(out, b, 12 * s, kWhite, open ? 0.03f : 0.0f, kWhite, kLineA, edge);
            if (hot(look, i)) panel::roundRect(out, panel::Box{b.x, b.y, b.w, float(r.h)}, 12 * s, kWhite, 0.04f);
            if (open) fill(out, panel::Box{b.x, b.y + float(r.h) - edge, b.w, edge}, kWhite, kLineA);
        } else {
            panel::roundRect(out, b, 12 * s, kWhite, kBlockA, kWhite, kLineA, edge);
        }
        out.clipY0 = clip0;
        out.clipY1 = clip1;
    }
    for (size_t i = 0; i < rows.size() && i < p.rows.size(); ++i) {
        const Placed& r = rows[i];
        if (!shown(L, r, scroll)) continue;
        const panel::Box b{float(r.x), float(L.bodyY + r.y - scroll), float(r.w), float(r.h)};
        const refpages::Row& row = p.rows[i];
        const bool on = hot(look, i);
        uiicons::Icon icon = uiicons::Icon::Search;
        if (framed(row)) continue;  // drawn above
        switch (row.style) {
            case refpages::Style::Search: {  // the concept's big field: gold edged, its icon, its caret, its key
                panel::roundRect(out, b, 12 * s, kBlack, 0.38f, kAccent, 1.0f, 1.5f * s);
                uiicons::draw(out, uiicons::Icon::Search, b.x + 18 * s, b.y + (b.h - 20 * s) / 2, 20 * s, kMuted, 1.0f,
                              2.2f);
                fill(out, panel::Box{b.x + c.caret, b.y + (b.h - 24 * s) / 2, 2 * s, 24 * s}, kAccent2);
                if (!row.right.empty()) panel::roundRect(out, searchKey(L, b, row.right), 5 * s, kWhite, kCellA);
                continue;
            }
            case refpages::Style::Tile:
                if (on)
                    panel::roundRect(out, b, 12 * s, kAccent, kSelA, kAccent, kRingA, edge);
                else
                    panel::roundRect(out, b, 12 * s, kWhite, kTileA, kWhite, kLineA, edge);
                if (iconNamed(row.icon, icon))
                    uiicons::draw(out, icon, b.x + 16 * s, b.y + 14 * s + 1.5f * s, 21 * s, kAccent2, 1.0f, 2.0f);
                continue;
            case refpages::Style::Chip:
                if (row.dot) {  // 3.0.11-beta.26: an element's - green more, red heals, its dot
                    panel::roundRect(out, b, 7 * s, row.tone > 0 ? 0x5BD36C : row.tone < 0 ? 0xFF5A4E : kWhite,
                                     row.tone != 0 ? 0.13f : 0.08f);
                    panel::circle(out, b.x + 12.5f * s, b.y + b.h / 2, 4.5f * s, row.dot, 1.0f);
                    continue;
                }
                if (on)
                    panel::roundRect(out, b, 9 * s, kAccent, kSelA, kAccent, kRingA, edge);
                else
                    panel::roundRect(out, b, 9 * s, kWhite, kCellA);
                if (iconNamed(row.icon, icon)) {
                    uiicons::draw(out, icon, b.x + 6 * s + 3 * s, b.y + (b.h - 20 * s) / 2, 20 * s, kMuted, 1.0f, 2.0f);
                } else if (const panel::Picture* pic = iconOf(look, row)) {
                    const float side = kChipPic * s;
                    blit(out, *pic, int(b.x + 6 * s + (side - float(pic->width)) / 2),
                         int(b.y + (b.h - float(pic->height)) / 2));
                }
                continue;
            case refpages::Style::World:
                paintWorld(out, b, look.world, s);
                continue;
            case refpages::Style::Result: {  // 3.0.11-beta.28: picked - gold, its bar; under the pointer - lighter
                if (look.selected == int(i)) {
                    panel::roundRect(out, b, 9 * s, kAccent, kSelA);
                    panel::roundRect(out, panel::Box{b.x, b.y + 5 * s, 3 * s, b.h - 10 * s}, 1.5f * s, kAccent, 1.0f);
                } else if (on) {
                    panel::roundRect(out, b, 9 * s, kWhite, 0.05f);
                }
                const ResultBoxes parts = resultBoxes(L, row, b);
                if (iconNamed(row.icon, icon)) {
                    uiicons::draw(out, icon, parts.icon.x + 4 * s, parts.icon.y + 4 * s, 20 * s,
                                  row.dot ? row.dot : kMuted, 1.0f, 2.0f);
                } else if (const panel::Picture* pic = iconOf(look, row)) {
                    blit(out, *pic, int(parts.icon.x + (parts.icon.w - float(pic->width)) / 2),
                         int(parts.icon.y + (parts.icon.h - float(pic->height)) / 2));
                }
                marked(out, L, row.left, row.marks, parts.name, 14.5f * s, true);
                if (!row.sub.empty()) marked(out, L, row.sub, row.subMarks, parts.sub, 12.5f * s, false);
                continue;
            }
            case refpages::Style::More:  // 3.0.11-beta.28: «ще N ›»
                if (look.selected == int(i)) {
                    panel::roundRect(out, b, 9 * s, kAccent, kSelA);
                    panel::roundRect(out, panel::Box{b.x, b.y + 5 * s, 3 * s, b.h - 10 * s}, 1.5f * s, kAccent, 1.0f);
                } else if (on) {
                    panel::roundRect(out, b, 9 * s, kWhite, 0.05f);
                }
                continue;
            case refpages::Style::Empty:  // 3.0.11-beta.28: nothing found - the search's sign over the words
                uiicons::draw(out, uiicons::Icon::Search, b.x + (b.w - 34 * s) / 2, b.y + 18 * s, 34 * s, kMuted, 1.0f,
                              1.8f);
                continue;
            case refpages::Style::Group:
                continue;
            case refpages::Style::Hint:
                if (closing(p, i)) fill(out, panel::Box{b.x, b.y, b.w, edge}, kWhite, kLineA);
                continue;
            case refpages::Style::Portrait: {  // 3.0.11-beta.26: a card's head - its picture, its tags
                const float side = 112 * s;
                if (!row.picture.empty()) {
                    panel::roundRect(out, panel::Box{b.x, b.y, side, side}, 14 * s, kBlack, 0.32f);
                    if (const panel::Picture* pic = pictureOf(look, row))
                        blit(out, *pic, int(b.x + (side - float(pic->width)) / 2),
                             int(b.y + (side - float(pic->height)) / 2));
                }
                const float tx = b.x + (row.picture.empty() ? 0 : side + 18 * s);
                const std::vector<panel::Box> tags = tagBoxes(L, row, tx, b.y + 50 * s, b.x + b.w);
                for (size_t k = 0; k < tags.size(); ++k) {
                    const std::string& tag = row.tags[k];
                    uint32_t tint = kWhite;
                    float a = 0.08f;
                    if (tag.rfind("gold:", 0) == 0) {
                        tint = kAccent;
                        a = kSelA;
                    } else if (tag.size() > 4 && tag.rfind("el", 0) == 0 && tag[3] == ':') {
                        tint = refpages::elementColour(tag[2] - '0');
                        a = 0.15f;
                    }
                    panel::roundRect(out, tags[k], 7 * s, tint, a);
                }
                continue;
            }
            default:
                break;
        }
        if (r.in) {  // 3.0.11-beta.26: a row inside a frame - the concept's picked, its icon, a link's underline
            if (on) {
                panel::roundRect(out, panel::Box{b.x - 8 * s, b.y, b.w + 16 * s, b.h}, 6 * s, kAccent, kSelA);
                fill(out, panel::Box{b.x - 8 * s, b.y, 3 * s, b.h}, kAccent);
            }
            const float indent = !row.icon.empty() ? 34 * s : row.dot ? 16 * s : 0;
            if (row.dot) {
                const float middle = row.right.empty() || !row.icon.empty() ? b.h / 2 : 11 * s;
                panel::circle(out, b.x + 6 * s, b.y + middle, 4 * s, row.dot, 1.0f);
            }
            if (const panel::Picture* pic = iconOf(look, row))
                blit(out, *pic, int(b.x + (24 * s - float(pic->width)) / 2), int(b.y + (b.h - float(pic->height)) / 2));
            if (row.style == refpages::Style::Row && row.link.go != refpages::Go::None &&
                (!row.icon.empty() || !row.right.empty())) {
                const float middle = row.icon.empty() ? b.y + 11 * s : b.y + b.h / 2;
                const float wide = std::min(L.measure(row.left, 14 * s, false), b.w - indent);
                fill(out, panel::Box{b.x + indent, middle + 7.5f * s, wide, edge}, kAccent2, 0.55f);
            }
            continue;
        }
        if (on) {  // the concept's picked row: gold-tinted, its gold edge at the left
            panel::roundRect(out, panel::Box{b.x - 6 * s, b.y, b.w + 12 * s, b.h}, 4 * s, kAccent, kSelA);
            fill(out, panel::Box{b.x - 6 * s, b.y, 3 * s, b.h}, kAccent);
        }
        if (row.style == refpages::Style::Heading)
            fill(out, panel::Box{b.x, b.y + b.h - edge, b.w, edge}, kWhite, kLineA);
        if (row.dot) panel::circle(out, b.x + 6 * s, b.y + b.h / 2, 4 * s, row.dot, 1.0f);
        if (const panel::Picture* pic = iconOf(look, row))  // 3.0.11-beta.15: an item's icon
            blit(out, *pic, int(b.x), int(b.y + (b.h - float(pic->height)) / 2));
    }
    if (total > L.bodyH && L.bodyH > 0) {  // the scroll bar: the concept's thumb
        const float trackY = float(L.bodyY) + 10 * s, trackH = float(L.bodyH) - 20 * s;
        const float thumbH = std::max(24 * s, trackH * float(L.bodyH) / float(total));
        const float at = trackY + (trackH - thumbH) * float(scroll) / float(std::max(1, total - L.bodyH));
        panel::roundRect(out, panel::Box{float(L.w) - 12 * s, at, 6 * s, thumbH}, 3 * s, kWhite, 0.22f);
    }
}

// A box of the window in its whole pixels, a pixel more each way (the shapes' smooth edges), inside the window
Rect pixelsOf(const Layout& L, const panel::Box& b) {
    return Rect{std::clamp(int(std::floor(b.x)) - 1, 0, L.w), std::clamp(int(std::floor(b.y)) - 1, 0, L.h),
                std::clamp(int(std::ceil(b.x + b.w)) + 1, 0, L.w), std::clamp(int(std::ceil(b.y + b.h)) + 1, 0, L.h)};
}

}  // namespace

void paint(panel::Picture& out, const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
           int total, const Look& look) {
    out.width = L.w;
    out.height = L.h;
    out.px.assign(size_t(L.w) * size_t(L.h), 0u);  // 3.0.11-beta.26: the glass's rounded corners see through
    out.clipX0 = out.clipY0 = 0;
    out.clipX1 = out.clipY1 = kAnywhere;
    shapes(out, L, p, rows, scroll, total, look);
}

void repaint(panel::Picture& out, const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows,
             int scroll, int total, const Look& look, const Rect& area) {
    if (out.width != L.w || out.height != L.h || out.px.size() != size_t(L.w) * size_t(L.h)) {
        paint(out, L, p, rows, scroll, total, look);
        return;
    }
    const Rect a{std::clamp(area.left, 0, L.w), std::clamp(area.top, 0, L.h), std::clamp(area.right, 0, L.w),
                 std::clamp(area.bottom, 0, L.h)};
    if (a.empty()) return;
    for (int y = a.top; y < a.bottom; ++y) {
        const auto row = out.px.begin() + std::ptrdiff_t(size_t(y) * size_t(L.w));
        std::fill(row + a.left, row + a.right, 0u);
    }
    out.clipX0 = a.left;
    out.clipY0 = a.top;
    out.clipX1 = a.right;
    out.clipY1 = a.bottom;
    shapes(out, L, p, rows, scroll, total, look);
    out.clipX0 = out.clipY0 = 0;
    out.clipX1 = out.clipY1 = kAnywhere;
}

bool sameRow(const refpages::Row& a, const refpages::Row& b) {
    return a.style == b.style && a.left == b.left && a.right == b.right && refpages::sameCard(a.link, b.link) &&
           a.gold == b.gold && a.dot == b.dot && a.picture == b.picture && a.icon == b.icon && a.fold == b.fold &&
           a.sub == b.sub && a.share == b.share && a.marks == b.marks && a.subMarks == b.subMarks;
}

Rect worldArea(const Layout& L, const std::vector<Placed>& rows, int scroll, int row, const worldmap::Sheet& sheet,
               const std::string& map) {
    const worldmap::Tile* t = map.empty() ? nullptr : worldmap::tileOf(sheet, map);
    if (!t || row < 0 || size_t(row) >= rows.size() || !shown(L, rows[size_t(row)], scroll)) return Rect{};
    const Placed& r = rows[size_t(row)];
    const worldmap::Fit f = worldmap::fit(sheet.w, sheet.h, r.w, r.h);
    const panel::Box tb = tileBox(*t, f, float(r.x), float(L.bodyY + r.y - scroll));
    const float out = 2.5f * L.s / 2 + 2;  // the widest frame drawn on a map (yours), its smooth edge
    return pixelsOf(L, panel::Box{tb.x - out, tb.y - out, tb.w + 2 * out, tb.h + 2 * out});
}

Rect rowArea(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll, int i) {
    if (i < 0 || size_t(i) >= rows.size() || !shown(L, rows[size_t(i)], scroll)) return Rect{};
    const Placed& r = rows[size_t(i)];
    const float s = L.s;
    const refpages::Row* row = size_t(i) < p.rows.size() ? &p.rows[size_t(i)] : nullptr;
    const refpages::Style st = row ? row->style : refpages::Style::Row;
    // their own: a tile, a chip, a frame's heading (its line); 3.0.11-beta.26 a row inside a frame: its band
    if (st == refpages::Style::Tile || st == refpages::Style::Chip || st == refpages::Style::Result ||
        st == refpages::Style::More || (row && framed(*row)))  // 3.0.11-beta.28: a result's band its own box
        return pixelsOf(L, panel::Box{float(r.x), float(L.bodyY + r.y - scroll), float(r.w), float(r.h)});
    const float band = (r.in ? 8 : 6) * s;
    return pixelsOf(L,
                    panel::Box{float(r.x) - band, float(L.bodyY + r.y - scroll), float(r.w) + 2 * band, float(r.h)});
}

std::vector<Rect> hotAreas(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
                           const Look& look) {
    std::vector<Rect> out;
    const Chrome& c = look.chrome;
    const Part part = look.hover.part;
    if (part == Part::Back && c.back.w > 0) out.push_back(pixelsOf(L, c.back));
    if (part == Part::Close && c.close.w > 0) out.push_back(pixelsOf(L, c.close));
    if (part == Part::Star && c.star.w > 0) out.push_back(pixelsOf(L, c.star));
    if (part == Part::Search && c.search.w > 0) out.push_back(pixelsOf(L, c.search));
    if (part == Part::Rail && look.hover.row >= 0 && size_t(look.hover.row) < c.rail.size())  // 3.0.11-beta.26
        out.push_back(pixelsOf(L, c.rail[size_t(look.hover.row)]));
    for (const int i : {part == Part::Row ? look.hover.row : -1, look.selected})
        if (i >= 0 && size_t(i) < p.rows.size())
            if (const Rect a = rowArea(L, p, rows, scroll, i); !a.empty()) out.push_back(a);
    return out;
}

std::vector<Words> words(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
                         const Texts& t, const Look& look) {
    std::vector<Words> out;
    const float s = L.s;
    const Chrome& c = look.chrome;
    const Part part = look.hover.part;
    // the header (3.0.11-beta.26): the title, «Назад до …», the way here, the search, the star, the cross
    out.push_back(Words{c.title, t.title, kText, 16 * s, true, Align::Left});
    if (c.back.w > 0)
        out.push_back(Words{panel::Box{c.back.x + 30 * s, c.back.y, c.back.w - 42 * s, c.back.h}, t.back, kText,
                            14 * s, true, Align::Left});
    for (size_t k = 0; k < c.crumbs.size(); ++k) {
        const size_t i = c.crumbFirst + k / 2;
        if (k % 2 == 1) {
            out.push_back(Words{c.crumbs[k], "›", kFaint, 14.5f * s, false, Align::Left});
        } else if (i < t.crumbs.size()) {
            const bool last = i + 1 == t.crumbs.size();
            out.push_back(Words{c.crumbs[k], t.crumbs[i], last ? kText : kMuted, 14.5f * s, last, Align::Left});
        }
    }
    if (c.search.w > 0) {
        const float x = c.search.x + 37 * s;
        out.push_back(Words{panel::Box{x, c.search.y, c.kbd.x - 8 * s - x, c.search.h}, t.find, kFaint, 14 * s, false,
                            Align::Left});
        out.push_back(Words{c.kbd, "Ctrl+F", kText, 13 * s, true, Align::Center});
    }
    if (c.star.w > 0)
        out.push_back(Words{c.star, c.starred > 0 ? "★" : "☆", c.starred > 0 ? kAccent2 : kFaint, 17 * s, false,
                            Align::Center});
    out.push_back(Words{c.close, "✕", part == Part::Close ? kText : kFaint, 16 * s, false, Align::Center});
    for (size_t k = 0; k < c.chips.size(); ++k) {
        const panel::Box& b = c.chips[k];
        if (b.w <= 0) continue;
        out.push_back(Words{panel::Box{b.x + 22 * s, b.y, b.w - 30 * s, b.h}, k == 0 ? t.pack : t.live, kMuted, 12 * s,
                            true, Align::Left});
    }
    if (c.keys.w > 0) out.push_back(Words{c.keys, t.keys, kFaint, 12.5f * s, false, Align::Right});
    if (!c.narrow)  // 3.0.11-beta.26: the rail's sections' names and counts
        for (size_t i = 0; i < c.rail.size() && i < p.rail.size(); ++i) {
            const panel::Box& b = c.rail[i];
            const bool on = int(i) == c.railOn || (part == Part::Rail && look.hover.row == int(i));
            const float countW = p.rail[i].right.empty() ? 0 : std::ceil(L.measure(p.rail[i].right, 12 * s, true)) + 2;
            const float tx = b.x + 39 * s;
            out.push_back(Words{panel::Box{tx, b.y, b.x + b.w - 10 * s - countW - 4 * s - tx, b.h}, p.rail[i].left,
                                on ? kText : kMuted, 14.5f * s, true, Align::Left});
            if (countW > 0)
                out.push_back(Words{panel::Box{b.x + b.w - 10 * s - countW, b.y, countW, b.h}, p.rail[i].right, kFaint,
                                    12 * s, true, Align::Right});
        }
    for (size_t i = 0; i < rows.size() && i < p.rows.size(); ++i) {
        const Placed& r = rows[i];
        if (!shown(L, r, scroll)) continue;
        const refpages::Row& row = p.rows[i];
        const float x = float(r.x), y = float(L.bodyY + r.y - scroll), w = float(r.w), h = float(r.h);
        const bool link = row.link.go != refpages::Go::None;
        if (framed(row) && row.style == refpages::Style::Heading && folds(row)) {  // 3.0.11-beta.26: ▸/▾, its name
            const float nameW = std::ceil(L.measure(row.left, 14.5f * s, true)) + 2;
            out.push_back(Words{panel::Box{x + 16 * s, y, 18 * s, h}, row.closed ? "▸" : "▾", kMuted, 13 * s, false,
                                Align::Left});
            out.push_back(Words{panel::Box{x + 44 * s, y, std::min(nameW, w - 60 * s), h}, row.left, kText, 14.5f * s,
                                true, Align::Left});
            const float sx = x + 44 * s + nameW + 10 * s;
            if (!row.right.empty() && sx < x + w - 24 * s)
                out.push_back(Words{panel::Box{sx, y, x + w - 16 * s - sx, h}, row.right, kMuted, 13 * s, false,
                                    Align::Left});
            continue;
        }
        if (framed(row)) {  // a block's name in capitals, its note at its right in gold
            const float top = y + px(kBlockPadY, s);
            out.push_back(Words{panel::Box{x + 16 * s, top, (w - 32 * s) * 0.65f, 18 * s}, capitals(row.left), kMuted,
                                13 * s, true, Align::Left});
            if (!row.right.empty())
                out.push_back(Words{panel::Box{x + 16 * s, top, w - 32 * s, 18 * s}, row.right, kAccent2, 13 * s, true,
                                    Align::Right});
            continue;
        }
        if (r.in && row.style == refpages::Style::Row) {  // 3.0.11-beta.26: a row inside a frame, as the concept's
            const float indent = !row.icon.empty() ? 34 * s : row.dot ? 16 * s : 0;
            const uint32_t name = row.gold ? kAccent2 : kText;
            if (!row.icon.empty()) {  // one line: its icon, its name, its number in gold at its right
                const float rightW = row.right.empty() ? 0 : std::ceil(L.measure(row.right, 14 * s, true)) + 2;
                out.push_back(Words{panel::Box{x + indent, y, w - indent - rightW - 8 * s, h}, row.left, name, 14 * s,
                                    false, Align::Left});
                if (rightW > 0)
                    out.push_back(Words{panel::Box{x + w - rightW, y, rightW, h}, row.right, kAccent2, 14 * s, true,
                                        Align::Right});
            } else if (!row.right.empty()) {  // two lines: its name, its note muted under it
                out.push_back(Words{panel::Box{x + indent, y, w - indent, 22 * s}, row.left, name, 14 * s, row.gold,
                                    Align::Left});
                out.push_back(Words{panel::Box{x + indent, y + 22 * s, w - indent, 18 * s}, row.right, kMuted,
                                    12.5f * s, false, Align::Left});
            } else {  // a link alone («усі N ›», «Показати на карті світу ›»): gold
                out.push_back(Words{panel::Box{x + indent, y, w - indent, h}, row.left, link ? kAccent2 : kText,
                                    13.5f * s, link, Align::Left});
            }
            continue;
        }
        switch (row.style) {
            case refpages::Style::Search: {
                const panel::Box b{x, y, w, h};
                const panel::Box key = searchKey(L, b, row.right);
                const float tx = x + 50 * s;
                if (t.query.empty())
                    out.push_back(Words{panel::Box{tx, y, key.x - 10 * s - tx, h}, t.hint, kFaint, 17 * s, false,
                                        Align::Left});
                else
                    out.push_back(Words{panel::Box{tx, y, key.x - 10 * s - tx, h}, t.query, kText, 17 * s, false,
                                        Align::Left});
                if (!row.right.empty()) out.push_back(Words{key, row.right, kText, 13 * s, true, Align::Center});
                break;
            }
            case refpages::Style::Hint:
                if (closing(p, i)) {  // 3.0.11-beta.28: the results' last - its lines under its line
                    Words rules{panel::Box{x, y + 12 * s, w, h - 12 * s}, row.left, kMuted, 13 * s, false, Align::Left};
                    rules.wrap = true;
                    out.push_back(rules);
                    break;
                }
                out.push_back(Words{panel::Box{x, y, w, h}, row.left, kMuted, 13 * s, false, Align::Left});
                break;
            case refpages::Style::Result: {  // 3.0.11-beta.28: its name (a boss's gold), its line, its kind
                const ResultBoxes parts = resultBoxes(L, row, panel::Box{x, y, w, h});
                out.push_back(Words{parts.name, row.left, row.gold ? kGold : kText, 14.5f * s, true, Align::Left});
                if (!row.sub.empty())
                    out.push_back(Words{parts.sub, row.sub, kMuted, 12.5f * s, false, Align::Left});
                if (parts.kind.w > 0)
                    out.push_back(Words{parts.kind, row.right, kFaint, 12 * s, false, Align::Right});
                break;
            }
            case refpages::Style::Group:  // 3.0.11-beta.28: the kind's capitals and count, its note at its right
                out.push_back(Words{panel::Box{x, y + 8 * s, w * 0.6f, 20 * s}, capitals(row.left), kMuted, 12 * s,
                                    true, Align::Left});
                if (!row.right.empty())
                    out.push_back(Words{panel::Box{x + w * 0.6f, y + 8 * s, w * 0.4f, 20 * s}, row.right, kMuted,
                                        12 * s, true, Align::Right});
                break;
            case refpages::Style::More:
                out.push_back(Words{panel::Box{x + 8 * s, y, w - 16 * s, h}, row.left,
                                    hot(look, i) ? kText : kMuted, 13 * s, false, Align::Left});
                break;
            case refpages::Style::Empty: {  // 3.0.11-beta.28: what was not found, what to try
                out.push_back(Words{panel::Box{x, y + 60 * s, w, 24 * s}, row.left, kText, 16 * s, true,
                                    Align::Center});
                const float wide = std::min(w, 440 * s);
                Words tip{panel::Box{x + (w - wide) / 2, y + 90 * s, wide, h - 90 * s}, row.sub, kMuted, 13.5f * s,
                          false, Align::Center};
                tip.wrap = true;
                out.push_back(tip);
                break;
            }
            case refpages::Style::Tile: {  // its icon, its name, its count at the right, what it holds under them
                const float countW = row.right.empty() ? 0 : std::ceil(L.measure(row.right, 13 * s, true)) + 2;
                const float tx = x + 16 * s + 21 * s + 10 * s;
                out.push_back(Words{panel::Box{tx, y + 14 * s, x + w - 16 * s - countW - 6 * s - tx, 24 * s}, row.left,
                                    kText, 16 * s, true, Align::Left});
                if (countW > 0)
                    out.push_back(Words{panel::Box{x + w - 16 * s - countW, y + 14 * s, countW, 24 * s}, row.right,
                                        kMuted, 13 * s, true, Align::Right});
                Words sub{panel::Box{x + 16 * s, y + 14 * s + 24 * s + 4 * s, w - 32 * s, 38 * s}, row.sub, kMuted,
                          13 * s, false, Align::Left};
                sub.wrap = true;
                out.push_back(sub);
                break;
            }
            case refpages::Style::Block:
                break;  // framed above
            case refpages::Style::Chip: {
                if (row.dot) {  // 3.0.11-beta.26: an element's - its name, its per cent green more, red heals
                    const float tx = x + 23 * s;
                    const float nameW = std::ceil(L.measure(row.left, 13.5f * s, true)) + 2;
                    out.push_back(Words{panel::Box{tx, y, nameW, h}, row.left, kText, 13.5f * s, true, Align::Left});
                    out.push_back(Words{panel::Box{tx + nameW + 4 * s, y, x + w - 6 * s - (tx + nameW + 4 * s), h},
                                        row.right, row.tone > 0 ? 0x8FE39C : row.tone < 0 ? 0xFF9A8F : kMuted,
                                        13.5f * s, true, Align::Left});
                    break;
                }
                const float tx = x + 6 * s + kChipPic * s + 8 * s;
                const float nameW = std::min(std::ceil(L.measure(row.left, 13.5f * s, false)) + 2, x + w - 10 * s - tx);
                out.push_back(Words{panel::Box{tx, y, nameW, h}, row.left, kText, 13.5f * s, false, Align::Left});
                if (!row.sub.empty() && tx + nameW + 5 * s < x + w - 10 * s)
                    out.push_back(Words{panel::Box{tx + nameW + 3 * s, y, x + w - 8 * s - (tx + nameW + 3 * s), h},
                                        "· " + row.sub, kMuted, 12 * s, false, Align::Left});
                break;
            }
            case refpages::Style::Title:
                if (i + 1 < p.rows.size() && p.rows[i + 1].style == refpages::Style::Portrait) {  // a card's name
                    out.push_back(Words{panel::Box{x, y, w, h}, row.left, row.gold ? kGold : kText, 26 * s, true,
                                        Align::Left});
                    break;
                }
                out.push_back(Words{panel::Box{x, y, w * 0.72f, h}, row.left, row.gold ? kGold : kText, 26 * s, true,
                                    Align::Left});
                out.push_back(Words{panel::Box{x + w * 0.72f, y, w * 0.28f, h}, row.right, kMuted, 15 * s, false,
                                    Align::Right});
                break;
            case refpages::Style::Sub:
                out.push_back(Words{panel::Box{x, y, w, h}, row.left, kMuted, 13.5f * s, false, Align::Left});
                break;
            case refpages::Style::Heading:  // 3.0.11-beta.26: the concept's capitals of a list's part
                out.push_back(Words{panel::Box{x, y, w * 0.8f, h}, capitals(row.left), kMuted, 13 * s, true,
                                    Align::Left});
                out.push_back(Words{panel::Box{x + w * 0.8f, y, w * 0.2f, h}, row.right, kAccent2, 13 * s, true,
                                    Align::Right});
                break;
            case refpages::Style::Note:
                out.push_back(Words{panel::Box{x, y, w, h}, row.left, kMuted, 13.5f * s, false, Align::Left});
                break;
            case refpages::Style::Text: {  // 3.0.11-beta.16
                Words words{panel::Box{x, y + 3 * s, w, h - 6 * s}, row.left, kText, 13.5f * s, false, Align::Left};
                words.wrap = true;
                out.push_back(words);
                break;
            }
            case refpages::Style::Portrait: {  // 3.0.11-beta.26: a card's head - its tags, its line
                const float tx = x + (row.picture.empty() ? 0 : 112 * s + 18 * s);
                const std::vector<panel::Box> tags = tagBoxes(L, row, tx, y + 50 * s, x + w);
                for (size_t k = 0; k < tags.size(); ++k) {
                    const std::string& tag = row.tags[k];
                    uint32_t rgb = kText;
                    if (tag.rfind("gold:", 0) == 0) rgb = kAccent2;
                    else if (tag.size() > 4 && tag.rfind("el", 0) == 0 && tag[3] == ':') rgb = 0xE8EDF5;
                    out.push_back(Words{tags[k], tagWords(tag), rgb, 13 * s, true, Align::Center});
                }
                out.push_back(Words{panel::Box{tx, y + 82 * s, x + w - tx, 22 * s}, row.left, kMuted, 14 * s, false,
                                    Align::Left});
                break;
            }
            case refpages::Style::World:  // the players' numbers in their badges
                if (look.world.sheet && !look.world.marks.empty()) {  // 3.0.11-beta.19: the monster's numbers
                    const worldmap::Fit f = worldmap::fit(look.world.sheet->w, look.world.sheet->h, int(w), int(h));
                    for (const auto& [map, n] : look.world.marks) {
                        const worldmap::Tile* tile = worldmap::tileOf(*look.world.sheet, map);
                        if (!tile) continue;
                        const panel::Box tb = tileBox(*tile, f, float(int(x)), float(int(y)));
                        const panel::Box badge{tb.x + tb.w / 2 - 11 * s, tb.y + tb.h / 2 - 9 * s, 22 * s, 18 * s};
                        out.push_back(Words{badge, std::to_string(n), kGold, 10.5f * s, true, Align::Center});
                    }
                } else if (look.world.sheet && look.world.players) {
                    const worldmap::Fit f = worldmap::fit(look.world.sheet->w, look.world.sheet->h, int(w), int(h));
                    for (const worldmap::Tile& tile : look.world.sheet->world) {
                        const auto n = look.world.players->find(tile.map);
                        if (n == look.world.players->end() || n->second <= 0) continue;
                        const panel::Box tb = tileBox(tile, f, float(int(x)), float(int(y)));
                        out.push_back(Words{panel::Box{tb.x + tb.w - 11 * s, tb.y - 7 * s, 18 * s, 18 * s},
                                            std::to_string(n->second), kText, 11 * s, true, Align::Center});
                    }
                }
                break;
            case refpages::Style::Row: {
                const float indent = !row.icon.empty() ? float(iconSide(L)) + 6 * s : row.dot ? 16 * s : 0;
                const float split = w * 0.6f;
                const uint32_t rgb = row.gold ? kGold : link ? kLink : kText;
                out.push_back(Words{panel::Box{x + indent, y, split - indent - 8 * s, h}, row.left, rgb, 14 * s,
                                    row.gold, Align::Left});
                out.push_back(Words{panel::Box{x + split, y, w - split, h}, row.right, kMuted, 13 * s, false,
                                    Align::Right});
                break;
            }
        }
    }
    return out;
}

}  // namespace uagfx::refwin
