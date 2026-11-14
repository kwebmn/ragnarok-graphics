#include "uikit/stats_live.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "uikit/hits.hpp"

namespace uagfx::uikit::stats {
namespace {

namespace sw = uagfx::statwin;
namespace ly = uagfx::uikit::layered;

std::string markup() {
    std::string s = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw stl" data-style-font-size="base + 'px'" data-style-width="width + 'px'"
  data-style-background-color="ground">
<div class="head" data-hit="title"><span class="title">{{title}}</span><span class="hk">{{hotkey}}</span><div
  class="tab" data-for="t, i : tabs" data-hit="tab" data-attr-data-index="i" data-class-on="t.on"
  data-class-hot="t.hot">{{t.name}}</div><span class="gap"></span><div class="chip" data-hit="chip"
  data-class-on="chipOn">{{chip}}</div></div>
<div class="x" data-hit="close" data-class-hot="closeHot">&#x2715;</div>
)RML";
    s += ly::rml();
    s += "\n</body></rml>";
    return s;
}

}  // namespace

const char* rml() {
    static const std::string s = markup();
    return s.c_str();
}

void bind(Rml::DataModelConstructor& c, Model& m) {
    if (auto e = c.RegisterStruct<TabV>()) {
        e.RegisterMember("name", &TabV::name);
        e.RegisterMember("on", &TabV::on);
        e.RegisterMember("hot", &TabV::hot);
    }
    c.RegisterArray<std::vector<TabV>>();
    c.Bind("base", &m.base);
    c.Bind("width", &m.width);
    c.Bind("ground", &m.ground);
    c.Bind("title", &m.title);
    c.Bind("hotkey", &m.hotkey);
    c.Bind("chip", &m.chip);
    c.Bind("closeHot", &m.closeHot);
    c.Bind("chipOn", &m.chipOn);
    c.Bind("tabs", &m.tabs);
    ly::bind(c, m.layer);
}

void fill(Model& m, const sw::Layout& L, const sw::Data& d, const sw::Show& sh, const sw::Texts& t,
          const sw::Measure& measure) {
    const float sc = L.s > 0 ? L.s : 1;
    constexpr uint32_t kWhite = 0xFFFFFF, kGold = 0xF3C653, kGold2 = 0xC9A55B, kGreen = 0x5FD08A;
    constexpr float kSide = 18, kHeaderH = 30;  // core statwin's
    m.base = 12 * sc;
    m.width = float(L.wide > 0 ? L.wide : L.w);  // 3.0.19-beta.1: its size's, never refit's old one
    m.title = t.title;
    m.hotkey = t.hotkey;
    std::string chip = t.free;
    if (const size_t at = chip.find("{1}"); at != std::string::npos) chip.replace(at, 3, std::to_string(d.free));
    m.chip = chip;
    m.chipOn = d.free > 0;
    m.closeHot = sh.hoverClose;
    m.tabs.resize(sw::kTabs);
    for (int i = 0; i < sw::kTabs; ++i)
        m.tabs[size_t(i)] = TabV{t.tabs[i], i == L.tab, i == sh.hoverTab && i != L.tab};
    const float top = L.title.y + L.title.h;  // the body under the head
    m.layer.w = float(L.w);
    {
        ly::Builder b(m.layer, panel::Box{0, top, 0, 0});
        // the highlights as the sheet drew them (core statwin's paint)
        if (L.tab == sw::kPlanTab) {
            const auto lit = [&](sw::Part part, int i) { return sh.hoverPlan == int(part) && sh.hoverPlanIndex == i; };
            for (int i = 0; i < sw::kStats && i < int(L.planMinus.size()); ++i) {
                b.box(L.planMinus[size_t(i)], 6 * sc, kWhite, lit(sw::Part::PlanMinus, i) ? 0.2f : 0.07f, kWhite, 0.15f,
                      1.0f * sc);
                b.box(L.planPlus[size_t(i)], 6 * sc, kWhite, lit(sw::Part::PlanPlus, i) ? 0.2f : 0.07f, kGreen, 0.6f,
                      1.0f * sc);
                b.hit("planminus", i, L.planMinus[size_t(i)]);
                b.hit("planplus", i, L.planPlus[size_t(i)]);
            }
            for (int i = 0; i < 2; ++i) {
                b.box(i ? L.levelNext : L.levelPrev, 6 * sc, kWhite, lit(sw::Part::PlanLevel, i) ? 0.2f : 0.08f);
                b.hit("level", i, i ? L.levelNext : L.levelPrev);
            }
            b.box(L.planReset, 8 * sc, kWhite, lit(sw::Part::PlanReset, -1) ? 0.16f : 0.07f, kWhite, 0.2f, 1.0f * sc);
            b.hit("reset", -1, L.planReset);
            for (int i = 0; i < sw::kPlanBuffs; ++i) {
                if (L.buffChips[i].w <= 0) continue;
                const bool on = sh.buffOn[i];
                b.box(L.buffChips[i], 12 * sc, on ? kGold : kWhite,
                      lit(sw::Part::PlanBuff, i) ? 0.22f : on ? 0.16f : 0.05f, on ? kGold2 : kWhite, on ? 0.7f : 0.15f,
                      1.0f * sc);
                b.hit("buff", i, L.buffChips[i]);
            }
            for (int i = 0; i < 2; ++i) {
                b.box(i ? L.goalNext : L.goalPrev, 6 * sc, kWhite, lit(sw::Part::Goal, i) ? 0.2f : 0.08f);
                b.hit("goal", i, i ? L.goalNext : L.goalPrev);
            }
            b.box(panel::Box{L.goals.x, L.goals.y - 8 * sc, L.goals.w, 1 * sc}, 0, kWhite, 0.08f);
            b.box(panel::Box{L.mobs.x, L.mobs.y - 6 * sc, L.mobs.w, 1 * sc}, 0, kWhite, 0.08f);
            b.box(panel::Box{L.planResults.x, L.planResults.y + kHeaderH * sc - 1 * sc, L.planResults.w, 1 * sc}, 0,
                  kWhite, 0.1f);
        } else if (L.tab == sw::kRefTab) {
            for (int i = 0; i < 2; ++i) {
                b.box(i ? L.next : L.prev, 6 * sc, kWhite, sh.hoverArrow == i ? 0.2f : 0.08f);
                b.hit("arrow", i, i ? L.next : L.prev);
            }
            for (const panel::Box* x : {&L.aspd, &L.cost, &L.bonus})
                b.box(panel::Box{x->x, x->y + 26 * sc - 1 * sc, x->w, 1 * sc}, 0, kWhite, 0.1f);
        } else {
            b.box(panel::Box{L.header.x, L.header.y + L.header.h - 1 * sc, L.header.w, 1 * sc}, 0, kWhite, 0.08f);
            const int lit = sh.hoverRow >= 0 ? sh.hoverRow : sh.hoverPlus >= 0 ? sh.hoverPlus : sh.selected;
            for (int i = 0; i < sw::kStats && i < int(L.rows.size()); ++i) {
                const panel::Box& r = L.rows[size_t(i)];
                if (i == lit) b.box(r, 6 * sc, kWhite, 0.06f);
                if (i < sw::kStats - 1) b.box(panel::Box{r.x, r.y + r.h - 1 * sc, r.w, 1 * sc}, 0, kWhite, 0.05f);
                b.hit("row", i, r);
                if (d.base[i] >= 99) continue;
                const bool can = sw::canRaise(d, i);
                b.box(L.pluses[size_t(i)], 6 * sc, kWhite, can ? (i == sh.hoverPlus ? 0.22f : 0.12f) : 0.03f,
                      can ? kGold2 : kWhite, can ? 0.7f : 0.06f, 1.0f * sc);
                b.hit("plus", i, L.pluses[size_t(i)]);
            }
            const int lit2 = sh.hoverBattle >= 0 ? sh.hoverBattle : sh.pinnedBattle;
            if (lit2 >= 0 && lit2 < int(L.battleRows.size()))
                b.box(L.battleRows[size_t(lit2)], 6 * sc, kWhite, sh.pinnedBattle == lit2 ? 0.09f : 0.06f);
            for (size_t i = 0; i < L.battleRows.size(); ++i) b.hit("battle", int(i), L.battleRows[i]);
            b.box(panel::Box{kSide * sc / 2, L.line.y + 4 * sc, L.glass.w - kSide * sc, L.line.h - 10 * sc}, 8 * sc,
                  kWhite, 0.04f, kWhite, 0.08f, 1.0f * sc);
        }
        for (const sw::Words& x : sw::words(L, d, sh, t, measure)) {  // the body's words (the head's are its own)
            if (x.box.y + x.box.h / 2 < top) continue;
            b.word(x.box, x.text, x.rgb, x.px, x.bold,
                   x.align == sw::Align::Right    ? ly::Align::Right
                   : x.align == sw::Align::Center ? ly::Align::Center
                                                  : ly::Align::Left);
        }
    }
    m.layer.h = ly::contentHeight(m.layer, 10 * sc);  // 3.0.18-beta.104: from what it holds
}

bool refit(Rml::ElementDocument* doc, sw::Layout& L) {
    if (!doc || !doc->IsVisible()) return false;
    const Rml::Vector2f size = doc->GetBox().GetSize(Rml::BoxArea::Border);
    const std::vector<Placed> tabs = placesOf(doc, "tab");
    if (size.x <= 0 || size.y <= 0 || tabs.size() != size_t(sw::kTabs)) return false;
    float dx = 0, dy = 0;
    if (!layered::bodyShift(doc, panel::Box{0, L.title.y + L.title.h, 0, 0}, dx, dy)) return false;
    const auto shift = [&](panel::Box& b) {
        if (b.w > 0) b.x += dx, b.y += dy;
    };
    for (panel::Box* b : {&L.table, &L.header, &L.battle, &L.line, &L.classRow, &L.prev, &L.next, &L.aspd, &L.cost,
                          &L.bonus, &L.planLevelRow, &L.levelPrev, &L.levelNext, &L.planResults, &L.planReset,
                          &L.goals, &L.goalPrev, &L.goalNext, &L.mobs, &L.buffsRow})
        shift(*b);
    for (panel::Box& b : L.buffChips) shift(b);
    for (auto* list : {&L.rows, &L.pluses, &L.battleRows, &L.planRowsBoxes, &L.planMinus, &L.planPlus})
        for (panel::Box& b : *list) shift(b);
    const std::vector<Placed> close = placesOf(doc, "close"), chip = placesOf(doc, "chip"),
                              head = placesOf(doc, "title");
    if (!close.empty()) L.close = close.front().box;
    if (!chip.empty()) L.freeChip = chip.front().box;
    for (int i = 0; i < sw::kTabs; ++i) L.tabs[i] = tabs[size_t(i)].box;
    if (!head.empty()) L.title = panel::Box{0, 0, size.x, head.front().box.y + head.front().box.h};
    L.glass = panel::Box{0, 0, size.x, size.y};
    L.w = int(std::ceil(size.x));
    L.h = int(std::ceil(size.y));
    return true;
}

}  // namespace uagfx::uikit::stats
