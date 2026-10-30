#include "core/item_window.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::itemwin {
namespace {

// The concept's sizes at scale 1 (v17_windows): 500 wide; the title 46, the tabs 32, the search 34, cells 48 eight a
// row 8 apart, five rows; the footer 40; the glass 90 % (chat and windows not under 80 %)
constexpr float kW = 500, kTitleH = 46, kTabsH = 32, kSearchH = 34, kCell = 48, kGap = 8, kFooterH = 44, kSide = 18;
constexpr int kColumns = 8, kRows = 5;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kGold = 0xF3C653, kGold2 = 0xC9A55B;
constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kFaint = 0x7E889A, kGreen = 0x5FD08A, kRed = 0xFF7D6E;

size_t utf8Next(const std::string& s, size_t i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    const size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    return std::min(s.size(), i + n);
}

// A premultiplied picture centred in a box, its alpha times `k`
void put(panel::Picture& out, const panel::Picture& pic, const panel::Box& box, float k) {
    const int ox = int(std::lround(box.x + (box.w - float(pic.width)) / 2));
    const int oy = int(std::lround(box.y + (box.h - float(pic.height)) / 2));
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (k < 1) {
                const auto ch = [&](int sh) { return uint32_t(float((c >> sh) & 0xFF) * k + 0.5f); };
                c = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
            }
            if (!(c >> 24)) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            const auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
}

std::string tipFirst(const Item& it) {
    std::string s = nameOf(it);
    if (it.count > 1) s += " \xC3\x97" + grouped(it.count);
    return s;
}

std::string tipSecond(const Item& it, const Texts& t) {
    const Use u = useOf(it);
    std::string s = u == Use::Use || u == Use::Confirm ? t.use : u == Use::Wear || u == Use::Ammo ? t.wear
                    : u == Use::Card                    ? t.card
                                                        : "";
    if (!s.empty()) s += " \xC2\xB7 ";
    return s + t.info;
}

// The pointed cell's tip: over the cell (under it on the first rows), inside the glass
panel::Box tipBox(const Layout& L, const Item& it, int cell, const Texts& t, const Measure& m) {
    if (cell < 0 || cell >= int(L.cells.size())) return panel::Box{};
    const float s = L.s;
    const float w = std::max(m(tipFirst(it), 13 * s, true), m(tipSecond(it, t), 11.5f * s, false)) + 22 * s;
    const float h = 44 * s;
    const panel::Box& c = L.cells[size_t(cell)];
    const float x = std::clamp(c.x + c.w / 2 - w / 2, L.glass.x + 6 * s, L.glass.x + L.glass.w - w - 6 * s);
    const bool under = c.y - h - 6 * s < L.grid.y;
    const float y = under ? c.y + c.h + 6 * s : c.y - h - 6 * s;
    return panel::Box{x, y, w, h};
}

}  // namespace

Tab tabOf(int type) {
    if (type == 0 || type == 1 || type == 2 || type == 0x12) return Tab::Items;
    if (type == 4 || type == 5 || type == 8 || type == 9 || (type >= 0xB && type <= 0xF)) return Tab::Gear;
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
        case 0xF: return it.location && it.identified ? Use::Wear : Use::None;
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
        if (tabOf(items[i].type) != tab) continue;
        if (!want.empty() && lowered(items[i].name).find(want) == std::string::npos) continue;
        out.push_back(int(i));
    }
    return out;
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
    for (size_t i = 0; i < L.cells.size(); ++i)
        if (L.cells[i].contains(x, y)) {
            const int at = int(i) + scroll * L.columns;
            return at < int(shownCount) ? Hit{Part::Cell, at} : Hit{Part::Grid, -1};
        }
    if (L.grid.contains(x, y)) return Hit{Part::Grid, -1};
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Item>& items, const std::vector<int>& shown,
           const Show& sh, const Texts& t, const Measure& m) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.09f, 1.0f * s);
    if (sh.hoverClose) panel::roundRect(out, L.close, 7 * s, kWhite, 0.12f);
    panel::roundRect(out, panel::Box{L.tabsRow.x, L.tabsRow.y + L.tabsRow.h - 1 * s, L.tabsRow.w, 1 * s}, 0, kWhite,
                     0.08f);
    for (size_t i = 0; i < L.tabs.size(); ++i)
        if (int(i) == int(sh.tab)) {
            const panel::Box& b = L.tabs[i];
            panel::roundRect(out, panel::Box{b.x, b.y + b.h - 2.5f * s, b.w, 2 * s}, 1 * s, kGold, 0.95f);
        }
    panel::roundRect(out, L.search, 8 * s, kBlack, 0.35f, sh.typing ? kGold2 : kWhite, sh.typing ? 0.6f : 0.10f,
                     1.0f * s);
    const float icon = 15 * s;
    uiicons::draw(out, uiicons::Icon::Search, L.search.x + 10 * s, L.search.y + (L.search.h - icon) / 2, icon, kMuted,
                  0.9f, 2.2f);
    if (sh.typing && sh.caretOn) {
        const float x = L.search.x + 32 * s + m(sh.search, 12.5f * s, false) + 1 * s;
        panel::roundRect(out, panel::Box{x, L.search.y + 9 * s, 1.5f * s, L.search.h - 18 * s}, 0, kText, 0.95f);
    }
    for (size_t i = 0; i < L.cells.size(); ++i) {
        const panel::Box& b = L.cells[i];
        const int at = int(i) + sh.scroll * L.columns;
        const bool hot = at == sh.hover;
        panel::roundRect(out, b, 8 * s, kBlack, 0.35f, hot ? kGold2 : kWhite, hot ? 0.95f : 0.08f,
                         (hot ? 1.5f : 1) * s);
        if (at >= int(shown.size())) continue;
        const Item& it = items[size_t(shown[size_t(at)])];
        if (it.icon && it.icon->width > 0) put(out, *it.icon, b, it.identified ? 1.0f : 0.45f);
        if (it.count > 1) {
            const float w = m(grouped(it.count), 10.5f * s, true) + 6 * s;
            panel::roundRect(out, panel::Box{b.x + b.w - w - 2 * s, b.y + b.h - 14 * s, w, 12 * s}, 3 * s, kBlack,
                             0.7f);
        }
        if (it.refine > 0) {
            const float w = m("+" + std::to_string(it.refine), 10.5f * s, true) + 6 * s;
            panel::roundRect(out, panel::Box{b.x + 2 * s, b.y + 2 * s, w, 12 * s}, 3 * s, kBlack, 0.7f);
        }
    }
    const size_t total = (shown.size() + size_t(L.columns) - 1) / size_t(L.columns);
    if (total > size_t(L.rows)) {  // the scroll's place beside the grid
        const float h = L.grid.h * float(L.rows) / float(total);
        const float y = L.grid.y + (L.grid.h - h) * float(sh.scroll) / float(total - size_t(L.rows));
        panel::roundRect(out, panel::Box{L.grid.x + L.grid.w + 4 * s, y, 3 * s, h}, 1.5f * s, kWhite, 0.25f);
    }
    panel::roundRect(out, panel::Box{0, L.footer.y, L.glass.w, 1 * s}, 0, kWhite, 0.08f);
    const float share = sh.weightMost > 0 ? std::clamp(float(sh.weight) / float(sh.weightMost), 0.0f, 1.0f) : 0.0f;
    panel::roundRect(out, L.bar, 3 * s, kWhite, 0.10f);
    if (share > 0)
        panel::roundRect(out, panel::Box{L.bar.x, L.bar.y, L.bar.w * share, L.bar.h}, 3 * s,
                         share >= 0.9f ? kRed : share >= 0.5f ? kGold : kGreen, 0.95f);
    for (const Flash& f : sh.flashes) {  // 3.0.11-beta.44: a use answered in its cell
        const float t = std::clamp(f.t, 0.0f, 1.0f), fade = 1 - t;
        bool empty = true;  // its cell now: nothing shown there (the last one gone)
        for (size_t i = 0; i < L.cells.size(); ++i) {
            const panel::Box& b = L.cells[i];
            if (std::fabs(b.x - f.cell.x) < 1 && std::fabs(b.y - f.cell.y) < 1)
                empty = int(i) + sh.scroll * L.columns >= int(shown.size());
        }
        if (empty && f.icon && f.icon->width > 0) put(out, *f.icon, f.cell, fade * fade);
        panel::roundRect(out, f.cell, 8 * s, kGold, 0.30f * fade);
        const float grow = 9 * s * t;
        const panel::Box ring{f.cell.x - grow, f.cell.y - grow, f.cell.w + 2 * grow, f.cell.h + 2 * grow};
        panel::roundRect(out, ring, 8 * s + grow, kGold, 0.0f, kGold, 0.95f * fade, 2.0f * s);
    }
    if (sh.tip && sh.hover >= 0 && sh.hover < int(shown.size())) {
        const Item& it = items[size_t(shown[size_t(sh.hover)])];
        const panel::Box tip = tipBox(L, it, sh.hover - sh.scroll * L.columns, t, m);
        if (tip.w > 0) panel::roundRect(out, tip, 8 * s, kCard, 0.96f, kWhite, 0.14f, 1.0f * s);
    }
}

std::vector<Words> words(const Layout& L, const std::vector<Item>& items, const std::vector<int>& shown,
                         const Show& sh, const Texts& t, const Measure& m) {
    std::vector<Words> out;
    const float s = L.s;
    const float titleW = m(t.title, 15 * s, true) + 2;
    out.push_back(Words{panel::Box{kSide * s, 0, titleW, L.title.h}, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{panel::Box{kSide * s + titleW + 8 * s, 0, m(t.hotkey, 11 * s, false) + 2, L.title.h}, t.hotkey,
                        kFaint, 11 * s, false, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", sh.hoverClose ? kText : kMuted, 16 * s, false, Align::Center});  // ×
    for (size_t i = 0; i < L.tabs.size(); ++i) {
        const bool on = int(i) == int(sh.tab);
        out.push_back(Words{L.tabs[i], t.tabs[i], on ? kText : int(i) == sh.hoverTab ? 0xC9D1DE : kFaint, 13 * s, true,
                            Align::Left});
    }
    const panel::Box field{L.search.x + 32 * s, L.search.y, L.search.w - 40 * s, L.search.h};
    if (sh.search.empty() && !sh.typing)
        out.push_back(Words{field, t.search, kFaint, 12 * s, false, Align::Left});
    else if (!sh.search.empty())
        out.push_back(Words{field, sh.search, kText, 12.5f * s, false, Align::Left});
    panel::Box tip;  // the pointed cell's tip: the cells' words under it not drawn over it
    if (sh.tip && sh.hover >= 0 && sh.hover < int(shown.size()))
        tip = tipBox(L, items[size_t(shown[size_t(sh.hover)])], sh.hover - sh.scroll * L.columns, t, m);
    const auto under = [&](const panel::Box& b) {
        return tip.w > 0 && b.x < tip.x + tip.w && b.x + b.w > tip.x && b.y < tip.y + tip.h && b.y + b.h > tip.y;
    };
    for (size_t i = 0; i < L.cells.size(); ++i) {
        const panel::Box& b = L.cells[i];
        const int at = int(i) + sh.scroll * L.columns;
        if (at >= int(shown.size()) || under(b)) continue;
        const Item& it = items[size_t(shown[size_t(at)])];
        if (it.count > 1) {
            const std::string n = grouped(it.count);
            const float w = m(n, 10.5f * s, true) + 6 * s;
            out.push_back(Words{panel::Box{b.x + b.w - w - 2 * s, b.y + b.h - 14 * s, w, 12 * s}, n, kText, 10.5f * s,
                                true, Align::Center});
        }
        if (it.refine > 0) {
            const std::string n = "+" + std::to_string(it.refine);
            out.push_back(Words{panel::Box{b.x + 2 * s, b.y + 2 * s, m(n, 10.5f * s, true) + 6 * s, 12 * s}, n, kGold,
                                10.5f * s, true, Align::Center});
        }
        if (!it.identified)
            out.push_back(Words{panel::Box{b.x + 2 * s, b.y + 2 * s, 12 * s, 12 * s}, "?", kMuted, 11 * s, true,
                                Align::Center});
    }
    const float side = kSide * s;
    out.push_back(
        Words{panel::Box{side, L.footer.y, 60 * s, L.footer.h}, t.weight, kMuted, 12 * s, false, Align::Left});
    if (sh.weightMost > 0) {
        const int percent = int(int64_t(sh.weight) * 100 / sh.weightMost);
        const std::string w = grouped(sh.weight) + " / " + grouped(sh.weightMost) + " \xC2\xB7 " +
                              std::to_string(percent) + " %";
        out.push_back(Words{panel::Box{L.bar.x + L.bar.w + 12 * s, L.footer.y, m(w, 12 * s, false) + 4, L.footer.h}, w,
                            0xC9D1DE, 12 * s, false, Align::Left});
    }
    const std::string z = grouped(sh.zeny) + " z";
    const float zw = m(z, 12 * s, true) + 4;
    out.push_back(Words{panel::Box{L.glass.w - side - zw, L.footer.y, zw, L.footer.h}, z, kText, 12 * s, true,
                        Align::Right});
    for (const Flash& f : sh.flashes) {  // its word rising out of the cell, dimming
        if (f.label.empty()) continue;
        const float t = std::clamp(f.t, 0.0f, 1.0f);
        const auto toward = [&](int sh2) {
            const float a = float((f.rgb >> sh2) & 0xFF), b = float((kCard >> sh2) & 0xFF);
            return uint32_t(a + (b - a) * t * t) << sh2;
        };
        const float w = m(f.label, 12 * s, true) + 8 * s, h = 16 * s;
        out.push_back(Words{panel::Box{f.cell.x + (f.cell.w - w) / 2, f.cell.y - 4 * s - 22 * s * t, w, h}, f.label,
                            toward(16) | toward(8) | toward(0), 12 * s, true, Align::Center});
    }
    if (sh.hover >= 0 && sh.hover < int(shown.size())) {
        const Item& it = items[size_t(shown[size_t(sh.hover)])];
        if (tip.w > 0) {
            out.push_back(Words{panel::Box{tip.x + 11 * s, tip.y + 4 * s, tip.w - 22 * s, 18 * s}, tipFirst(it), kText,
                                13 * s, true, Align::Left});
            out.push_back(Words{panel::Box{tip.x + 11 * s, tip.y + 23 * s, tip.w - 22 * s, 16 * s},
                                tipSecond(it, t), kMuted, 11.5f * s, false, Align::Left});
        }
    }
    return out;
}

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
