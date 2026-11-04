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
constexpr float kW = 700, kTreeW = 900, kTitleH = 46, kTabsH = 32, kRowH = 46, kFooterH = 40, kSide = 16;
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

// 3.0.11-beta.224 (stage 2): a link of «Дерево» in the content's pixels - from its requirement's right to the skill's
// left, the links into one skill spread down its left edge; an elbow at x - 14
struct Elbow {
    float x1, y1, xm, x2, y2;
};
Elbow elbowOf(const Layout& L, size_t link) {
    const skilltreeview::Link& l = L.plan.links[link];
    const panel::Box& to = L.plan.nodes[size_t(l.to)].box;
    const panel::Box& from = l.chip ? L.plan.chips[size_t(l.from)].box : L.plan.nodes[size_t(l.from)].box;
    int k = 0, n = 0;
    for (size_t i = 0; i < L.plan.links.size(); ++i)
        if (L.plan.links[i].to == l.to) {
            if (i < link) ++k;
            ++n;
        }
    const float y2 = to.y + to.h * float(k + 1) / float(n + 1);
    return Elbow{from.x + from.w, from.y + from.h / 2, to.x - 14 * L.s, to.x, y2};
}

bool inView(const Layout& L, const panel::Box& b) {  // a window box wholly in the tree's view
    return b.x >= L.view.x - 0.5f && b.y >= L.view.y - 0.5f && b.x + b.w <= L.view.x + L.view.w + 0.5f &&
           b.y + b.h <= L.view.y + L.view.h + 0.5f;
}

panel::Box toWindow(const Layout& L, const panel::Box& c) {  // the content's box in the window
    return panel::Box{c.x + L.view.x - L.scrollX, c.y + L.view.y - L.scrollY, c.w, c.h};
}

int litIndex(const Show& sh) { return sh.hoverRow >= 0 ? sh.hoverRow : sh.hoverPlus >= 0 ? sh.hoverPlus : sh.focus; }

void paintTree(panel::Picture& out, const Layout& L, const Data& d, const Show& sh) {
    const float s = L.s;
    const Tab* tab = tabOf(d, L.tab);
    if (!tab || L.plan.w <= 0 || L.plan.h <= 0) return;
    panel::Picture c;
    c.width = int(std::ceil(L.plan.w));
    c.height = int(std::ceil(L.plan.h));
    c.px.assign(size_t(c.width) * size_t(c.height), 0u);
    const int lit = litIndex(sh);
    const bool litOk = lit >= 0 && lit < int(tab->skills.size());
    std::set<int> chain = litOk ? skilltreeview::chain(d, tab->skills[size_t(lit)].id) : std::set<int>{};
    if (litOk) chain.insert(tab->skills[size_t(lit)].id);
    for (size_t i = 0; i < L.plan.links.size(); ++i) {
        const skilltreeview::Link& l = L.plan.links[i];
        const Elbow e = elbowOf(L, i);
        const int fromId = l.chip ? L.plan.chips[size_t(l.from)].id : tab->skills[size_t(l.from)].id;
        const bool on = chain.count(tab->skills[size_t(l.to)].id) && chain.count(fromId);
        const uint32_t rgb = on ? kGold : l.met ? kGold2 : kWhite;
        const float a = on ? 0.95f : l.met ? 0.5f : 0.16f, t = (on ? 2.0f : 1.4f) * s;
        panel::roundRect(c, panel::Box{e.x1, e.y1 - t / 2, std::max(0.0f, e.xm - e.x1 + t / 2), t}, 0, rgb, a);
        const float top = std::min(e.y1, e.y2) - t / 2, bottom = std::max(e.y1, e.y2) + t / 2;
        panel::roundRect(c, panel::Box{e.xm - t / 2, top, t, bottom - top}, 0, rgb, a);
        panel::roundRect(c, panel::Box{e.xm, e.y2 - t / 2, e.x2 - e.xm, t}, 0, rgb, a);
    }
    for (const skilltreeview::Chip& ch : L.plan.chips)
        panel::roundRect(c, ch.box, 6 * s, kWhite, chain.count(ch.id) ? 0.10f : 0.04f, chain.count(ch.id) ? kGold
                                                                                                          : kWhite,
                         chain.count(ch.id) ? 0.8f : 0.2f, 1.0f * s);
    for (const skilltreeview::Node& n : L.plan.nodes) {
        const Skill& k = tab->skills[size_t(n.index)];
        const bool closed = k.lock != Lock::None, learned = k.level > 0;
        float fillA = closed ? 0.03f : learned ? 0.10f : 0.06f;
        if (n.index == sh.hoverRow || n.index == sh.hoverPlus) fillA += 0.06f;
        uint32_t edge = learned ? kGold : kWhite;
        float edgeA = closed ? 0.10f : learned ? 0.6f : 0.25f, edgeW = 1.0f * s;
        if (chain.count(k.id)) {
            edge = kGold;
            edgeA = 0.95f;
        }
        if (skillplan::extra(k) > 0 && !chain.count(k.id)) {  // 3.0.11-beta.225: planned
            edge = kPlanBlue;
            edgeA = 0.85f;
        }
        if (n.index == sh.focus) {
            edge = kGold;
            edgeA = 1.0f;
            edgeW = 1.8f * s;
        }
        panel::roundRect(c, n.box, 8 * s, learned ? kGold2 : kWhite, fillA, edge, edgeA, edgeW);
        const panel::Box icon{n.box.x + 6 * s, n.box.y + (n.box.h - 26 * s) / 2, 26 * s, 26 * s};
        if (k.icon && k.icon->width > 0) put(c, *k.icon, icon, closed ? 0.3f : learned ? 1.0f : 0.5f);
        const float px = n.box.x + n.box.w - skilltreeview::kPlus * s * 0.65f, ps = skilltreeview::kPlus * s;
        if (plusOn(d, k, sh.plan))
            panel::roundRect(c, panel::Box{px, n.box.y - 5 * s, ps, ps}, 5 * s, sh.plan ? kPlanBlue : kGold,
                             n.index == sh.hoverPlus ? 1.0f : 0.85f);
        if (sh.plan && skillplan::extra(k) > 0)
            panel::roundRect(c, panel::Box{px - ps - 3 * s, n.box.y - 5 * s, ps, ps}, 5 * s, kCard, 1.0f, kPlanBlue,
                             n.index == sh.hoverMinus ? 1.0f : 0.7f, 1.0f * s);
    }
    const int x0 = int(std::lround(L.view.x)), y0 = int(std::lround(L.view.y));
    const int sx = int(std::lround(L.scrollX)), sy = int(std::lround(L.scrollY));
    const int vw = int(std::lround(L.view.w)), vh = int(std::lround(L.view.h));
    for (int y = 0; y < vh; ++y) {
        const int cy = y + sy, ty = y0 + y;
        if (cy < 0 || cy >= c.height || ty < 0 || ty >= out.height) continue;
        for (int x = 0; x < vw; ++x) {
            const int cx = x + sx, tx = x0 + x;
            if (cx < 0 || cx >= c.width || tx < 0 || tx >= out.width) continue;
            const uint32_t v = c.px[size_t(cy) * size_t(c.width) + size_t(cx)];
            if (!(v >> 24)) continue;
            uint32_t& o = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (v >> 24);
            const auto ch = [&](int sh2) {
                return std::min<uint32_t>(255, ((v >> sh2) & 0xFF) + ((o >> sh2) & 0xFF) * keep / 255);
            };
            o = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
    }
    if (L.scroll.w > 0) {  // its scroll bars
        panel::roundRect(out, L.scroll, L.scroll.w / 2, kWhite, 0.05f);
        const float h = std::max(L.scroll.h * L.view.h / L.plan.h, 16 * s);
        const float y = L.scroll.y + (L.scroll.h - h) * (L.maxY > 0 ? L.scrollY / L.maxY : 0);
        panel::roundRect(out, panel::Box{L.scroll.x, y, L.scroll.w, h}, L.scroll.w / 2, kWhite, 0.28f);
    }
    if (L.scrollH.w > 0) {
        panel::roundRect(out, L.scrollH, L.scrollH.h / 2, kWhite, 0.05f);
        const float w = std::max(L.scrollH.w * L.view.w / L.plan.w, 16 * s);
        const float x = L.scrollH.x + (L.scrollH.w - w) * (L.maxX > 0 ? L.scrollX / L.maxX : 0);
        panel::roundRect(out, panel::Box{x, L.scrollH.y, w, L.scrollH.h}, L.scrollH.h / 2, kWhite, 0.28f);
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
    const float w = (sh.tree ? kTreeW : kW) * s, side = kSide * s;
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
    const int shown = sh.tree ? kRows : rowsShown(d);
    L.first = std::clamp(sh.scroll, 0, std::max(0, count - kRows));
    const bool scrolls = count > kRows;
    float listY = L.tabsRow.y + L.tabsRow.h + 6 * s;
    if (!d.notice.empty()) {  // 3.0.11-beta.223: why skills are closed
        L.notice = panel::Box{side, listY, w - 2 * side, 28 * s};
        listY += L.notice.h + 6 * s;
    }
    if (sh.tree) {  // 3.0.11-beta.224: «Дерево»
        L.view = panel::Box{side, listY, w - 2 * side - 10 * s, float(shown) * kRowH * s - 10 * s};
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
    const float rowW = w - 2 * side - (scrolls ? 12 * s : 0);
    L.list = panel::Box{side, listY, w - 2 * side, float(shown) * kRowH * s};
    if (scrolls) L.scroll = panel::Box{w - side - 5 * s, listY + 4 * s, 5 * s, L.list.h - 8 * s};
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

void paint(panel::Picture& out, const Layout& L, const Data& d, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.09f, 1.0f * s);
    if (sh.hoverClose) panel::roundRect(out, L.close, 7 * s, kWhite, 0.12f);
    if (L.planButton.w > 0)  // 3.0.11-beta.225: «План», lit blue while on
        panel::roundRect(out, L.planButton, 7 * s, sh.plan ? kPlanBlue : kWhite,
                         (sh.plan ? 0.24f : 0.03f) + (sh.hoverPlan ? 0.07f : 0.0f), sh.plan ? kPlanBlue : kWhite,
                         sh.plan ? 0.8f : 0.16f, 1.0f * s);
    if (L.apply.w > 0) {
        const bool any = skillplan::cost(d) > 0 && !sh.applying;
        panel::roundRect(out, L.apply, 7 * s, kPlanBlue, (any ? 0.32f : 0.08f) + (sh.hoverApply && any ? 0.1f : 0.0f),
                         kPlanBlue, any ? 0.9f : 0.3f, 1.0f * s);
        panel::roundRect(out, L.reset, 7 * s, kWhite, sh.hoverReset ? 0.12f : 0.05f, kWhite, 0.18f, 1.0f * s);
    }
    for (size_t i = 0; i < L.minuses.size(); ++i)  // 3.0.11-beta.225: «−» (the tree's: drawn by its own)
        if (L.minuses[i].w > 0 && !L.tree)
            panel::roundRect(out, L.minuses[i], 6 * s, kWhite, int(i) + L.first == sh.hoverMinus ? 0.22f : 0.10f,
                             kPlanBlue, 0.7f, 1.0f * s);
    if (L.viewList.w > 0) {  // 3.0.11-beta.224: «Список» | «Дерево», the shown one lit
        const panel::Box both{L.viewList.x, L.viewList.y, L.viewTree.x + L.viewTree.w - L.viewList.x, L.viewList.h};
        panel::roundRect(out, both, 7 * s, kWhite, 0.03f, kWhite, 0.16f, 1.0f * s);
        const panel::Box& on = L.tree ? L.viewTree : L.viewList;
        panel::roundRect(out, on, 7 * s, kWhite, 0.14f);
        const panel::Box& off = L.tree ? L.viewList : L.viewTree;
        if (L.tree ? sh.hoverList : sh.hoverTree) panel::roundRect(out, off, 7 * s, kWhite, 0.07f);
    }
    for (size_t i = 0; i < L.tabs.size(); ++i) {  // the tab shown underlined in gold
        const panel::Box& b = L.tabs[i];
        if (int(i) == sh.hoverTab && int(i) != L.tab) panel::roundRect(out, b, 6 * s, kWhite, 0.05f);
        if (int(i) == L.tab)
            panel::roundRect(out, panel::Box{b.x + 6 * s, b.y + b.h - 3 * s, b.w - 12 * s, 2 * s}, 1 * s, kGold, 1.0f);
    }
    panel::roundRect(out, panel::Box{L.tabsRow.x, L.tabsRow.y + L.tabsRow.h, L.tabsRow.w, 1 * s}, 0, kWhite, 0.08f);
    if (L.notice.w > 0) panel::roundRect(out, L.notice, 7 * s, kGold2, 0.12f, kGold2, 0.45f, 1.0f * s);
    const Tab* tab = tabOf(d, L.tab);
    if (L.tree) paintTree(out, L, d, sh);
    for (size_t i = 0; i < L.rows.size() && tab && !L.tree; ++i) {
        const int index = L.first + int(i);
        const Skill& k = tab->skills[size_t(index)];
        const panel::Box& r = L.rows[i];
        if (index == sh.hoverRow) panel::roundRect(out, r, 6 * s, kWhite, 0.06f);
        if (sh.pad && index == sh.focus)  // the controller's frame
            panel::roundRect(out, r, 7 * s, kGold2, 0.08f, kGold, 0.9f, 1.5f * s);
        if (i + 1 < L.rows.size())
            panel::roundRect(out, panel::Box{r.x + 8 * s, r.y + r.h - 1 * s, r.w - 16 * s, 1 * s}, 0, kWhite, 0.05f);
        if (k.icon && k.icon->width > 0)
            put(out, *k.icon, L.icons[i], k.lock != Lock::None ? 0.3f : k.level > 0 ? 1.0f : 0.45f);
        // 3.0.11-beta.223: a gold hairline over the first closed skill («Ще закриті» below it)
        if (k.lock != Lock::None && (index == 0 || tab->skills[size_t(index) - 1].lock == Lock::None))
            panel::roundRect(out, panel::Box{r.x + 4 * s, r.y, r.w - 8 * s, 1 * s}, 0, kGold2, 0.55f);
        const panel::Box& dots = L.dots[i];
        if (dots.w > 0)
            for (int n = 0; n < k.most; ++n) {
                const float cx = dots.x + (float(n) * kDotStep + kDot / 2) * s, cy = dots.y + dots.h / 2;
                if (n < k.level)
                    panel::circle(out, cx, cy, kDot / 2 * s, kGold, 1.0f);
                else if (n < k.target)  // 3.0.11-beta.225: planned
                    panel::circle(out, cx, cy, kDot / 2 * s, kPlanBlue, 1.0f);
                else
                    panel::circle(out, cx, cy, kDot / 2 * s, kWhite, 0.14f);
            }
        if (L.pluses[i].w > 0)
            panel::roundRect(out, L.pluses[i], 6 * s, kWhite, index == sh.hoverPlus ? 0.22f : 0.12f,
                             sh.plan ? kPlanBlue : kGold2, 0.7f, 1.0f * s);
        if (L.less[i].w > 0) {
            panel::roundRect(out, L.less[i], 5 * s, kWhite, index == sh.hoverLess ? 0.16f : 0.06f);
            panel::roundRect(out, L.more[i], 5 * s, kWhite, index == sh.hoverMore ? 0.16f : 0.06f);
        }
    }
    if (L.scroll.w > 0 && tab && !L.tree) {
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
    const Tab* tab = tabOf(d, L.tab);
    if (L.tree && tab) {  // 3.0.11-beta.224: «Дерево» - the words wholly in its view
        for (size_t i = 0; i < L.rows.size(); ++i) {
            const Skill& k = tab->skills[i];
            const panel::Box& r = L.rows[i];
            const panel::Box name{r.x + 38 * s, r.y + 4 * s, r.w - 44 * s, 17 * s};
            const panel::Box level{r.x + 38 * s, r.y + 21 * s, r.w - 44 * s, 15 * s};
            const uint32_t rgb = k.lock != Lock::None ? kFaint : k.level > 0 ? kText : kMuted;
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
        for (const skilltreeview::Chip& ch : L.plan.chips) {
            const panel::Box b = toWindow(L, ch.box);
            const panel::Box in{b.x + 8 * s, b.y, b.w - 12 * s, b.h};
            if (inView(L, b))
                out.push_back(Words{in, ch.name + " " + std::to_string(ch.level), kMuted, 11 * s, false, Align::Left});
        }
        for (size_t i = 0; i < L.plan.links.size(); ++i) {  // the level each link needs, at its end
            const skilltreeview::Link& l = L.plan.links[i];
            const Elbow e = elbowOf(L, i);
            const panel::Box b = toWindow(L, panel::Box{e.xm + 1 * s, e.y2 - 13 * s, e.x2 - e.xm - 1 * s, 11 * s});
            if (inView(L, b))
                out.push_back(Words{b, std::to_string(l.level), l.met ? kGold2 : kRose, 9.5f * s, true,
                                    Align::Center});
        }
    }
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
    if (L.tree && tab && !t.path.empty()) {  // 3.0.11-beta.224: what the path to a closed skill costs
        const int lit = litIndex(sh);
        if (lit >= 0 && lit < int(tab->skills.size()) && tab->skills[size_t(lit)].level <= 0) {
            const Skill& k = tab->skills[size_t(lit)];
            const int cost = skilltreeview::pathCost(d, k.id);
            if (cost > 0 && k.lock != Lock::Quest && k.lock != Lock::Wedding && k.lock != Lock::SoulLink) {
                const std::string line = fill2(t.path, k.name, std::to_string(cost));
                const float lw = m(line, 12 * s, true) + 4;
                const float lx = side + m(points, 12 * s, true) + 24 * s;
                out.push_back(Words{panel::Box{lx, L.footer.y, lw, L.footer.h}, line, cost > d.points ? kRose : kGold2,
                                    12 * s, true, Align::Left});
                if (!sh.pad) return out;  // in the drag hint's place
            }
        }
    }
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

}  // namespace uagfx::skillwin
