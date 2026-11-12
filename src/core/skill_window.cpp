#include "core/skill_window.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

#include "core/pad_glyphs.hpp"
#include "core/skill_plan.hpp"
#include "core/ui_style.hpp"

namespace uagfx::skillwin {
namespace {

// The concept's sizes at scale 1: 700 wide, the title 46, the tabs 32, a row 46, the footer 40
constexpr float kW = 700, kTreeW = 1000, kSideW = 268, kTitleH = 46, kTabsH = 32, kRowH = 46, kFooterH = 40, kSide = 16;
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

std::string fill2(std::string form, const std::string& a, const std::string& b) {  // {1}, {2}
    for (const auto& [key, v] : {std::pair<std::string, std::string>{"{1}", a}, {"{2}", b}})
        for (size_t at = form.find(key); at != std::string::npos; at = form.find(key, at + v.size()))
            form.replace(at, key.size(), v);
    return form;
}

constexpr uint32_t kRose = 0xEF8FA0, kPlanBlue = 0x6EA3FF;

const Tab* tabOf(const Data& d, int tab) {
    if (d.tabs.empty()) return nullptr;
    return &d.tabs[size_t(std::clamp(tab, 0, int(d.tabs.size()) - 1))];
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

// The footer's right: «План»'s buttons, or the controller's; the glass's size
void footerEnd(Layout& L, const Show& sh, const Texts& t, const Measure& m) {
    const float s = L.s, w = L.title.w;
    if (sh.plan) {  // 3.0.11-beta.225: «Застосувати» at the right, «Скинути» left of it
        const float bh = 26 * s, by = L.footer.y + (L.footer.h - bh) / 2;
        const std::string& go = sh.applying ? t.applying : t.apply;
        const float aw = m(go, 12 * s, true) + 24 * s, rw = m(t.reset, 12 * s, false) + 24 * s;
        L.apply = panel::Box{w - kSide * s - aw, by, aw, bh};
        L.reset = panel::Box{L.apply.x - 8 * s - rw, by, rw, bh};
    } else if (sh.pad) {
        padBoxes(L, t, m);
    }
    L.glass = panel::Box{0, 0, w, L.footer.y + L.footer.h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(L.glass.h));
}

bool inView(const Layout& L, const panel::Box& b) {  // a window box wholly in the tree's view
    return b.x >= L.view.x - 0.5f && b.y >= L.view.y - 0.5f && b.x + b.w <= L.view.x + L.view.w + 0.5f &&
           b.y + b.h <= L.view.y + L.view.h + 0.5f;
}

panel::Box toWindow(const Layout& L, const panel::Box& c) {  // the content's box in the window
    return panel::Box{c.x + L.view.x - L.scrollX, c.y + L.view.y - L.scrollY, c.w, c.h};
}

int litIndex(const Show& sh) { return sh.hoverRow >= 0 ? sh.hoverRow : sh.hoverPlus >= 0 ? sh.hoverPlus : sh.focus; }

// 3.0.11-beta.229 (R-EQDQWH): the side panel of «Дерево» - the skill pointed at (else the controller's): its level,
// what it is or why closed, every requirement with its own level (the jobs before's too), what it opens, the path's
// points
void sideWords(std::vector<Words>& out, const Layout& L, const Data& d, const Show& sh, const Texts& t) {
    const float s = L.s;
    const panel::Box& p = L.side;
    if (p.w <= 0) return;
    const Tab* tab = tabOf(d, L.tab);
    const int lit = litIndex(sh);
    const float x = p.x + 14 * s, w = p.w - 28 * s;
    float y = p.y + 12 * s;
    const auto line = [&](const std::string& text, uint32_t rgb, float px, bool bold, float h) {
        out.push_back(Words{panel::Box{x, y, w, h * s}, text, rgb, px * s, bold, Align::Left});
        y += h * s;
    };
    if (!tab || lit < 0 || lit >= int(tab->skills.size())) {
        if (!t.sideHint.empty()) line(t.sideHint, kFaint, 12, false, 20);
        return;
    }
    const Skill& k = tab->skills[size_t(lit)];
    line(k.name, kText, 14.5f, true, 24);
    std::string lv = fill2(t.levelOf, std::to_string(k.level), std::to_string(std::max(k.most, k.level)));
    if (skillplan::extra(k) > 0 && !t.planned.empty()) lv += kDotSep + fill(t.planned, std::to_string(k.target));
    line(lv, skillplan::extra(k) > 0 ? kPlanBlue : k.level > 0 ? kGold2 : kMuted, 12, false, 20);
    if (k.lock != Lock::Needs) line(info(k, t), kMuted, 11.5f, false, 22);
    y += 6 * s;
    if (!k.req.empty()) {
        line(t.needsHead, kFaint, 11, true, 18);
        for (const auto& [need, want] : k.req) {
            const Skill* r = skilltreeview::find(d, need);
            const int have = skilltreeview::levelOf(d, need);
            const bool met = have >= want;
            const std::string name = (r ? r->name : "#" + std::to_string(need)) + " " + std::to_string(want);
            out.push_back(Words{panel::Box{x, y, w - 46 * s, 19 * s}, name, met ? kText : 0xC9D0DC, 12 * s, false,
                                Align::Left});
            out.push_back(Words{panel::Box{x + w - 46 * s, y, 46 * s, 19 * s},
                                met ? "\xE2\x9C\x93" : std::to_string(have) + "/" + std::to_string(want),
                                met ? kGold : kRose, 12 * s, true, Align::Right});
            y += 19 * s;
        }
        y += 6 * s;
    }
    const float bottom = p.y + p.h - 10 * s;
    if (!k.spLevels.empty() && y + 38 * s < bottom) {  // 3.0.11-beta.230: SP by level, five a row
        if (!t.spByLevel.empty()) line(t.spByLevel, kFaint, 11, true, 18);
        const float cw = w / 5;
        for (size_t i = 0; i < k.spLevels.size() && i < 10; ++i) {
            const int lv = int(i) + 1;
            const uint32_t rgb = lv == k.level ? kGold : lv > k.level && lv <= k.target ? kPlanBlue : kMuted;
            out.push_back(Words{panel::Box{x + cw * float(i % 5), y + 17 * s * float(i / 5), cw, 17 * s},
                                std::to_string(lv) + ": " + std::to_string(k.spLevels[i]), rgb, 11 * s,
                                lv == k.level, Align::Left});
        }
        y += 17 * s * float((std::min<size_t>(k.spLevels.size(), 10) + 4) / 5) + 6 * s;
    }
    std::string opens;  // what it opens, on any tab
    int count = 0;
    for (const Tab& other : d.tabs)
        for (const Skill& o : other.skills)
            for (const auto& [need, want] : o.req)
                if (need == k.id && count < 5) {
                    opens += (opens.empty() ? "" : ", ") + o.name;
                    ++count;
                }
    if (!opens.empty() && y + 38 * s < bottom) {
        line(t.unlocks, kFaint, 11, true, 18);
        line(opens, kMuted, 11.5f, false, 20);
    }
    const bool byPoints = k.lock != Lock::Quest && k.lock != Lock::Wedding && k.lock != Lock::SoulLink;
    const int cost = k.level <= 0 && byPoints ? skilltreeview::pathCost(d, k.id) : 0;
    const float descEnd = bottom - (cost > 0 && !t.path.empty() ? 24 * s : 0);
    if (!k.desc.empty()) y += 4 * s;
    for (const std::string& l : k.desc) {  // 3.0.11-beta.230: the game's description, as much as there is room for
        if (y + 16 * s > descEnd) break;
        if (!l.empty()) out.push_back(Words{panel::Box{x, y, w, 16 * s}, l, 0xB7BFCC, 11 * s, false, Align::Left});
        y += (l.empty() ? 8 : 16) * s;
    }
    if (cost > 0 && !t.path.empty()) {
        y = std::max(y, p.y + p.h - 32 * s);
        line(fill2(t.path, k.name, std::to_string(cost)), cost > d.points ? kRose : kGold2, 12, true, 20);
    }
}

}  // namespace

bool canRaise(const Skill& k, int points) {
    return points > 0 && k.up && k.lock == Lock::None && (k.most <= 0 || k.level < k.most);
}

bool plusOn(const Data& d, const Skill& k, bool plan) {
    return plan ? skillplan::canPlan(d, k) : canRaise(k, d.points);
}

bool usable(const Skill& k) { return k.level > 0 && k.type != 0; }

std::string info(const Skill& k, const Texts& t) {
    switch (k.lock) {
        case Lock::Quest: return t.quest;
        case Lock::Wedding: return t.wedding;
        case Lock::SoulLink: return t.soulLink;
        case Lock::Earlier: return fill(t.earlier, k.lockJob);
        case Lock::NextJob: return t.afterChange;
        case Lock::Needs: return k.needs.empty() ? t.unlearned : fill(t.needs, k.needs);
        case Lock::None: break;
    }
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
    L.tree = sh.tree;
    // 3.0.11-beta.230: the side panel in «Список» too - the list as wide as before, the panel right of it
    const float w = (sh.tree ? kTreeW : kW + kSideW + 14) * s, side = kSide * s;
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.close = panel::Box{w - side - 26 * s, (kTitleH * s - 26 * s) / 2, 26 * s, 26 * s};
    if (!t.list.empty() && !t.tree.empty()) {  // 3.0.11-beta.224: the view's buttons left of the close
        const float bh = 24 * s, by = (kTitleH * s - bh) / 2;
        const float tw = m(t.tree, 12 * s, false) + 22 * s, lw = m(t.list, 12 * s, false) + 22 * s;
        L.viewTree = panel::Box{L.close.x - 12 * s - tw, by, tw, bh};
        L.viewList = panel::Box{L.viewTree.x - lw, by, lw, bh};
        if (!t.plan.empty()) {  // 3.0.11-beta.225
            const float pw = m(t.plan, 12 * s, false) + 22 * s;
            L.planButton = panel::Box{L.viewList.x - 10 * s - pw, by, pw, bh};
        }
    }
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
    const int shown = kRows;  // 3.0.11-beta.230: always eight rows high - room for the side panel
    L.first = std::clamp(sh.scroll, 0, std::max(0, count - kRows));
    const bool scrolls = count > kRows;
    float listY = L.tabsRow.y + L.tabsRow.h + 6 * s;
    if (!d.notice.empty()) {  // 3.0.11-beta.223: why skills are closed
        L.notice = panel::Box{side, listY, w - 2 * side, 28 * s};
        listY += L.notice.h + 6 * s;
    }
    if (tab && tab->choices.size() > 1) {  // 3.0.11-beta.226: «Наступна»'s jobs as buttons
        float cx = side;
        for (const std::string& c : tab->choices) {
            const float cw = m(c, 12 * s, true) + 26 * s;
            L.choices.push_back(panel::Box{cx, listY, cw, 26 * s});
            cx += cw + 6 * s;
        }
        listY += 26 * s + 8 * s;
    }
    if (sh.tree) {  // 3.0.11-beta.224: «Дерево»
        // 3.0.11-beta.229 (R-EQDQWH): the side panel of the skill pointed at on the right
        const float sideW = kSideW * s;
        L.view = panel::Box{side, listY, w - 2 * side - 10 * s - sideW - 14 * s, float(shown) * kRowH * s - 10 * s};
        L.side = panel::Box{w - side - sideW, listY, sideW, float(shown) * kRowH * s};
        L.list = panel::Box{side, listY, w - 2 * side, float(shown) * kRowH * s};
        L.plan = skilltreeview::arrange(d, L.tab, s);
        L.maxX = std::max(0.0f, L.plan.w - L.view.w);
        L.maxY = std::max(0.0f, L.plan.h - L.view.h);
        L.scrollX = std::clamp(sh.treeX * s, 0.0f, L.maxX);
        L.scrollY = std::clamp(sh.treeY * s, 0.0f, L.maxY);
        if (L.maxY > 0) L.scroll = panel::Box{L.view.x + L.view.w + 4 * s, L.view.y + 2 * s, 5 * s, L.view.h - 4 * s};
        if (L.maxX > 0) L.scrollH = panel::Box{L.view.x + 2 * s, L.view.y + L.view.h + 4 * s, L.view.w - 4 * s, 5 * s};
        L.first = 0;
        for (const skilltreeview::Node& n : L.plan.nodes) {
            const Skill& k = tab->skills[size_t(n.index)];
            const panel::Box b = toWindow(L, n.box);
            L.rows.push_back(b);
            L.icons.push_back(panel::Box{b.x + 6 * s, b.y + (b.h - 26 * s) / 2, 26 * s, 26 * s});
            const float px = b.x + b.w - skilltreeview::kPlus * s * 0.65f, ps = skilltreeview::kPlus * s;
            L.pluses.push_back(plusOn(d, k, sh.plan) ? panel::Box{px, b.y - 5 * s, ps, ps} : panel::Box{});
            L.minuses.push_back(sh.plan && skillplan::extra(k) > 0 ? panel::Box{px - ps - 3 * s, b.y - 5 * s, ps, ps}
                                                                    : panel::Box{});
            L.less.push_back(panel::Box{});
            L.more.push_back(panel::Box{});
            L.dots.push_back(panel::Box{});
        }
        L.footer = panel::Box{0, L.list.y + L.list.h + 6 * s, w, kFooterH * s};
        footerEnd(L, sh, t, m);
        L.x = x;
        L.y = y;
        return L;
    }
    const float sideW = kSideW * s, bodyW = w - 2 * side - sideW - 14 * s;
    const float rowW = bodyW - (scrolls ? 12 * s : 0);
    L.list = panel::Box{side, listY, bodyW, float(shown) * kRowH * s};
    L.side = panel::Box{w - side - sideW, listY, sideW, L.list.h};
    if (scrolls) L.scroll = panel::Box{side + bodyW - 5 * s, listY + 4 * s, 5 * s, L.list.h - 8 * s};
    for (int i = L.first; i < count && i < L.first + kRows; ++i) {
        const Skill& k = tab->skills[size_t(i)];
        const panel::Box r{side, listY + float(i - L.first) * kRowH * s, rowW, kRowH * s};
        L.rows.push_back(r);
        L.icons.push_back(panel::Box{r.x + 6 * s, r.y + (r.h - 32 * s) / 2, 32 * s, 32 * s});
        const float plusX = r.x + r.w - 8 * s - kPlus * s;
        L.pluses.push_back(plusOn(d, k, sh.plan)
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
        if (sh.plan && skillplan::extra(k) > 0) {  // 3.0.11-beta.225: «−» where the use level's arrows stand
            const float cx = dotsRight - kDotsMost * s - 14 * s - kPlus * s;
            L.minuses.push_back(panel::Box{cx, r.y + (r.h - kPlus * s) / 2, kPlus * s, kPlus * s});
        } else {
            L.minuses.push_back(panel::Box{});
        }
        if (usable(k) && k.level > 1 && !sh.plan) {
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
    footerEnd(L, sh, t, m);
    L.x = x;
    L.y = y;
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    if (L.planButton.w > 0 && L.planButton.contains(x, y)) return Hit{Part::Plan, -1};
    if (L.apply.w > 0 && L.apply.contains(x, y)) return Hit{Part::Apply, -1};
    if (L.reset.w > 0 && L.reset.contains(x, y)) return Hit{Part::Reset, -1};
    if (L.viewList.w > 0 && L.viewList.contains(x, y)) return Hit{Part::ViewList, -1};
    if (L.viewTree.w > 0 && L.viewTree.contains(x, y)) return Hit{Part::ViewTree, -1};
    for (size_t i = 0; i < L.tabs.size(); ++i)
        if (L.tabs[i].contains(x, y)) return Hit{Part::Tab, int(i)};
    for (size_t i = 0; i < L.choices.size(); ++i)
        if (L.choices[i].contains(x, y)) return Hit{Part::Choice, int(i)};
    const auto nearBar = [&](const panel::Box& b) {  // 3.0.11-beta.245: a scroll bar, a little wider to take
        const float m = 4 * L.s;
        return b.w > 0 && x >= b.x - m && x <= b.x + b.w + m && y >= b.y - m && y <= b.y + b.h + m;
    };
    if (nearBar(L.scroll)) return Hit{Part::BarV, -1};
    if (L.tree && nearBar(L.scrollH)) return Hit{Part::BarH, -1};
    if (L.tree) {  // 3.0.11-beta.224: only what the view shows
        if (!L.view.contains(x, y)) return Hit{y < L.title.y + L.title.h ? Part::Title : Part::Body, -1};
        for (size_t i = 0; i < L.rows.size(); ++i) {
            if (L.pluses[i].w > 0 && L.pluses[i].contains(x, y)) return Hit{Part::Plus, int(i)};
            if (i < L.minuses.size() && L.minuses[i].w > 0 && L.minuses[i].contains(x, y))
                return Hit{Part::Minus, int(i)};
        }
        for (size_t i = 0; i < L.rows.size(); ++i)
            if (L.rows[i].contains(x, y)) return Hit{Part::Row, int(i)};
        return Hit{Part::View, -1};
    }
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const int index = L.first + int(i);
        if (L.pluses[i].w > 0 && L.pluses[i].contains(x, y)) return Hit{Part::Plus, index};
        if (i < L.minuses.size() && L.minuses[i].w > 0 && L.minuses[i].contains(x, y)) return Hit{Part::Minus, index};
        if (L.less[i].w > 0 && L.less[i].contains(x, y)) return Hit{Part::Less, index};
        if (L.more[i].w > 0 && L.more[i].contains(x, y)) return Hit{Part::More, index};
        if (L.rows[i].contains(x, y)) return Hit{Part::Row, index};
    }
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

std::vector<Words> words(const Layout& L, const Data& d, const Show& sh, const Texts& t, const Measure& m) {
    std::vector<Words> out;
    const float s = L.s, side = kSide * s;
    const float titleW = m(t.title, 15 * s, true) + 2;
    out.push_back(Words{panel::Box{side, 0, titleW, L.title.h}, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{panel::Box{side + titleW + 8 * s, 0, m(t.hotkey, 11 * s, false) + 2, L.title.h}, t.hotkey,
                        kFaint, 11 * s, false, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", sh.hoverClose ? kText : kMuted, 16 * s, false, Align::Center});
    if (L.planButton.w > 0)  // 3.0.11-beta.225
        out.push_back(Words{L.planButton, t.plan, sh.plan ? kText : kMuted, 12 * s, sh.plan, Align::Center});
    if (L.apply.w > 0) {
        out.push_back(Words{L.apply, sh.applying ? t.applying : t.apply,
                            skillplan::cost(d) > 0 || sh.applying ? kText : kFaint, 12 * s, true, Align::Center});
        out.push_back(Words{L.reset, t.reset, kMuted, 12 * s, false, Align::Center});
    }
    for (size_t i = 0; i < L.minuses.size(); ++i)
        if (L.minuses[i].w > 0 && (!L.tree || inView(L, L.minuses[i])))
            out.push_back(Words{L.minuses[i], "\xE2\x88\x92", kText, 14 * s, true, Align::Center});
    if (L.viewList.w > 0) {  // 3.0.11-beta.224
        out.push_back(Words{L.viewList, t.list, L.tree ? kMuted : kText, 12 * s, !L.tree, Align::Center});
        out.push_back(Words{L.viewTree, t.tree, L.tree ? kText : kMuted, 12 * s, L.tree, Align::Center});
    }
    for (size_t i = 0; i < L.tabs.size(); ++i)
        out.push_back(Words{L.tabs[i], d.tabs[i].name, int(i) == L.tab ? kText : kMuted, 12.5f * s, int(i) == L.tab,
                            Align::Center});
    if (L.notice.w > 0)
        out.push_back(Words{panel::Box{L.notice.x + 10 * s, L.notice.y, L.notice.w - 20 * s, L.notice.h}, d.notice,
                            kText, 12 * s, false, Align::Left});
    if (const Tab* shown = tabOf(d, L.tab))  // 3.0.11-beta.226
        for (size_t i = 0; i < L.choices.size() && i < shown->choices.size(); ++i)
            out.push_back(Words{L.choices[i], shown->choices[i], int(i) == shown->chosen ? kText : kMuted, 12 * s,
                                int(i) == shown->chosen, Align::Center});
    const Tab* tab = tabOf(d, L.tab);
    if (L.tree && tab) {  // 3.0.11-beta.224: «Дерево» - the words wholly in its view
        for (size_t i = 0; i < L.rows.size(); ++i) {
            const Skill& k = tab->skills[i];
            const panel::Box& r = L.rows[i];
            const panel::Box name{r.x + 38 * s, r.y + 4 * s, r.w - 44 * s, 17 * s};
            const panel::Box level{r.x + 38 * s, r.y + 21 * s, r.w - 44 * s, 15 * s};
            const uint32_t rgb = k.lock != Lock::None ? kMuted : k.level > 0 ? kText : 0xC9D0DC;
            if (inView(L, name)) out.push_back(Words{name, k.name, rgb, 12 * s, true, Align::Left});
            const bool planned = skillplan::extra(k) > 0;
            const int shownLv = planned ? k.target : k.level;
            const std::string lv =
                k.most > 0 ? std::to_string(shownLv) + "/" + std::to_string(k.most) : std::to_string(shownLv);
            if (inView(L, level))
                out.push_back(Words{level, lv, planned ? kPlanBlue : k.level > 0 ? kGold2 : kFaint, 10.5f * s, planned,
                                    Align::Left});
            if (L.pluses[i].w > 0 && inView(L, L.pluses[i]))
                out.push_back(Words{L.pluses[i], "+", kCard, 13 * s, true, Align::Center});
        }
    }
    sideWords(out, L, d, sh, t);  // 3.0.11-beta.230: in both views
    for (size_t i = 0; i < L.rows.size() && tab && !L.tree; ++i) {
        const Skill& k = tab->skills[size_t(L.first) + i];
        const panel::Box& r = L.rows[i];
        const float textX = r.x + 48 * s;
        const float textRight = (L.less[i].w > 0 ? L.less[i].x : L.dots[i].w > 0 ? L.dots[i].x : r.x + r.w * 0.6f) -
                                10 * s;
        const uint32_t nameRgb = k.lock != Lock::None ? kFaint : k.level > 0 ? kText : kMuted;
        out.push_back(Words{panel::Box{textX, r.y + 5 * s, textRight - textX, 18 * s}, k.name, nameRgb, 13.5f * s,
                            true, Align::Left});
        out.push_back(Words{panel::Box{textX, r.y + 24 * s, textRight - textX, 16 * s}, info(k, t), kMuted, 11.5f * s,
                            false, Align::Left});
        const float plusX = r.x + r.w - 8 * s - kPlus * s;
        const bool planned = skillplan::extra(k) > 0;  // 3.0.11-beta.225: the planned level in blue
        const int shownLv = planned ? k.target : k.level;
        const std::string level =
            k.most > 0 ? std::to_string(shownLv) + "/" + std::to_string(k.most) : std::to_string(shownLv);
        out.push_back(Words{panel::Box{plusX - 10 * s - kLevelW * s, r.y, kLevelW * s, r.h}, level,
                            planned ? kPlanBlue : k.level > 0 ? kText : kFaint, 12.5f * s, planned, Align::Right});
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
    const int planCost = skillplan::cost(d);  // 3.0.11-beta.225: the points the plan leaves, and what it takes
    std::string points = fill(t.points, std::to_string(planCost > 0 ? skillplan::left(d) : d.points));
    if (planCost > 0 && !t.planned.empty()) points += kDotSep + fill(t.planned, std::to_string(planCost));
    out.push_back(Words{panel::Box{side, L.footer.y, m(points, 12 * s, true) + 4, L.footer.h}, points,
                        planCost > 0 ? kPlanBlue : d.points > 0 ? kGold : kFaint, 12 * s, true, Align::Left});
    if (sh.plan) return out;  // its buttons in the hints' place
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

void scrollTree(Show& sh, const Layout& L, float dx, float dy) {
    if (!L.tree || L.s <= 0) return;
    sh.treeX = std::clamp(L.scrollX + dx, 0.0f, L.maxX) / L.s;
    sh.treeY = std::clamp(L.scrollY + dy, 0.0f, L.maxY) / L.s;
}

void wheelTree(Show& sh, const Layout& L, float by, bool shift) {
    if (shift || (L.maxY <= 0 && L.maxX > 0))
        scrollTree(sh, L, by, 0);
    else
        scrollTree(sh, L, 0, by);
}

bool treeInSight(Show& sh, const Layout& L, int index) {
    if (!L.tree || index < 0 || index >= int(L.rows.size())) return false;
    const panel::Box& b = L.rows[size_t(index)];
    const float pad = 12 * L.s;
    float dx = 0, dy = 0;
    if (b.x - pad < L.view.x) dx = b.x - pad - L.view.x;
    if (b.x + b.w + pad > L.view.x + L.view.w) dx = b.x + b.w + pad - L.view.x - L.view.w;
    if (b.y - pad < L.view.y) dy = b.y - pad - L.view.y;
    if (b.y + b.h + pad > L.view.y + L.view.h) dy = b.y + b.h + pad - L.view.y - L.view.h;
    if (dx == 0 && dy == 0) return false;
    const float x = sh.treeX, y = sh.treeY;
    scrollTree(sh, L, dx, dy);
    return std::abs(sh.treeX - x) > 0.01f || std::abs(sh.treeY - y) > 0.01f;
}

int treeStep(const Layout& L, int from, int dx, int dy) {
    if (!L.tree || from < 0 || from >= int(L.plan.nodes.size())) return from;
    const panel::Box& a = L.plan.nodes[size_t(from)].box;
    const float ax = a.x + a.w / 2, ay = a.y + a.h / 2;
    int best = from;
    float bestScore = 0;
    for (size_t i = 0; i < L.plan.nodes.size(); ++i) {
        if (int(i) == from) continue;
        const panel::Box& b = L.plan.nodes[i].box;
        const float ex = b.x + b.w / 2 - ax, ey = b.y + b.h / 2 - ay;
        const float along = dx ? ex * float(dx) : ey * float(dy), across = std::abs(dx ? ey : ex);
        if (along < 1.0f) continue;  // not that way
        const float score = along + 2 * across;
        if (best == from || score < bestScore) {
            best = int(i);
            bestScore = score;
        }
    }
    return best;
}

void barTo(Show& sh, const Layout& L, const Data& d, bool horizontal, float x, float y) {
    const float s = L.s > 0 ? L.s : 1.0f;
    if (L.tree) {
        const panel::Box& b = horizontal ? L.scrollH : L.scroll;
        if (b.w <= 0) return;
        const float track = horizontal ? b.w : b.h, view = horizontal ? L.view.w : L.view.h;
        const float whole = horizontal ? L.plan.w : L.plan.h, most = horizontal ? L.maxX : L.maxY;
        const float thumb = std::max(track * view / std::max(whole, 1.0f), 16 * s);
        const float at = (horizontal ? x - b.x : y - b.y) - thumb / 2;
        const float part = std::clamp(at / std::max(1.0f, track - thumb), 0.0f, 1.0f);
        (horizontal ? sh.treeX : sh.treeY) = part * most / s;
        return;
    }
    const Tab* tab = tabOf(d, L.tab);
    if (!tab || L.scroll.w <= 0) return;
    const float count = float(tab->skills.size());
    const float thumb = std::max(L.scroll.h * float(kRows) / count, 16 * s);
    const float part = std::clamp((y - L.scroll.y - thumb / 2) / std::max(1.0f, L.scroll.h - thumb), 0.0f, 1.0f);
    sh.scroll = int(std::lround(part * (count - float(kRows))));
}

TreeView treeView(const Layout& L, const Data& d, const Show& sh) {
    TreeView v;
    const float s = L.s;
    const Tab* tab = tabOf(d, L.tab);
    if (!L.tree || !tab || L.plan.w <= 0 || L.plan.h <= 0) return v;
    const float ox = L.view.x - L.scrollX, oy = L.view.y - L.scrollY;  // the content's corner in the window
    const auto moved = [&](const panel::Box& b) { return panel::Box{b.x + ox, b.y + oy, b.w, b.h}; };
    const int lit = litIndex(sh);
    const bool litOk = lit >= 0 && lit < int(tab->skills.size());
    std::set<int> chain = litOk ? skilltreeview::chain(d, tab->skills[size_t(lit)].id) : std::set<int>{};
    if (litOk) chain.insert(tab->skills[size_t(lit)].id);
    for (const skilltreeview::Link& l : L.plan.links) {
        const skilltreeview::Route r = skilltreeview::route(L.plan, l, s);
        const bool on = chain.count(tab->skills[size_t(l.to)].id) && chain.count(tab->skills[size_t(l.from)].id);
        const uint32_t rgb = on ? kGold : l.met ? kGold2 : kWhite;
        const float a = on ? 0.95f : litOk ? 0.12f : l.met ? 0.45f : 0.2f, t = (on ? 2.0f : 1.3f) * s;
        for (int k = 0; k + 1 < r.n; ++k) {
            const float x0 = std::min(r.x[k], r.x[k + 1]), x1 = std::max(r.x[k], r.x[k + 1]);
            const float y0 = std::min(r.y[k], r.y[k + 1]), y1 = std::max(r.y[k], r.y[k + 1]);
            v.links.push_back(TreeLink{moved(panel::Box{x0 - t / 2, y0 - t / 2, x1 - x0 + t, y1 - y0 + t}), rgb, a});
        }
    }
    for (const skilltreeview::Node& n : L.plan.nodes) {
        const Skill& k = tab->skills[size_t(n.index)];
        TreeNode x;
        const bool closed = k.lock != Lock::None, learned = k.level > 0;
        x.fillA = closed ? 0.05f : learned ? 0.10f : 0.07f;
        if (n.index == sh.hoverRow || n.index == sh.hoverPlus) x.fillA += 0.06f;
        x.fill = learned ? kGold2 : kWhite;
        x.edge = learned ? kGold : kWhite;
        x.edgeA = closed ? 0.18f : learned ? 0.6f : 0.3f, x.edgeW = 1.0f * s;
        if (chain.count(k.id)) x.edge = kGold, x.edgeA = 0.95f;
        if (skillplan::extra(k) > 0 && !chain.count(k.id)) x.edge = kPlanBlue, x.edgeA = 0.85f;
        if (n.index == sh.focus) x.edge = kGold, x.edgeA = 1.0f, x.edgeW = 1.8f * s;
        x.box = moved(n.box);
        x.icon = moved(panel::Box{n.box.x + 6 * s, n.box.y + (n.box.h - 26 * s) / 2, 26 * s, 26 * s});
        x.picture = k.icon && k.icon->width > 0 ? k.icon : nullptr;
        x.iconOpacity = closed ? 0.45f : learned ? 1.0f : 0.6f;
        const float px = n.box.x + n.box.w - skilltreeview::kPlus * s * 0.65f, ps = skilltreeview::kPlus * s;
        if (plusOn(d, k, sh.plan)) {
            x.plus = moved(panel::Box{px, n.box.y - 5 * s, ps, ps});
            x.plusRgb = sh.plan ? kPlanBlue : kGold;
            x.plusA = n.index == sh.hoverPlus ? 1.0f : 0.85f;
        }
        if (sh.plan && skillplan::extra(k) > 0) {
            x.minus = moved(panel::Box{px - ps - 3 * s, n.box.y - 5 * s, ps, ps});
            x.minusA = n.index == sh.hoverMinus ? 1.0f : 0.7f;
        }
        v.nodes.push_back(x);
    }
    return v;
}

}  // namespace uagfx::skillwin
