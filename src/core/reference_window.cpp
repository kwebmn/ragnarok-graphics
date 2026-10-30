#include "core/reference_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

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
// 3.0.11-beta.29: the concept's world map (v19_route) - its bar's line, its panel at the right, the panel's rows
constexpr float kBarH = 34, kPanelW = 380, kPanelGap = 20, kStepH = 44, kButtonH = 36;
constexpr uint32_t kMark = 0xE9C47C;
constexpr float kMarkA = 0.28f;
// The concept's colours: the window's glass (--card), white over it (--cell .06, --line .09, --line2 .15, the tiles
// .04, the blocks .035), the gold (--accent) of the picked (--sel .16) and its ring (.55)
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000;
constexpr float kCellA = 0.06f, kLineA = 0.09f, kLine2A = 0.15f, kTileA = 0.04f, kBlockA = 0.035f;
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
        case refpages::Style::Tab:  // 3.0.11-beta.29
        case refpages::Style::Tool: return px(kBarH, s);
        case refpages::Style::Caption: return px(30, s);
        case refpages::Style::Step: return px(kStepH, s);
        case refpages::Style::Legend: return px(24, s);
        case refpages::Style::Button: return px(kButtonH, s);
        case refpages::Style::Grid: return px(28, s);  // 3.0.11-beta.32
        case refpages::Style::Warn:  // place() gives it its lines
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
        {"ui:search", uiicons::Icon::Search}, {"ui:dungeon", uiicons::Icon::Dungeon},
        {"ui:minus", uiicons::Icon::Minus}, {"ui:plus", uiicons::Icon::Plus},
        {"ui:expand", uiicons::Icon::Expand}, {"ui:home", uiicons::Icon::Home},
    };
    for (const auto& [name, i] : kNames)
        if (key == name) {
            icon = i;
            return true;
        }
    return false;
}

// 3.0.11-beta.29: a bar's tab (its icon, its words) or its chip (its dot, its words); a panel's button
float tabWidth(const Layout& L, const refpages::Row& r) {
    const float s = L.s, words = std::ceil(L.measure(r.left, 13.5f * s, true)) + 2;
    if (r.link.go == refpages::Go::None) return 12 * s + 8 * s + 7 * s + words + 12 * s;  // its dot
    return 14 * s + 16 * s + 8 * s + words + 14 * s;  // its icon
}
float buttonWidth(const Layout& L, const refpages::Row& r) {
    return std::ceil(L.measure(r.left, 14 * L.s, true)) + 2 + 32 * L.s;
}

// Words wrapped in a width: their lines' height (13 px words, 19 px a line)
int wrappedHeight(const Layout& L, const std::string& words, float width) {
    const float wide = L.measure(words, 13 * L.s, false) * 1.08f;
    return px(19, L.s) * std::max(1, int(std::ceil(wide / std::max(1.0f, width))));
}

// The bar of the world map's pages: its tabs and its chip from the left, its zoom's buttons at the map's right
// (`mapRight`); the rows after it
size_t placeBar(const Layout& L, const refpages::Page& p, int x, int mapRight, int& y, std::vector<Placed>& out) {
    const float s = L.s;
    size_t i = 0;
    std::vector<size_t> tools;
    for (; i < p.rows.size(); ++i) {
        const refpages::Row& r = p.rows[i];
        if (r.style == refpages::Style::Tool) {
            tools.push_back(i);
            continue;
        }
        if (r.style != refpages::Style::Tab) break;
        const int w = int(tabWidth(L, r));
        out[i] = Placed{x, y, w, px(kBarH, s)};
        x += w + px(8, s);
    }
    for (auto t = tools.rbegin(); t != tools.rend(); ++t) {
        mapRight -= px(kBarH, s);
        out[*t] = Placed{mapRight, y, px(kBarH, s), px(kBarH, s)};
        mapRight -= px(6, s);
    }
    if (i > 0) y += px(kBarH, s) + px(12, s);
    return i;
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
    if (t.home) {  // 3.0.11-beta.30 (the owner: «нету кнопки перейти на главный экран справочника»)
        c.home = panel::Box{x, (H - 32 * s) / 2, 32 * s, 32 * s};
        x += c.home.w + 8 * s;
    }
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
    int y = px(kTop, s);
    // 3.0.11-beta.29, the concept's world map (v19_route): its bar, the map at the left, the panel at its right
    const auto world = std::find_if(p.rows.begin(), p.rows.end(),
                                    [](const refpages::Row& r) { return r.style == refpages::Style::World; });
    const int panelW = world == p.rows.end() ? 0 : std::min(px(kPanelW, s), col.w * 2 / 5);
    const int mapW = panelW ? col.w - panelW - px(kPanelGap, s) : col.w;
    size_t start = placeBar(L, p, col.x, col.x + mapW, y, out);
    if (world != p.rows.end()) {
        const size_t at = size_t(world - p.rows.begin());
        for (; start < at; ++start) {  // anything between the bar and the map
            out[start] = Placed{col.x, y, mapW, heightOf(p.rows[start].style, s)};
            y += out[start].h;
        }
        out[at] = Placed{col.x, y, mapW, std::max(px(kWorldLeast, s), L.bodyH - y - px(12, s))};
        const int px0 = col.x + mapW + px(kPanelGap, s);
        int py = px(kTop, s), bx = px0;
        bool buttons = false;  // a line of buttons going on
        for (size_t j = at + 1; j < n; ++j) {
            const refpages::Row& r = p.rows[j];
            if (r.style == refpages::Style::Button) {
                const int w = int(buttonWidth(L, r));
                if (buttons && bx + w > px0 + panelW) {
                    py += px(kButtonH + 8, s);
                    bx = px0;
                }
                if (!buttons) py += px(6, s);
                out[j] = Placed{bx, py, w, px(kButtonH, s)};
                bx += w + px(8, s);
                buttons = true;
                continue;
            }
            if (buttons) {
                py += px(kButtonH + 12, s);
                bx = px0;
                buttons = false;
            }
            int h = heightOf(r.style, s);
            if (r.style == refpages::Style::Warn) h = wrappedHeight(L, r.left, float(panelW) - 46 * s) + px(22, s);
            if (r.style == refpages::Style::Text) h = textHeight(r.left, panelW, s);
            if (r.style == refpages::Style::Row) h = px(34, s);
            out[j] = Placed{px0, py, panelW, h};
            int gap = 0;
            if (r.style == refpages::Style::Sub || r.style == refpages::Style::Warn) gap = px(12, s);
            if (r.style == refpages::Style::Text) gap = px(10, s);
            if (r.style == refpages::Style::Legend && j + 1 < n && p.rows[j + 1].style != refpages::Style::Legend)
                gap = px(10, s);
            if (r.style == refpages::Style::Row && j + 1 < n && p.rows[j + 1].style != refpages::Style::Row)
                gap = px(10, s);
            py += h + gap;
        }
        if (buttons) py += px(kButtonH, s);
        total = std::max(out[at].y + out[at].h, py) + px(8, s);
        return out;
    }
    int others = y;  // 3.0.11-beta.11: the world map's row takes what the other rows leave of the body
    for (size_t i = start; i < n; ++i)
        if (p.rows[i].style != refpages::Style::World) others += heightOf(p.rows[i].style, s);
    for (size_t i = start; i < n;) {
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

worldmap::Fit worldView(const WorldLook& w, const Placed& row) {
    return w.sheet ? worldmap::view(w.sheet->w, w.sheet->h, row.w, row.h, w.zoom, w.cx, w.cy) : worldmap::Fit{};
}

Under under(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll, const Chrome& c,
            float x, float y, const WorldLook* world) {
    if (x < 0 || y < 0 || x >= float(L.w) || y >= float(L.h)) return Under{};
    if (c.close.w > 0 && c.close.contains(x, y)) return Under{Part::Close, -1};
    if (c.home.w > 0 && (c.home.contains(x, y) || c.title.contains(x, y))) return Under{Part::Home, -1};
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
        if (world && world->sheet && p.rows[i].style == refpages::Style::World) {  // a map on the world map
            const int t = worldmap::tileAt(*world->sheet, worldView(*world, r), x - float(r.x), by - float(r.y));
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
enum class Over : uint8_t { Players, Town, Here, Pin, Jump, Mark, Tip };
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
        }
    }
}

constexpr int kAnywhere = 1 << 30;  // panel::Picture's clip by default

// A row under the pointer or picked (3.0.11-beta.21)
bool hot(const Look& look, size_t i) {
    return (look.hover.part == Part::Row && look.hover.row == int(i)) || look.selected == int(i);
}

// 3.0.11-beta.32: a table's cell (refpages Style::Grid): its box, the colour of its per cent (more green, less red,
// below 0 - it heals - violet, 100 none)
panel::Box gridCell(const panel::Box& row, size_t k, size_t n, float s) {
    const float label = 104 * s, w = (row.w - label) / float(std::max<size_t>(1, n));
    return panel::Box{row.x + label + float(k) * w + 1.5f * s, row.y + 2 * s, w - 3 * s, row.h - 4 * s};
}
// A column's name (bold) whole when it fits `width`, else its longest beginning that does - 3 letters at the least
std::string gridHead(const Layout& L, const std::string& name, float size, float width) {
    std::string out = name;
    size_t letters = 0;
    for (const char c : out) letters += (uint8_t(c) & 0xC0) != 0x80;
    while (letters > 3 && L.measure(out, size, true) > width) {
        size_t cut = out.size() - 1;
        while (cut > 0 && (uint8_t(out[cut]) & 0xC0) == 0x80) --cut;  // the last letter's first byte
        out.resize(cut);
        --letters;
    }
    return out;
}
void gridTint(int v, uint32_t& ground, float& a, uint32_t& words) {
    if (v > 100) {
        ground = 0x5BD36C, a = std::clamp(0.12f + float(v - 100) / 300.0f, 0.12f, 0.5f), words = 0x8FE39C;
    } else if (v < 0) {
        ground = 0xB28CFF, a = std::clamp(0.2f + float(-v) / 300.0f, 0.2f, 0.5f), words = 0xD8C2FF;
    } else if (v < 100) {
        ground = 0xFF5A4E, a = std::clamp(0.10f + float(100 - v) / 280.0f, 0.1f, 0.45f), words = 0xFF9A8F;
    } else {
        ground = 0xFFFFFF, a = 0.035f, words = kMuted;
    }
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
    if (c.home.w > 0) {  // 3.0.11-beta.30: «Головна»
        panel::roundRect(out, c.home, 9 * s, kWhite, hover == Part::Home ? 0.12f : kCellA);
        uiicons::draw(out, uiicons::Icon::Home, c.home.x + (c.home.w - 17 * s) / 2, c.home.y + (c.home.h - 17 * s) / 2,
                      17 * s, kText, 1.0f, 2.2f);
    }
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
    const panel::Box ground{0, 0, float(L.w), float(L.h)};
    panel::roundRect(out, ground, kRadius * s, kCard, uistyle::window(), kWhite, kLineA, edge);
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
                paintWorld(out, L, b, look.world);
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
            case refpages::Style::Tab:  // 3.0.11-beta.29: the world map's tab (the page's gold) or its green chip
                if (row.link.go == refpages::Go::None) {
                    panel::roundRect(out, b, b.h / 2, kLiveDot, 0.12f);
                    panel::circle(out, b.x + 16 * s, b.y + b.h / 2, 4 * s, row.dot ? row.dot : kLiveDot, 1.0f);
                    continue;
                }
                if (row.gold)
                    panel::roundRect(out, b, 9 * s, kAccent, kSelA, kAccent, kRingA, edge);
                else
                    panel::roundRect(out, b, 9 * s, kWhite, on ? 0.10f : kCellA);
                if (iconNamed(row.icon, icon))
                    uiicons::draw(out, icon, b.x + 14 * s, b.y + (b.h - 16 * s) / 2, 16 * s,
                                  row.gold ? kAccent2 : kMuted, 1.0f, 2.0f);
                continue;
            case refpages::Style::Tool:  // its zoom's
                panel::roundRect(out, b, 8 * s, kWhite, on ? 0.12f : kCellA);
                if (iconNamed(row.icon, icon))
                    uiicons::draw(out, icon, b.x + (b.w - 16 * s) / 2, b.y + (b.h - 16 * s) / 2, 16 * s, kText, 1.0f,
                                  2.2f);
                continue;
            case refpages::Style::Caption:
                continue;
            case refpages::Style::Warn:
                panel::roundRect(out, b, 9 * s, kWarn, 0.10f, kWarn, 0.35f, edge);
                uiicons::draw(out, uiicons::Icon::Warn, b.x + 11 * s, b.y + 11 * s, 16 * s, kWarn, 1.0f, 2.0f);
                continue;
            case refpages::Style::Step: {  // its number in a circle (a paid move's pink), a line under it
                if (on) panel::roundRect(out, b, 8 * s, kWhite, 0.05f);
                const float cx = b.x + 14 * s, cy = b.y + b.h / 2;
                panel::circle(out, cx, cy, 11 * s, row.tone > 0 ? kJump : kWhite, row.tone > 0 ? 0.9f : 0.10f);
                fill(out, panel::Box{b.x + 36 * s, b.y + b.h - edge, b.w - 36 * s, edge}, kWhite, kLineA);
                continue;
            }
            case refpages::Style::Legend: {  // the mark it tells of
                const float y = b.y + b.h / 2, x0 = b.x + 2 * s, x1 = b.x + 26 * s;
                if (row.icon == "mark:route") {
                    line(out, x0, y, x1, y, 6.5f * s, kBlack, 0.55f);
                    line(out, x0, y, x1, y, 4 * s, kWay, 1.0f);
                } else if (row.icon == "mark:jump") {
                    line(out, x0, y, x0 + 9 * s, y, 3 * s, kJump, 1.0f);
                    line(out, x0 + 15 * s, y, x1, y, 3 * s, kJump, 1.0f);
                } else if (row.icon == "mark:players") {
                    panel::roundRect(out, panel::Box{x0, y - 8.5f * s, 24 * s, 17 * s}, 8.5f * s, kPill, 0.94f);
                    uiicons::draw(out, uiicons::Icon::Npc, x0 + 6.5f * s, y - 5.5f * s, 11 * s, kWhite, 1.0f, 2.4f);
                } else if (row.icon == "mark:here") {
                    panel::circle(out, x0 + 12 * s, y, 5 * s + 1.5f, kBlack, 0.85f);
                    panel::circle(out, x0 + 12 * s, y, 5 * s, kWhite, 1.0f);
                }
                continue;
            }
            case refpages::Style::Button:
                if (row.gold)
                    panel::roundRect(out, b, 9 * s, kAccent, on ? 0.18f : 0.10f, kAccent2, 0.85f, 1.5f * s);
                else
                    panel::roundRect(out, b, 9 * s, kWhite, on ? 0.12f : kCellA);
                continue;
            case refpages::Style::Grid:  // 3.0.11-beta.32: the cells' tints; the columns' names on none
                if (!row.gold)
                    for (size_t k = 0; k < row.tags.size(); ++k) {
                        uint32_t ground = 0, words = 0;
                        float a = 0;
                        gridTint(std::atoi(row.tags[k].c_str()), ground, a, words);
                        panel::roundRect(out, gridCell(b, k, row.tags.size(), s), 4 * s, ground, a);
                    }
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

Rect worldArea(const Layout& L, const std::vector<Placed>& rows, int scroll, int row, const WorldLook& w) {
    const worldmap::Tile* t = w.hover.empty() || !w.sheet ? nullptr : worldmap::tileOf(*w.sheet, w.hover);
    if (!t || row < 0 || size_t(row) >= rows.size() || !shown(L, rows[size_t(row)], scroll)) return Rect{};
    const Placed& r = rows[size_t(row)];
    const panel::Box b{float(r.x), float(L.bodyY + r.y - scroll), float(r.w), float(r.h)};
    const panel::Box tb = tileBox(*t, worldView(w, r), float(int(b.x)), float(int(b.y)));
    const float out = 2.5f * L.s / 2 + 2;  // the widest frame drawn on a map (yours), its smooth edge
    float x0 = tb.x - out, y0 = tb.y - out, x1 = tb.x + tb.w + out, y1 = tb.y + tb.h + out;
    for (const OverItem& o : overItems(L, w, b))  // 3.0.11-beta.29: its card
        if (o.kind == Over::Tip) {
            x0 = std::min(x0, o.box.x - 2);
            y0 = std::min(y0, o.box.y - 2);
            x1 = std::max(x1, o.box.x + o.box.w + 2);
            y1 = std::max(y1, o.box.y + o.box.h + 2);
        }
    x0 = std::max(x0, b.x);  // the map's own row
    y0 = std::max(y0, b.y);
    x1 = std::min(x1, b.x + b.w);
    y1 = std::min(y1, b.y + b.h);
    return x1 > x0 && y1 > y0 ? pixelsOf(L, panel::Box{x0, y0, x1 - x0, y1 - y0}) : Rect{};
}

Rect rowArea(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll, int i) {
    if (i < 0 || size_t(i) >= rows.size() || !shown(L, rows[size_t(i)], scroll)) return Rect{};
    const Placed& r = rows[size_t(i)];
    const float s = L.s;
    const refpages::Row* row = size_t(i) < p.rows.size() ? &p.rows[size_t(i)] : nullptr;
    const refpages::Style st = row ? row->style : refpages::Style::Row;
    // their own: a tile, a chip, a frame's heading (its line); 3.0.11-beta.26 a row inside a frame: its band
    if (st == refpages::Style::Tile || st == refpages::Style::Chip || st == refpages::Style::Result ||
        st == refpages::Style::More || st == refpages::Style::Tab || st == refpages::Style::Tool ||
        st == refpages::Style::Step || st == refpages::Style::Button ||
        (row && framed(*row)))  // 3.0.11-beta.28: a result's band its own box; 3.0.11-beta.29 the world map's
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
    if (part == Part::Home && c.home.w > 0) out.push_back(pixelsOf(L, c.home));  // 3.0.11-beta.30
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
            case refpages::Style::Tab: {  // 3.0.11-beta.29: the world map's bar
                const bool chip = row.link.go == refpages::Go::None;
                const float tx = x + (chip ? 27 * s : 38 * s);
                out.push_back(Words{panel::Box{tx, y, x + w - tx - 8 * s, h}, row.left,
                                    chip ? kLiveDot : row.gold ? kAccent2 : kText, 13.5f * s, true, Align::Left});
                break;
            }
            case refpages::Style::Tool:
                break;
            case refpages::Style::Caption:
                out.push_back(Words{panel::Box{x, y, w, h}, row.left, kText, 16 * s, true, Align::Left});
                break;
            case refpages::Style::Warn: {
                Words warn{panel::Box{x + 34 * s, y + 11 * s, w - 46 * s, h - 22 * s}, row.left, kWarnWords, 13 * s,
                           false, Align::Left};
                warn.wrap = true;
                out.push_back(warn);
                break;
            }
            case refpages::Style::Step:
                out.push_back(Words{panel::Box{x + 3 * s, y, 22 * s, h}, row.right, row.tone > 0 ? kDark : kText,
                                    12 * s, true, Align::Center});
                out.push_back(Words{panel::Box{x + 36 * s, y + 4 * s, w - 40 * s, 19 * s}, row.left, kText, 14 * s,
                                    true, Align::Left});
                out.push_back(Words{panel::Box{x + 36 * s, y + 23 * s, w - 40 * s, 16 * s}, row.sub, kMuted,
                                    12.5f * s, false, Align::Left});
                break;
            case refpages::Style::Legend:
                out.push_back(Words{panel::Box{x + 36 * s, y, w - 36 * s, h}, row.left, kMuted, 12.5f * s, false,
                                    Align::Left});
                break;
            case refpages::Style::Button:
                out.push_back(Words{panel::Box{x, y, w, h}, row.left, row.gold ? kAccent2 : kText, 14 * s, true,
                                    Align::Center});
                break;
            case refpages::Style::Grid:  // 3.0.11-beta.32: its name, its cells (the columns' names cut to theirs)
                out.push_back(Words{panel::Box{x, y, 100 * s, h}, row.left, kText, 13.5f * s, false, Align::Left});
                for (size_t k = 0; k < row.tags.size(); ++k) {
                    const panel::Box cell = gridCell(panel::Box{x, y, w, h}, k, row.tags.size(), s);
                    uint32_t ground = 0, colour = kMuted;
                    float a = 0;
                    if (!row.gold) gridTint(std::atoi(row.tags[k].c_str()), ground, a, colour);
                    const std::string text =
                        row.gold ? gridHead(L, row.tags[k], 12.5f * s, cell.w - 4 * s) : row.tags[k];
                    out.push_back(Words{cell, text, row.gold ? kMuted : colour, 12.5f * s, row.gold, Align::Center});
                }
                break;
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
            case refpages::Style::World:  // 3.0.11-beta.29: the words over the map (overItems)
                worldWords(out, L, look.world, panel::Box{x, y, w, h});
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
