#include "core/item_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::itemwin {
namespace {

// The concept's sizes at scale 1 (v17_windows): 500 wide; the title 46, the tabs 32, the search 34, cells 48 eight a
// row 8 apart, five rows; the footer 40; the glass 90 % (chat and windows not under 80 %)
constexpr float kW = 500, kTitleH = 46, kTabsH = 32, kSearchH = 34, kCell = 48, kGap = 8, kFooterH = 44, kSide = 18;
constexpr int kColumns = 8, kRows = 5;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kGold = 0xF3C653, kGold2 = 0xC9A55B;
constexpr uint32_t kCompare = 0xF2CF7A;  // 3.0.11-beta.285: the gear compared, gold
constexpr uint32_t kMarket = 0x9FE07C;   // 3.0.11-beta.287: the site's market, green
constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kFaint = 0x7E889A, kGreen = 0x5FD08A, kRed = 0xFF7D6E;

size_t utf8Next(const std::string& s, size_t i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    const size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    return std::min(s.size(), i + n);
}

std::string tipFirstOf(const Item& it) {
    std::string s = nameOf(it);
    if (it.count > 1) s += " \xC3\x97" + grouped(it.count);
    return s;
}

std::string tipSecondOf(const Item& it, const Texts& t) {
    const Use u = useOf(it);
    std::string s = !t.all.empty()                      ? t.all  // 3.0.11-beta.115: «Склад»'s
                    : u == Use::Use || u == Use::Confirm ? t.use
                    : u == Use::Wear || u == Use::Ammo   ? t.wear
                    : u == Use::Card                     ? t.card
                                                         : "";
    if (!s.empty()) s += " \xC2\xB7 ";
    if (it.worn && !t.worn.empty()) s = t.worn + " \xC2\xB7 " + s;  // 3.0.11-beta.118: the arrows worn
    return s + t.info;
}

// 3.0.11-beta.113: a cell's star - its lower left corner
panel::Box starBox(const panel::Box& cell, float s) {
    return panel::Box{cell.x + 2 * s, cell.y + cell.h - 15 * s, 13 * s, 13 * s};
}

}  // namespace

Tab tabOf(int type) {
    if (type == 0 || type == 1 || type == 2 || type == 0x12) return Tab::Use;
    if (type == 4 || type == 5 || type == 8 || type == 9 || (type >= 0xB && type <= 0xF) || type == 10 || type == 0x11)
        return Tab::Gear;
    if (type == 6) return Tab::Cards;
    return Tab::Etc;
}

Use useOf(const Item& it) {
    if (!it.index) return Use::None;
    switch (it.type) {
        case 0:
        case 1:
        case 2: return Use::Use;
        case 0x12: return Use::Confirm;  // the game asks first
        case 4:
        case 5:
        case 8:
        case 9:
        case 0xB:
        case 0xC:
        case 0xD:
        case 0xE:
        case 0xF:
            if (it.location && it.identified) return Use::Wear;
            // 3.0.16-beta.17 (R-PTDFCY: a pet's accessory did nothing): a gear with no place on the body, numbered
            // 10001-10020 - the pets' accessories - worn as the game's own window does (uaRO.exe FUN_004b9a10: wear,
            // its place 0)
            return it.id > 10000 && it.id < 0x2725 && it.identified ? Use::Wear : Use::None;
        case 6: return Use::Card;
        case 10:
        case 0x10:
        case 0x11: return Use::Ammo;
        default: return Use::None;
    }
}

std::string lowered(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        const size_t n = utf8Next(s, i) - i;
        if (n == 1) {
            out += char(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        } else if (n == 2 && c == 0xD0) {
            const unsigned char d = static_cast<unsigned char>(s[i + 1]);
            if (d >= 0x90 && d <= 0x9F) {
                out += char(0xD0);
                out += char(d + 0x20);
            } else if (d >= 0xA0 && d <= 0xAF) {
                out += char(0xD1);
                out += char(d - 0x20);
            } else if (d >= 0x80 && d <= 0x8F) {
                out += char(0xD1);
                out += char(d + 0x10);
            } else {
                out.append(s, i, n);
            }
        } else {
            out.append(s, i, n);
        }
        i += n;
    }
    return out;
}

std::vector<int> shownOf(const std::vector<Item>& items, Tab tab, const std::string& search) {
    std::vector<int> out;
    const std::string want = lowered(search);
    for (size_t i = 0; i < items.size(); ++i) {
        if (tab == Tab::Starred ? !items[i].starred : tab != Tab::All && tabOf(items[i].type) != tab) continue;
        if (!want.empty() && lowered(items[i].name).find(want) == std::string::npos) continue;
        out.push_back(int(i));
    }
    return out;
}

std::set<int> starredOf(const std::string& text, const std::string& who) {
    std::set<int> out;
    size_t at = 0;
    while (at < text.size()) {
        const size_t end = std::min(text.find('\n', at), text.size());
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        const size_t tab = line.find('\t');
        if (tab == std::string::npos || line.substr(0, tab) != who) continue;
        for (size_t p = tab + 1; p < line.size();) {
            const size_t comma = std::min(line.find(',', p), line.size());
            const int id = std::atoi(line.substr(p, comma - p).c_str());
            if (id > 0) out.insert(id);
            p = comma + 1;
        }
    }
    return out;
}

std::string withStarred(const std::string& text, const std::string& who, const std::set<int>& ids) {
    std::string out;
    size_t at = 0;
    while (at < text.size()) {  // the other characters' lines as they were
        const size_t end = std::min(text.find('\n', at), text.size());
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.substr(0, line.find('\t')) != who) out += line + '\n';
    }
    if (ids.empty() || who.empty()) return out;
    out += who + '\t';
    bool first = true;
    for (const int id : ids) {
        out += (first ? "" : ",") + std::to_string(id);
        first = false;
    }
    return out + '\n';
}

int iconSide(float scale) { return int(std::lround(32 * scale)); }

Layout layout(int x, int y, float scale, const Texts& t, const Measure& m) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.columns = kColumns;
    L.rows = kRows;
    const float w = kW * s, side = kSide * s;
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.close = panel::Box{w - side - 26 * s, (kTitleH * s - 26 * s) / 2, 26 * s, 26 * s};
    L.tabsRow = panel::Box{side, kTitleH * s, w - 2 * side, kTabsH * s};
    float tx = side;
    for (int i = 0; i < kTabs; ++i) {
        const float tw = m(t.tabs[i], 13 * s, true) + 4 * s;
        L.tabs.push_back(panel::Box{tx, L.tabsRow.y, tw, L.tabsRow.h});
        tx += tw + 20 * s;
    }
    L.search = panel::Box{side, L.tabsRow.y + L.tabsRow.h + 12 * s, w - 2 * side, kSearchH * s};
    const float cell = kCell * s, gap = kGap * s;
    const float gridW = float(kColumns) * cell + float(kColumns - 1) * gap;
    const float gridH = float(kRows) * cell + float(kRows - 1) * gap;
    L.grid = panel::Box{(w - gridW) / 2, L.search.y + L.search.h + 14 * s, gridW, gridH};
    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < kColumns; ++c)
            L.cells.push_back(panel::Box{L.grid.x + float(c) * (cell + gap), L.grid.y + float(r) * (cell + gap), cell,
                                         cell});
    L.footer = panel::Box{0, L.grid.y + L.grid.h + 14 * s, w, kFooterH * s};
    L.bar = panel::Box{side + m(t.weight, 12 * s, false) + 12 * s, L.footer.y + L.footer.h / 2 - 3 * s, 130 * s,
                       6 * s};
    L.glass = panel::Box{0, 0, w, L.footer.y + L.footer.h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(L.glass.h));
    L.x = x;
    L.y = y;
    return L;
}

int mostScroll(const Layout& L, size_t count) {
    const int rows = int((count + size_t(L.columns) - 1) / size_t(L.columns));
    return std::max(0, rows - L.rows);
}

Hit hitTest(const Layout& L, size_t shownCount, int scroll, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (size_t i = 0; i < L.tabs.size(); ++i)
        if (L.tabs[i].contains(x, y)) return Hit{Part::Tab, int(i)};
    if (L.search.contains(x, y)) return Hit{Part::Search, -1};
    if (L.picked.w > 0 && L.picked.contains(x, y)) return Hit{Part::Picked, -1};  // 3.0.18-beta.155
    for (size_t i = 0; i < L.cells.size(); ++i)
        if (L.cells[i].contains(x, y)) {
            const int at = int(i) + scroll * L.columns;
            if (at >= int(shownCount)) return Hit{Part::Grid, -1};
            return Hit{starBox(L.cells[i], L.s).contains(x, y) ? Part::Star : Part::Cell, at};  // 3.0.11-beta.113
        }
    if (L.grid.contains(x, y)) return Hit{Part::Grid, -1};
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

std::string tipFirst(const Item& it) { return tipFirstOf(it); }
std::string tipSecond(const Item& it, const Texts& t) { return tipSecondOf(it, t); }

std::string nameOf(const Item& it) {
    return it.refine > 0 ? "+" + std::to_string(it.refine) + " " + it.name : it.name;
}

std::string grouped(long long n) {
    std::string digits = std::to_string(n < 0 ? -n : n), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0) out += ' ';
        out += digits[i];
    }
    return n < 0 ? "-" + out : out;
}

}  // namespace uagfx::itemwin
