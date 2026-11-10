#include "uikit/skills_live.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/pad_glyphs.hpp"
#include "core/skill_plan.hpp"
#include "uikit/hits.hpp"

namespace uagfx::uikit::skills {
namespace {

namespace kw = uagfx::skillwin;

const char* kRml = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw skl" data-class-treeon="tree" data-style-background-color="ground">
<div class="head" data-hit="title"><span class="title">{{title}}</span><span class="hk">{{hotkey}}</span>
<span class="gap"></span><div class="hbt plan" data-if="planButton" data-hit="planbtn" data-class-on="plan"
  data-class-hot="planHot">{{planWord}}</div><div class="hbt v l" data-if="views" data-hit="viewlist"
  data-class-on="!tree" data-class-hot="listHot">{{listWord}}</div><div class="hbt v r" data-if="views"
  data-hit="viewtree" data-class-on="tree" data-class-hot="treeHot">{{treeWord}}</div></div>
<div class="x" data-hit="close" data-class-hot="closeHot">&#x2715;</div>
<div class="tabs"><div class="tab" data-for="t, i : tabs" data-hit="tab" data-attr-data-index="i" data-class-on="t.on"
  data-class-hot="t.hot">{{t.name}}</div></div>
<div class="notice" data-if="notice != ''" data-hit="notice">{{notice}}</div>
<div class="choices" data-if="choices.size > 1"><div class="choice" data-for="c, i : choices" data-hit="choice"
  data-attr-data-index="i" data-class-on="c.on" data-class-hot="c.hot">{{c.name}}</div></div>
<div class="body"><div class="area" data-hit="list">
<div class="rows" data-if="!tree" data-class-scrolls="scrollable"><div class="row" data-for="r : rows"
  data-hit="row" data-attr-data-index="r.index" data-class-unused="!r.used" data-class-hot="r.hot"
  data-class-focus="r.focus" data-class-locked="r.locked" data-class-locktop="r.lockTop" data-class-last="r.last"><div
  class="ic" data-hit="icon" data-attr-data-index="r.index"><img data-if="r.icon != ''" data-attr-src="r.icon"
  data-style-width="r.iconW + 'px'" data-style-height="r.iconH + 'px'" data-style-opacity="r.op"/></div><div
  class="tx"><div data-attr-class="r.nameCls">{{r.name}}</div><div class="inf">{{r.info}}</div></div><div
  class="use" data-if="r.arrows"><span class="ar" data-hit="less" data-attr-data-index="r.index"
  data-class-on="r.lessOn" data-class-hot="r.lessHot">&#x2039;</span><span class="un">{{r.use}}</span><span
  class="ar" data-hit="more" data-attr-data-index="r.index" data-class-on="r.moreOn"
  data-class-hot="r.moreHot">&#x203A;</span></div><span class="minus" data-if="r.minus" data-hit="minus"
  data-attr-data-index="r.index" data-class-hot="r.minusHot">&#x2212;</span><div class="dots" data-hit="dots"
  data-attr-data-index="r.index"><span class="dot" data-for="d : r.dots" data-class-lv="d == 2"
  data-class-plan="d == 1"></span></div><span class="lvl" data-class-planned="r.planned"
  data-class-learned="r.learned">{{r.level}}</span><div class="pl"><span class="plus" data-if="r.plus"
  data-hit="plus" data-attr-data-index="r.index" data-class-plan="plan"
  data-class-hot="r.plusHot">+</span></div></div></div>
<div class="thumb" data-if="!tree && scrollable" data-hit="scroll"><div class="th" data-style-top="thumbTop + '%'"
  data-style-height="thumbH + '%'"></div></div>
<div class="treebox" data-if="tree"><div class="view" data-hit="view" data-style-width="viewW + 'px'"
  data-style-height="viewH + 'px'"><div class="ln" data-for="b : links" data-style-left="b.x + 'px'"
  data-style-top="b.y + 'px'" data-style-width="b.w + 'px'" data-style-height="b.h + 'px'"
  data-style-background-color="b.fill"></div><div class="nd" data-for="b : nodes" data-style-left="b.x + 'px'"
  data-style-top="b.y + 'px'" data-style-width="b.w + 'px'" data-style-height="b.h + 'px'"
  data-style-border-radius="b.r + 'px'" data-style-border-width="b.e + 'px'" data-style-border-color="b.edge"
  data-style-background-color="b.fill"></div><div class="ni" data-for="i : nodeIcons"><img
  data-if="i.src != ''" data-attr-src="i.src" data-style-left="i.x + 'px'" data-style-top="i.y + 'px'"
  data-style-width="i.w + 'px'" data-style-height="i.h + 'px'" data-style-opacity="i.op"/></div><span
  data-for="x : treeWords" data-attr-class="x.cls" data-style-left="x.x + 'px'" data-style-top="x.y + 'px'"
  data-style-width="x.w + 'px'" data-style-height="x.h + 'px'" data-style-line-height="x.h + 'px'"
  data-style-font-size="x.px + 'px'" data-style-color="x.colour">{{x.text}}</span></div><div class="bar"
  data-for="b : bars" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'" data-style-width="b.w + 'px'"
  data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'" data-style-background-color="b.fill"></div>
</div></div>
<div class="side" data-hit="side"><span data-for="x : sideWords" data-attr-class="x.cls" data-style-left="x.x + 'px'"
  data-style-top="x.y + 'px'" data-style-width="x.w + 'px'" data-style-height="x.h + 'px'"
  data-style-line-height="x.h + 'px'" data-style-font-size="x.px + 'px'"
  data-style-color="x.colour">{{x.text}}</span></div></div>
<div class="foot" data-hit="footer"><span data-attr-class="pointsCls">{{points}}</span><span class="drag"
  data-if="drag != ''">{{drag}}</span><div class="pads" data-if="pad && !plan"><div class="pad"
  data-for="p : padItems"><img data-attr-src="p.glyph" data-style-width="p.w + 'px'"
  data-style-height="p.h + 'px'"/><span>{{p.words}}</span></div></div><div class="pbtns" data-if="plan"><div
  class="fb" data-hit="reset" data-class-hot="resetHot">{{resetWord}}</div><div class="fb go" data-hit="apply"
  data-class-on="applyOn" data-class-hot="applyHot">{{applyWord}}</div></div></div>
</body></rml>)RML";

std::string hexa(uint32_t rgb, float alpha) {
    char b[16];
    std::snprintf(b, sizeof b, "#%06x%02x", unsigned(rgb & 0xFFFFFF),
                  unsigned(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255)) & 0xFF);
    return b;
}

const kw::Tab* tabOf(const kw::Data& d, int tab) {
    return d.tabs.empty() ? nullptr : &d.tabs[size_t(std::clamp(tab, 0, int(d.tabs.size()) - 1))];
}

bool inside(const panel::Box& outer, const panel::Box& b) {
    return outer.w > 0 && b.x >= outer.x - 0.5f && b.y >= outer.y - 0.5f && b.x + b.w <= outer.x + outer.w + 0.5f &&
           b.y + b.h <= outer.y + outer.h + 0.5f;
}

WordV wordOf(const kw::Words& x, const panel::Box& origin) {
    const char* align = x.align == kw::Align::Right ? " right" : x.align == kw::Align::Center ? " centre" : "";
    return WordV{x.box.x - origin.x, x.box.y - origin.y, x.box.w, x.box.h, x.px, x.text, hexa(x.rgb, 1),
                 std::string(x.bold ? "w bold" : "w") + align};
}

template <class T>
void keepLength(std::vector<T>& v, size_t had) {  // RmlUi's lists never shrink under it: the rest unseen
    if (v.size() < had) v.resize(had);
}

}  // namespace

const char* rml() { return kRml; }

void bind(Rml::DataModelConstructor& c, Model& m) {
    c.RegisterArray<std::vector<int>>();
    if (auto e = c.RegisterStruct<WordV>()) {
        e.RegisterMember("x", &WordV::x);
        e.RegisterMember("y", &WordV::y);
        e.RegisterMember("w", &WordV::w);
        e.RegisterMember("h", &WordV::h);
        e.RegisterMember("px", &WordV::px);
        e.RegisterMember("text", &WordV::text);
        e.RegisterMember("colour", &WordV::colour);
        e.RegisterMember("cls", &WordV::cls);
    }
    if (auto e = c.RegisterStruct<BoxV>()) {
        e.RegisterMember("x", &BoxV::x);
        e.RegisterMember("y", &BoxV::y);
        e.RegisterMember("w", &BoxV::w);
        e.RegisterMember("h", &BoxV::h);
        e.RegisterMember("r", &BoxV::r);
        e.RegisterMember("e", &BoxV::e);
        e.RegisterMember("fill", &BoxV::fill);
        e.RegisterMember("edge", &BoxV::edge);
    }
    if (auto e = c.RegisterStruct<IconV>()) {
        e.RegisterMember("x", &IconV::x);
        e.RegisterMember("y", &IconV::y);
        e.RegisterMember("w", &IconV::w);
        e.RegisterMember("h", &IconV::h);
        e.RegisterMember("op", &IconV::op);
        e.RegisterMember("src", &IconV::src);
    }
    if (auto e = c.RegisterStruct<TabV>()) {
        e.RegisterMember("name", &TabV::name);
        e.RegisterMember("on", &TabV::on);
        e.RegisterMember("hot", &TabV::hot);
    }
    if (auto e = c.RegisterStruct<RowV>()) {
        e.RegisterMember("index", &RowV::index);
        e.RegisterMember("used", &RowV::used);
        e.RegisterMember("hot", &RowV::hot);
        e.RegisterMember("focus", &RowV::focus);
        e.RegisterMember("locked", &RowV::locked);
        e.RegisterMember("lockTop", &RowV::lockTop);
        e.RegisterMember("last", &RowV::last);
        e.RegisterMember("icon", &RowV::icon);
        e.RegisterMember("name", &RowV::name);
        e.RegisterMember("info", &RowV::info);
        e.RegisterMember("nameCls", &RowV::nameCls);
        e.RegisterMember("use", &RowV::use);
        e.RegisterMember("level", &RowV::level);
        e.RegisterMember("op", &RowV::op);
        e.RegisterMember("iconW", &RowV::iconW);
        e.RegisterMember("iconH", &RowV::iconH);
        e.RegisterMember("arrows", &RowV::arrows);
        e.RegisterMember("lessOn", &RowV::lessOn);
        e.RegisterMember("moreOn", &RowV::moreOn);
        e.RegisterMember("minus", &RowV::minus);
        e.RegisterMember("minusHot", &RowV::minusHot);
        e.RegisterMember("plus", &RowV::plus);
        e.RegisterMember("plusHot", &RowV::plusHot);
        e.RegisterMember("lessHot", &RowV::lessHot);
        e.RegisterMember("moreHot", &RowV::moreHot);
        e.RegisterMember("planned", &RowV::planned);
        e.RegisterMember("learned", &RowV::learned);
        e.RegisterMember("dots", &RowV::dots);
    }
    if (auto e = c.RegisterStruct<PadV>()) {
        e.RegisterMember("glyph", &PadV::glyph);
        e.RegisterMember("words", &PadV::words);
        e.RegisterMember("w", &PadV::w);
        e.RegisterMember("h", &PadV::h);
    }
    c.RegisterArray<std::vector<WordV>>();
    c.RegisterArray<std::vector<BoxV>>();
    c.RegisterArray<std::vector<IconV>>();
    c.RegisterArray<std::vector<TabV>>();
    c.RegisterArray<std::vector<RowV>>();
    c.RegisterArray<std::vector<PadV>>();
    c.Bind("ground", &m.ground);
    c.Bind("title", &m.title);
    c.Bind("hotkey", &m.hotkey);
    c.Bind("planWord", &m.planWord);
    c.Bind("listWord", &m.listWord);
    c.Bind("treeWord", &m.treeWord);
    c.Bind("notice", &m.notice);
    c.Bind("points", &m.points);
    c.Bind("pointsCls", &m.pointsCls);
    c.Bind("drag", &m.drag);
    c.Bind("applyWord", &m.applyWord);
    c.Bind("resetWord", &m.resetWord);
    c.Bind("closeHot", &m.closeHot);
    c.Bind("planButton", &m.planButton);
    c.Bind("plan", &m.plan);
    c.Bind("planHot", &m.planHot);
    c.Bind("views", &m.views);
    c.Bind("tree", &m.tree);
    c.Bind("listHot", &m.listHot);
    c.Bind("treeHot", &m.treeHot);
    c.Bind("pad", &m.pad);
    c.Bind("applyOn", &m.applyOn);
    c.Bind("applyHot", &m.applyHot);
    c.Bind("resetHot", &m.resetHot);
    c.Bind("scrollable", &m.scrollable);
    c.Bind("thumbTop", &m.thumbTop);
    c.Bind("thumbH", &m.thumbH);
    c.Bind("viewW", &m.viewW);
    c.Bind("viewH", &m.viewH);
    c.Bind("tabs", &m.tabs);
    c.Bind("choices", &m.choices);
    c.Bind("rows", &m.rows);
    c.Bind("links", &m.links);
    c.Bind("nodes", &m.nodes);
    c.Bind("bars", &m.bars);
    c.Bind("nodeIcons", &m.nodeIcons);
    c.Bind("treeWords", &m.treeWords);
    c.Bind("sideWords", &m.sideWords);
    c.Bind("padItems", &m.padItems);
}

void fill(Model& m, const kw::Layout& L, const kw::Data& d, const kw::Show& sh, const kw::Texts& t,
          const kw::Measure& measure, const Source& src, const Glyph& glyph) {
    const float s = L.s > 0 ? L.s : 1;
    m.title = t.title;
    m.hotkey = t.hotkey;
    m.closeHot = sh.hoverClose;
    m.views = !t.list.empty() && !t.tree.empty();
    m.planButton = m.views && !t.plan.empty();
    m.plan = sh.plan;
    m.planHot = sh.hoverPlan;
    m.planWord = t.plan;
    m.listWord = t.list;
    m.treeWord = t.tree;
    m.tree = sh.tree;
    m.listHot = sh.hoverList;
    m.treeHot = sh.hoverTree;
    const size_t hadTabs = m.tabs.size(), hadChoices = m.choices.size();
    m.tabs.clear();
    for (size_t i = 0; i < d.tabs.size(); ++i)
        m.tabs.push_back(TabV{d.tabs[i].name, int(i) == L.tab, int(i) == sh.hoverTab && int(i) != L.tab});
    keepLength(m.tabs, hadTabs);
    m.notice = d.notice;
    const kw::Tab* tab = tabOf(d, L.tab);
    m.choices.clear();
    if (tab && tab->choices.size() > 1)
        for (size_t i = 0; i < tab->choices.size(); ++i)
            m.choices.push_back(TabV{tab->choices[i], int(i) == tab->chosen, int(i) == sh.hoverChoice});
    keepLength(m.choices, hadChoices);
    const int count = tab ? int(tab->skills.size()) : 0;
    m.scrollable = !sh.tree && count > kw::kRows;
    if (m.scrollable) {
        m.thumbH = std::max(100.0f * float(kw::kRows) / float(count), 8.0f);
        m.thumbTop = (100 - m.thumbH) * float(L.first) / float(std::max(1, count - kw::kRows));
    }
    m.rows.assign(size_t(kw::kRows), RowV{});
    for (int i = 0; i < kw::kRows; ++i) {
        RowV& r = m.rows[size_t(i)];
        const int index = L.first + i;
        r.index = index;
        r.last = i + 1 == kw::kRows || index + 1 >= count;
        if (sh.tree || !tab || index >= count) continue;
        const kw::Skill& k = tab->skills[size_t(index)];
        r.used = true;
        r.hot = index == sh.hoverRow;
        r.focus = sh.pad && index == sh.focus;
        r.locked = k.lock != kw::Lock::None;
        r.lockTop = r.locked && (index == 0 || tab->skills[size_t(index) - 1].lock == kw::Lock::None);
        r.name = k.name;
        r.nameCls = r.locked ? "nm lock" : k.level > 0 ? "nm" : "nm un";
        r.info = kw::info(k, t);
        if (k.icon && k.icon->width > 0) {
            r.icon = src(k.icon);
            r.iconW = float(k.icon->width);
            r.iconH = float(k.icon->height);
            r.op = r.locked ? 0.3f : k.level > 0 ? 1.0f : 0.45f;
        }
        r.arrows = kw::usable(k) && k.level > 1 && !sh.plan;
        if (r.arrows) {
            const int use = std::clamp(k.use, 1, k.level);
            r.use = std::to_string(use);
            r.lessOn = k.use > 1;
            r.moreOn = k.use < k.level;
            r.lessHot = index == sh.hoverLess;
            r.moreHot = index == sh.hoverMore;
        }
        r.minus = sh.plan && skillplan::extra(k) > 0;
        r.minusHot = index == sh.hoverMinus;
        if (k.most > 0 && k.most <= 10)
            for (int n = 0; n < k.most; ++n) r.dots.push_back(n < k.level ? 2 : n < k.target ? 1 : 0);
        r.planned = skillplan::extra(k) > 0;
        r.learned = k.level > 0;
        const int shownLv = r.planned ? k.target : k.level;
        r.level = k.most > 0 ? std::to_string(shownLv) + "/" + std::to_string(k.most) : std::to_string(shownLv);
        r.plus = kw::plusOn(d, k, sh.plan);
        r.plusHot = index == sh.hoverPlus;
    }
    // the tree (a diagram: core skillwin's places, against the view) and the side panel's lines (against the panel)
    const size_t hadLinks = m.links.size(), hadNodes = m.nodes.size(), hadIcons = m.nodeIcons.size(),
                 hadTreeWords = m.treeWords.size(), hadSide = m.sideWords.size(), hadBars = m.bars.size();
    m.links.clear(), m.nodes.clear(), m.nodeIcons.clear(), m.treeWords.clear(), m.sideWords.clear(), m.bars.clear();
    m.viewW = L.view.w;
    m.viewH = L.view.h;
    const std::vector<kw::Words> all = kw::words(L, d, sh, t, measure);
    if (sh.tree && tab) {
        const panel::Box& o = L.view;
        const kw::TreeView v = kw::treeView(L, d, sh);
        for (const kw::TreeLink& l : v.links)
            m.links.push_back(BoxV{l.box.x - o.x, l.box.y - o.y, l.box.w, l.box.h, 0, 0, hexa(l.rgb, l.alpha)});
        for (const kw::TreeNode& n : v.nodes) {
            m.nodes.push_back(BoxV{n.box.x - o.x, n.box.y - o.y, n.box.w, n.box.h, 8 * s, n.edgeW,
                                   hexa(n.fill, n.fillA), hexa(n.edge, n.edgeA)});
            if (n.picture && n.picture->width > 0)
                m.nodeIcons.push_back(IconV{std::round(n.icon.x - o.x + (n.icon.w - float(n.picture->width)) / 2),
                                            std::round(n.icon.y - o.y + (n.icon.h - float(n.picture->height)) / 2),
                                            float(n.picture->width), float(n.picture->height), n.iconOpacity,
                                            src(n.picture)});
        }
        for (const kw::TreeNode& n : v.nodes) {  // the pluses and minuses over the nodes
            if (n.plus.w > 0)
                m.nodes.push_back(BoxV{n.plus.x - o.x, n.plus.y - o.y, n.plus.w, n.plus.h, 5 * s, 0,
                                       hexa(n.plusRgb, n.plusA)});
            if (n.minus.w > 0)
                m.nodes.push_back(BoxV{n.minus.x - o.x, n.minus.y - o.y, n.minus.w, n.minus.h, 5 * s, 1 * s,
                                       hexa(0x10141C, 1), hexa(0x6EA3FF, n.minusA)});
        }
        for (const kw::Words& x : all)
            if (inside(o, x.box)) m.treeWords.push_back(wordOf(x, o));
        const auto bar = [&](const panel::Box& track, const panel::Box& thumb) {
            m.bars.push_back(BoxV{track.x - o.x, track.y - o.y, track.w, track.h, std::min(track.w, track.h) / 2, 0,
                                  hexa(0xFFFFFF, 0.05f)});
            m.bars.push_back(BoxV{thumb.x - o.x, thumb.y - o.y, thumb.w, thumb.h, std::min(thumb.w, thumb.h) / 2, 0,
                                  hexa(0xFFFFFF, 0.28f)});
        };
        if (L.scroll.w > 0) {
            const float h = std::max(L.scroll.h * L.view.h / L.plan.h, 16 * s);
            const float y = L.scroll.y + (L.scroll.h - h) * (L.maxY > 0 ? L.scrollY / L.maxY : 0);
            bar(L.scroll, panel::Box{L.scroll.x, y, L.scroll.w, h});
        }
        if (L.scrollH.w > 0) {
            const float w = std::max(L.scrollH.w * L.view.w / L.plan.w, 16 * s);
            const float x = L.scrollH.x + (L.scrollH.w - w) * (L.maxX > 0 ? L.scrollX / L.maxX : 0);
            bar(L.scrollH, panel::Box{x, L.scrollH.y, w, L.scrollH.h});
        }
    }
    for (const kw::Words& x : all)
        if (inside(L.side, x.box)) m.sideWords.push_back(wordOf(x, L.side));
    keepLength(m.links, hadLinks);
    keepLength(m.nodes, hadNodes);
    keepLength(m.nodeIcons, hadIcons);
    keepLength(m.treeWords, hadTreeWords);
    keepLength(m.sideWords, hadSide);
    keepLength(m.bars, hadBars);
    const int planCost = skillplan::cost(d);  // the points the plan leaves, and what it takes
    std::string points = t.points;
    const std::string n = std::to_string(planCost > 0 ? skillplan::left(d) : d.points);
    if (const size_t at = points.find("{1}"); at != std::string::npos) points.replace(at, 3, n);
    if (planCost > 0 && !t.planned.empty()) {
        std::string p = t.planned;
        if (const size_t at = p.find("{1}"); at != std::string::npos) p.replace(at, 3, std::to_string(planCost));
        points += " \xC2\xB7 " + p;
    }
    m.points = points;
    m.pointsCls = planCost > 0 ? "pts plan" : d.points > 0 ? "pts gold" : "pts";
    m.pad = sh.pad;
    m.drag = !sh.plan && !sh.pad ? t.drag : "";
    m.applyWord = sh.applying ? t.applying : t.apply;
    m.resetWord = t.reset;
    m.applyOn = planCost > 0 && !sh.applying;
    m.applyHot = sh.hoverApply;
    m.resetHot = sh.hoverReset;
    m.padItems.clear();
    if (sh.pad && !sh.plan) {
        using B = padglyph::Button;
        const std::pair<B, const std::string*> items[4] = {{B::A, &t.use}, {B::Y, &t.raise}, {B::X, &t.assign},
                                                           {B::B, &t.close}};
        const float h = std::round(17 * s);
        for (const auto& [b, words] : items) {
            PadV v;
            v.h = h;
            v.glyph = glyph(int(b), h, v.w);
            v.words = *words;
            m.padItems.push_back(v);
        }
    }
}

bool refit(Rml::ElementDocument* doc, kw::Layout& L) {
    if (!doc || !doc->IsVisible()) return false;
    const Rml::Vector2f size = doc->GetBox().GetSize(Rml::BoxArea::Border);
    if (size.x <= 0 || size.y <= 0) return false;
    const std::vector<Placed> sides = placesOf(doc, "side");
    if (sides.empty()) return false;
    const auto one = [&](const char* part, panel::Box& to) {
        const std::vector<Placed> p = placesOf(doc, part);
        to = p.empty() ? panel::Box{} : p.front().box;
    };
    const auto byIndex = [&](const char* part, std::vector<panel::Box>& to) {
        for (panel::Box& b : to) b = panel::Box{};
        for (const Placed& p : placesOf(doc, part)) {
            const int i = p.index - L.first;
            if (i >= 0 && i < int(to.size())) to[size_t(i)] = p.box;
        }
    };
    L.glass = panel::Box{0, 0, size.x, size.y};
    L.w = int(std::ceil(size.x));
    L.h = int(std::ceil(size.y));
    one("title", L.title);
    L.title = panel::Box{0, 0, size.x, L.title.y + L.title.h};
    one("close", L.close);
    one("planbtn", L.planButton);
    one("viewlist", L.viewList);
    one("viewtree", L.viewTree);
    one("notice", L.notice);
    one("footer", L.footer);
    one("apply", L.apply);
    one("reset", L.reset);
    L.side = sides.front().box;
    L.tabs.clear();
    for (const Placed& p : placesOf(doc, "tab")) L.tabs.push_back(p.box);
    L.choices.clear();
    for (const Placed& p : placesOf(doc, "choice")) L.choices.push_back(p.box);
    if (L.tree) {  // the diagram moved as its view moved
        panel::Box view;
        one("view", view);
        if (view.w <= 0) return true;
        const float dx = view.x - L.view.x, dy = view.y - L.view.y;
        const auto shift = [&](panel::Box& b) {
            if (b.w > 0) b.x += dx, b.y += dy;
        };
        for (auto* list : {&L.rows, &L.icons, &L.pluses, &L.minuses})
            for (panel::Box& b : *list) shift(b);
        shift(L.scroll);
        shift(L.scrollH);
        shift(L.list);
        L.view = view;
        return true;
    }
    one("list", L.list);
    one("scroll", L.scroll);
    const size_t rows = L.rows.size();
    std::vector<panel::Box> got(rows);
    byIndex("row", got);
    L.rows = got;
    byIndex("icon", L.icons);
    byIndex("plus", L.pluses);
    byIndex("minus", L.minuses);
    byIndex("less", L.less);
    byIndex("more", L.more);
    byIndex("dots", L.dots);
    return true;
}

}  // namespace uagfx::uikit::skills
