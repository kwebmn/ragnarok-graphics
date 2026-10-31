#include "core/skill_window.hpp"

#include <algorithm>
#include <cmath>

#include "core/pad_glyphs.hpp"
#include "core/ui_style.hpp"

namespace uagfx::skillwin {
namespace {

// The concept's sizes at scale 1: 700 wide, the title 46, the tabs 32, a row 46, the footer 40
constexpr float kW = 700, kTitleH = 46, kTabsH = 32, kRowH = 46, kFooterH = 40, kSide = 16;
constexpr float kDot = 7, kDotStep = 10, kDotsMost = 10 * kDotStep - (kDotStep - kDot), kLevelW = 46, kPlus = 24;
constexpr float kArrowW = 18, kArrowH = 22, kUseNumW = 26, kGlyph = 17;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xF3C653, kGold2 = 0xC9A55B;
constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kFaint = 0x7E889A;

std::string fill(std::string form, const std::string& v) {
    const std::string key = "{1}";
    for (size_t at = form.find(key); at != std::string::npos; at = form.find(key, at + v.size()))
        form.replace(at, key.size(), v);
    return form;
}

const std::string kDotSep = " \xC2\xB7 ";  // « · »

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

const Tab* tabOf(const Data& d, int tab) {
    if (d.tabs.empty()) return nullptr;
    return &d.tabs[size_t(std::clamp(tab, 0, int(d.tabs.size()) - 1))];
}

int rowsShown(const Data& d) {  // the same height on every tab
    size_t most = 1;
    for (const Tab& t : d.tabs) most = std::max(most, t.skills.size());
    return int(std::min<size_t>(size_t(kRows), most));
}

struct PadItem {
    padglyph::Button button;
    const std::string* words;
};

std::vector<PadItem> padItems(const Texts& t) {
    return {{padglyph::Button::A, &t.use}, {padglyph::Button::Y, &t.raise}, {padglyph::Button::X, &t.assign},
            {padglyph::Button::B, &t.close}};
}

// The controller's buttons and words in the footer, from its right
void padBoxes(Layout& L, const Texts& t, const Measure& m) {
    const float s = L.s;
    float right = L.footer.x + L.footer.w - kSide * s;
    const std::vector<PadItem> items = padItems(t);
    L.padGlyphs.resize(items.size());
    L.padWords.resize(items.size());
    for (size_t i = items.size(); i-- > 0;) {
        const float ww = m(*items[i].words, 12 * s, false) + 2;
        const float gw = padglyph::widthOf(items[i].button, kGlyph * s);
        L.padWords[i] = panel::Box{right - ww, L.footer.y, ww, L.footer.h};
        L.padGlyphs[i] = panel::Box{L.padWords[i].x - 6 * s - gw, L.footer.y + (L.footer.h - kGlyph * s) / 2, gw,
                                    kGlyph * s};
        right = L.padGlyphs[i].x - 16 * s;
    }
}

}  // namespace

bool canRaise(const Skill& k, int points) { return points > 0 && k.up && (k.most <= 0 || k.level < k.most); }

bool usable(const Skill& k) { return k.level > 0 && k.type != 0; }

std::string info(const Skill& k, const Texts& t) {
    if (k.level <= 0) return k.needs.empty() ? t.unlearned : fill(t.needs, k.needs);
    if (k.type == 0) return t.passive;
    const std::string& kind = (k.type & 32) ? t.trap
                              : (k.type & 16) ? t.ally
                              : (k.type & 2)  ? t.ground
                              : (k.type & 4)  ? t.self
                                              : t.enemy;
    std::string line = kind;
    if (k.sp > 0) line += kDotSep + fill(t.sp, std::to_string(k.sp));
    if (k.range > 1) line += kDotSep + fill(t.range, std::to_string(k.range));
    return line;
}

int keepInSight(int focus, int scroll, int count) {
    const int most = std::max(0, count - kRows);
    scroll = std::clamp(scroll, 0, most);
    if (focus < 0) return scroll;
    if (focus < scroll) return focus;
    if (focus >= scroll + kRows) return std::min(most, focus - kRows + 1);
    return scroll;
}

int iconSide(float scale) { return int(std::lround(32 * scale)); }

Layout layout(int x, int y, float scale, const Data& d, const Show& sh, const Texts& t, const Measure& m) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, side = kSide * s;
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.close = panel::Box{w - side - 26 * s, (kTitleH * s - 26 * s) / 2, 26 * s, 26 * s};
    L.tabsRow = panel::Box{side, kTitleH * s, w - 2 * side, kTabsH * s};
    float tx = side;
    for (const Tab& tab : d.tabs) {
        const float tw = m(tab.name, 12.5f * s, true) + 20 * s;
        L.tabs.push_back(panel::Box{tx, L.tabsRow.y, tw, L.tabsRow.h});
        tx += tw + 4 * s;
    }
    const Tab* tab = tabOf(d, sh.tab);
    L.tab = tab ? int(tab - d.tabs.data()) : 0;
    const int count = tab ? int(tab->skills.size()) : 0;
    const int shown = rowsShown(d);
    L.first = std::clamp(sh.scroll, 0, std::max(0, count - kRows));
    const bool scrolls = count > kRows;
    const float listY = L.tabsRow.y + L.tabsRow.h + 6 * s;
    const float rowW = w - 2 * side - (scrolls ? 12 * s : 0);
    L.list = panel::Box{side, listY, w - 2 * side, float(shown) * kRowH * s};
    if (scrolls) L.scroll = panel::Box{w - side - 5 * s, listY + 4 * s, 5 * s, L.list.h - 8 * s};
    for (int i = L.first; i < count && i < L.first + kRows; ++i) {
        const Skill& k = tab->skills[size_t(i)];
        const panel::Box r{side, listY + float(i - L.first) * kRowH * s, rowW, kRowH * s};
        L.rows.push_back(r);
        L.icons.push_back(panel::Box{r.x + 6 * s, r.y + (r.h - 32 * s) / 2, 32 * s, 32 * s});
        const float plusX = r.x + r.w - 8 * s - kPlus * s;
        L.pluses.push_back(canRaise(k, d.points)
                               ? panel::Box{plusX, r.y + (r.h - kPlus * s) / 2, kPlus * s, kPlus * s}
                               : panel::Box{});
        const float levelX = plusX - 10 * s - kLevelW * s;
        const float dotsRight = levelX - 10 * s;
        if (k.most > 0 && k.most <= 10) {
            const float dw = float(k.most) * kDotStep * s - (kDotStep - kDot) * s;
            L.dots.push_back(panel::Box{dotsRight - dw, r.y + (r.h - kDot * s) / 2, dw, kDot * s});
        } else {
            L.dots.push_back(panel::Box{});
        }
        if (usable(k) && k.level > 1) {
            const float cx = dotsRight - kDotsMost * s - 14 * s - (2 * kArrowW + kUseNumW) * s;
            const float cy = r.y + (r.h - kArrowH * s) / 2;
            L.less.push_back(panel::Box{cx, cy, kArrowW * s, kArrowH * s});
            L.more.push_back(panel::Box{cx + (kArrowW + kUseNumW) * s, cy, kArrowW * s, kArrowH * s});
        } else {
            L.less.push_back(panel::Box{});
            L.more.push_back(panel::Box{});
        }
    }
    L.footer = panel::Box{0, L.list.y + L.list.h + 6 * s, w, kFooterH * s};
    if (sh.pad) padBoxes(L, t, m);
    L.glass = panel::Box{0, 0, w, L.footer.y + L.footer.h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(L.glass.h));
    L.x = x;
    L.y = y;
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (size_t i = 0; i < L.tabs.size(); ++i)
        if (L.tabs[i].contains(x, y)) return Hit{Part::Tab, int(i)};
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const int index = L.first + int(i);
        if (L.pluses[i].w > 0 && L.pluses[i].contains(x, y)) return Hit{Part::Plus, index};
        if (L.less[i].w > 0 && L.less[i].contains(x, y)) return Hit{Part::Less, index};
        if (L.more[i].w > 0 && L.more[i].contains(x, y)) return Hit{Part::More, index};
        if (L.rows[i].contains(x, y)) return Hit{Part::Row, index};
    }
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

void paint(panel::Picture& out, const Layout& L, const Data& d, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.09f, 1.0f * s);
    if (sh.hoverClose) panel::roundRect(out, L.close, 7 * s, kWhite, 0.12f);
    for (size_t i = 0; i < L.tabs.size(); ++i) {  // the tab shown underlined in gold
        const panel::Box& b = L.tabs[i];
        if (int(i) == sh.hoverTab && int(i) != L.tab) panel::roundRect(out, b, 6 * s, kWhite, 0.05f);
        if (int(i) == L.tab)
            panel::roundRect(out, panel::Box{b.x + 6 * s, b.y + b.h - 3 * s, b.w - 12 * s, 2 * s}, 1 * s, kGold, 1.0f);
    }
    panel::roundRect(out, panel::Box{L.tabsRow.x, L.tabsRow.y + L.tabsRow.h, L.tabsRow.w, 1 * s}, 0, kWhite, 0.08f);
    const Tab* tab = tabOf(d, L.tab);
    for (size_t i = 0; i < L.rows.size() && tab; ++i) {
        const int index = L.first + int(i);
        const Skill& k = tab->skills[size_t(index)];
        const panel::Box& r = L.rows[i];
        if (index == sh.hoverRow) panel::roundRect(out, r, 6 * s, kWhite, 0.06f);
        if (sh.pad && index == sh.focus)  // the controller's frame
            panel::roundRect(out, r, 7 * s, kGold2, 0.08f, kGold, 0.9f, 1.5f * s);
        if (i + 1 < L.rows.size())
            panel::roundRect(out, panel::Box{r.x + 8 * s, r.y + r.h - 1 * s, r.w - 16 * s, 1 * s}, 0, kWhite, 0.05f);
        if (k.icon && k.icon->width > 0) put(out, *k.icon, L.icons[i], k.level > 0 ? 1.0f : 0.45f);
        const panel::Box& dots = L.dots[i];
        if (dots.w > 0)
            for (int n = 0; n < k.most; ++n) {
                const float cx = dots.x + (float(n) * kDotStep + kDot / 2) * s, cy = dots.y + dots.h / 2;
                if (n < k.level)
                    panel::circle(out, cx, cy, kDot / 2 * s, kGold, 1.0f);
                else
                    panel::circle(out, cx, cy, kDot / 2 * s, kWhite, 0.14f);
            }
        if (L.pluses[i].w > 0)
            panel::roundRect(out, L.pluses[i], 6 * s, kWhite, index == sh.hoverPlus ? 0.22f : 0.12f, kGold2, 0.7f,
                             1.0f * s);
        if (L.less[i].w > 0) {
            panel::roundRect(out, L.less[i], 5 * s, kWhite, index == sh.hoverLess ? 0.16f : 0.06f);
            panel::roundRect(out, L.more[i], 5 * s, kWhite, index == sh.hoverMore ? 0.16f : 0.06f);
        }
    }
    if (L.scroll.w > 0 && tab) {
        panel::roundRect(out, L.scroll, L.scroll.w / 2, kWhite, 0.05f);
        const float count = float(tab->skills.size());
        const float h = std::max(L.scroll.h * float(kRows) / count, 16 * s);
        const float y = L.scroll.y + (L.scroll.h - h) * float(L.first) / std::max(1.0f, count - float(kRows));
        panel::roundRect(out, panel::Box{L.scroll.x, y, L.scroll.w, h}, L.scroll.w / 2, kWhite, 0.28f);
    }
    panel::roundRect(out, panel::Box{0, L.footer.y, L.glass.w, 1 * s}, 0, kWhite, 0.08f);
    if (sh.pad) {  // the controller's buttons
        const padglyph::Family f = sh.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
        const padglyph::Button b[4] = {padglyph::Button::A, padglyph::Button::Y, padglyph::Button::X,
                                       padglyph::Button::B};
        for (size_t i = 0; i < L.padGlyphs.size() && i < 4; ++i)
            padglyph::paint(out, b[i], f, L.padGlyphs[i].x, L.padGlyphs[i].y, kGlyph * s);
    }
}

std::vector<Words> words(const Layout& L, const Data& d, const Show& sh, const Texts& t, const Measure& m) {
    std::vector<Words> out;
    const float s = L.s, side = kSide * s;
    const float titleW = m(t.title, 15 * s, true) + 2;
    out.push_back(Words{panel::Box{side, 0, titleW, L.title.h}, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{panel::Box{side + titleW + 8 * s, 0, m(t.hotkey, 11 * s, false) + 2, L.title.h}, t.hotkey,
                        kFaint, 11 * s, false, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", sh.hoverClose ? kText : kMuted, 16 * s, false, Align::Center});
    for (size_t i = 0; i < L.tabs.size(); ++i)
        out.push_back(Words{L.tabs[i], d.tabs[i].name, int(i) == L.tab ? kText : kMuted, 12.5f * s, int(i) == L.tab,
                            Align::Center});
    const Tab* tab = tabOf(d, L.tab);
    for (size_t i = 0; i < L.rows.size() && tab; ++i) {
        const Skill& k = tab->skills[size_t(L.first) + i];
        const panel::Box& r = L.rows[i];
        const float textX = r.x + 48 * s;
        const float textRight = (L.less[i].w > 0 ? L.less[i].x : L.dots[i].w > 0 ? L.dots[i].x : r.x + r.w * 0.6f) -
                                10 * s;
        const uint32_t nameRgb = k.level > 0 ? kText : kMuted;
        out.push_back(Words{panel::Box{textX, r.y + 5 * s, textRight - textX, 18 * s}, k.name, nameRgb, 13.5f * s,
                            true, Align::Left});
        out.push_back(Words{panel::Box{textX, r.y + 24 * s, textRight - textX, 16 * s}, info(k, t), kMuted, 11.5f * s,
                            false, Align::Left});
        const float plusX = r.x + r.w - 8 * s - kPlus * s;
        const std::string level =
            k.most > 0 ? std::to_string(k.level) + "/" + std::to_string(k.most) : std::to_string(k.level);
        out.push_back(Words{panel::Box{plusX - 10 * s - kLevelW * s, r.y, kLevelW * s, r.h}, level,
                            k.level > 0 ? kText : kFaint, 12.5f * s, false, Align::Right});
        if (L.pluses[i].w > 0) out.push_back(Words{L.pluses[i], "+", kText, 15 * s, true, Align::Center});
        if (L.less[i].w > 0) {
            out.push_back(Words{L.less[i], "\xE2\x80\xB9", k.use > 1 ? kText : kFaint, 14 * s, true, Align::Center});
            out.push_back(Words{L.more[i], "\xE2\x80\xBA", k.use < k.level ? kText : kFaint, 14 * s, true,
                                Align::Center});
            const panel::Box num{L.less[i].x + L.less[i].w, L.less[i].y, L.more[i].x - L.less[i].x - L.less[i].w,
                                 L.less[i].h};
            out.push_back(Words{num, std::to_string(std::clamp(k.use, 1, k.level)), kGold, 12 * s, true,
                                Align::Center});
        }
    }
    const std::string points = fill(t.points, std::to_string(d.points));
    out.push_back(Words{panel::Box{side, L.footer.y, m(points, 12 * s, true) + 4, L.footer.h}, points,
                        d.points > 0 ? kGold : kFaint, 12 * s, true, Align::Left});
    if (!sh.pad) {
        const float dw = m(t.drag, 11.5f * s, false) + 2;
        out.push_back(Words{panel::Box{L.footer.w - side - dw, L.footer.y, dw, L.footer.h}, t.drag, kFaint, 11.5f * s,
                            false, Align::Right});
        return out;
    }
    const padglyph::Family f = sh.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
    const std::vector<PadItem> items = padItems(t);
    for (size_t i = 0; i < items.size() && i < L.padGlyphs.size(); ++i) {
        const padglyph::Letter l = padglyph::letter(items[i].button, f, L.padGlyphs[i].x, L.padGlyphs[i].y,
                                                    kGlyph * s);
        if (!l.text.empty()) out.push_back(Words{l.box, l.text, l.rgb, l.px, true, Align::Center});
        out.push_back(Words{L.padWords[i], *items[i].words, kText, 12 * s, false, Align::Left});
    }
    return out;
}

}  // namespace uagfx::skillwin
