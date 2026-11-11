#include "uikit/item_grid_live.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "uikit/hits.hpp"

namespace uagfx::uikit::itemgrid {
namespace {

namespace iw = uagfx::itemwin;

const char* kRml = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw igl" data-style-font-size="base + 'px'" data-style-background-color="ground">
<div class="head" data-hit="title"><span class="title">{{title}}</span><span class="hk">{{hotkey}}</span></div>
<div class="x" data-hit="close" data-class-hot="closeHot">&#x2715;</div>
<div class="tabs" data-hit="tabsrow"><div class="tab" data-for="t, i : tabs" data-hit="tab" data-attr-data-index="i"
  data-class-on="t.on" data-class-hot="t.hot">{{t.name}}<span class="new" data-if="t.fresh != ''">{{t.fresh}}</span>
</div></div>
<div class="search" data-hit="search" data-class-typing="typing"><img data-if="searchIcon != ''"
  data-attr-src="searchIcon"/><span class="q" data-if="query != ''">{{query}}</span><span class="caret"
  data-if="caret"></span><span class="q qh" data-if="hint != ''">{{hint}}</span></div>
<div class="gridbox" data-hit="grid"><div class="grid"><div class="cell" data-for="c : cells" data-hit="cellbox"
  data-attr-data-index="c.index" data-class-hot="c.hot" data-class-thing="c.hit == 'cell'"><div class="flash"
  data-if="c.flash > 0" data-style-opacity="c.flash"></div><img data-if="c.icon != ''" data-attr-src="c.icon"
  data-style-width="c.iconW + 'px'" data-style-height="c.iconH + 'px'" data-style-opacity="c.op"/><span class="cnt"
  data-if="c.count != ''">{{c.count}}</span><span class="ref" data-if="c.refine != ''">{{c.refine}}</span><span
  data-if="c.mark != ''" data-attr-class="c.markCls">{{c.mark}}</span><span class="fresh"
  data-if="c.fresh != ''">{{c.fresh}}</span><span class="ele" data-if="c.ele != '#00000000'"
  data-style-background-color="c.ele"></span><span class="star" data-if="c.star != ''"
  data-class-on="c.starOn">{{c.star}}</span><div class="ring" data-if="c.ring > 0"
  data-style-opacity="c.ring"></div><span class="rise" data-if="c.rising != ''" data-style-color="c.risingColour"
  data-style-margin-top="((-4 - c.rise) / 12) + 'rem'">{{c.rising}}</span></div></div>
<div class="thumb" data-if="scrollable" data-style-top="thumbTop + '%'" data-style-height="thumbH + '%'"></div>
<div class="drop" data-if="dropping"></div></div>
<div class="foot" data-hit="footer"><span class="also" data-if="also != ''">{{also}}</span><span class="w"
  data-if="also == ''">{{weightWord}}</span><div class="bar" data-hit="bar" data-if="bar"><div
  data-attr-class="'fill ' + tone" data-style-width="fill + '%'"></div></div><span class="wt"
  data-if="weightText != '' && also == ''">{{weightText}}</span><span class="z"
  data-if="zeny != '' && also == ''">{{zeny}}</span></div>
<div class="tip" data-hit="tip" data-if="tip" data-style-left="tipX + 'px'" data-style-top="tipY + 'px'"><div
  class="t1" data-style-color="tipColour">{{tip1}}</div><div class="t2">{{tip2}}</div><div class="t3"
  data-if="tip3 != ''">{{tip3}}</div><div
  class="t4" data-if="tip4 != ''">{{tip4}}</div></div>
</body></rml>)RML";

constexpr const char* kTipName = "#eef2f8";  // 3.0.18-beta.144: the tip's name without an element

uint32_t toward(uint32_t from, uint32_t to, float t) {
    uint32_t out = 0;
    for (int sh = 0; sh <= 16; sh += 8) {
        const float a = float((from >> sh) & 0xFF), b = float((to >> sh) & 0xFF);
        out |= uint32_t(a + (b - a) * t * t) << sh;
    }
    return out;
}

std::string hex(uint32_t rgb) {
    char b[16];
    std::snprintf(b, sizeof b, "#%06x", unsigned(rgb & 0xFFFFFF));
    return b;
}

}  // namespace

const char* rml() { return kRml; }

void bind(Rml::DataModelConstructor& c, Model& m) {
    if (auto t = c.RegisterStruct<TabV>()) {
        t.RegisterMember("name", &TabV::name);
        t.RegisterMember("fresh", &TabV::fresh);
        t.RegisterMember("on", &TabV::on);
        t.RegisterMember("hot", &TabV::hot);
    }
    if (auto e = c.RegisterStruct<CellV>()) {
        e.RegisterMember("index", &CellV::index);
        e.RegisterMember("hit", &CellV::hit);
        e.RegisterMember("icon", &CellV::icon);
        e.RegisterMember("count", &CellV::count);
        e.RegisterMember("refine", &CellV::refine);
        e.RegisterMember("mark", &CellV::mark);
        e.RegisterMember("markCls", &CellV::markCls);
        e.RegisterMember("fresh", &CellV::fresh);
        e.RegisterMember("star", &CellV::star);
        e.RegisterMember("rising", &CellV::rising);
        e.RegisterMember("risingColour", &CellV::risingColour);
        e.RegisterMember("ele", &CellV::ele);
        e.RegisterMember("hot", &CellV::hot);
        e.RegisterMember("starOn", &CellV::starOn);
        e.RegisterMember("op", &CellV::op);
        e.RegisterMember("iconW", &CellV::iconW);
        e.RegisterMember("iconH", &CellV::iconH);
        e.RegisterMember("flash", &CellV::flash);
        e.RegisterMember("ring", &CellV::ring);
        e.RegisterMember("rise", &CellV::rise);
    }
    c.RegisterArray<std::vector<TabV>>();
    c.RegisterArray<std::vector<CellV>>();
    c.Bind("ground", &m.ground);
    c.Bind("base", &m.base);
    c.Bind("title", &m.title);
    c.Bind("hotkey", &m.hotkey);
    c.Bind("query", &m.query);
    c.Bind("hint", &m.hint);
    c.Bind("searchIcon", &m.searchIcon);
    c.Bind("also", &m.also);
    c.Bind("weightWord", &m.weightWord);
    c.Bind("weightText", &m.weightText);
    c.Bind("zeny", &m.zeny);
    c.Bind("tone", &m.tone);
    c.Bind("typing", &m.typing);
    c.Bind("caret", &m.caret);
    c.Bind("bar", &m.bar);
    c.Bind("dropping", &m.dropping);
    c.Bind("scrollable", &m.scrollable);
    c.Bind("tip", &m.tip);
    c.Bind("closeHot", &m.closeHot);
    c.Bind("fill", &m.fill);
    c.Bind("thumbTop", &m.thumbTop);
    c.Bind("thumbH", &m.thumbH);
    c.Bind("tipX", &m.tipX);
    c.Bind("tipY", &m.tipY);
    c.Bind("tip1", &m.tip1);
    c.Bind("tipColour", &m.tipColour);
    c.Bind("tip2", &m.tip2);
    c.Bind("tip3", &m.tip3);
    c.Bind("tip4", &m.tip4);
    c.Bind("tabs", &m.tabs);
    c.Bind("cells", &m.cells);
}

void fill(Model& m, const iw::Layout& L, const std::vector<iw::Item>& items, const std::vector<int>& shown,
          const iw::Show& sh, const iw::Texts& t, const Source& src) {
    m.base = 12 * (L.s > 0 ? L.s : 1);
    m.title = t.title;
    m.hotkey = t.hotkey;
    int fresh[iw::kTabs] = {};
    for (const iw::Item& it : items)
        if (it.fresh) ++fresh[int(iw::tabOf(it.type))];
    m.tabs.resize(iw::kTabs);
    for (int i = 0; i < iw::kTabs; ++i) {
        TabV& v = m.tabs[size_t(i)];
        v.name = t.tabs[i];
        v.fresh = fresh[i] > 0 ? std::to_string(fresh[i]) : "";
        v.on = i == int(sh.tab);
        v.hot = i == sh.hoverTab;
    }
    m.closeHot = sh.hoverClose;
    m.typing = sh.typing;
    m.caret = sh.typing && sh.caretOn;
    m.query = sh.search;
    m.hint = sh.search.empty() && !sh.typing ? t.search : "";
    const int columns = std::max(1, L.columns), count = columns * std::max(1, L.rows);
    m.cells.assign(size_t(count), CellV{});  // the same count always: RmlUi's lists never shrink under it
    for (int i = 0; i < count; ++i) {
        CellV& c = m.cells[size_t(i)];
        const int at = i + sh.scroll * columns;
        c.index = at;
        c.hot = at == sh.hover;
        if (at >= int(shown.size())) continue;
        const iw::Item& it = items[size_t(shown[size_t(at)])];
        c.hit = "cell";
        if (it.icon && it.icon->width > 0) {
            c.icon = src(it.icon);
            c.iconW = float(it.icon->width);
            c.iconH = float(it.icon->height);
            c.op = it.identified ? 1.0f : 0.45f;
        }
        if (it.count > 1) c.count = iw::grouped(it.count);
        if (it.refine > 0) c.refine = "+" + std::to_string(it.refine);
        if (!it.identified) c.mark = "?", c.markCls = "mk q";
        else if (it.worn) c.mark = "E", c.markCls = "mk e";  // 3.0.11-beta.118: the arrows worn
        if (it.fresh && !t.fresh.empty()) c.fresh = t.fresh;
        c.ele = it.elementRgb ? hex(it.elementRgb) : "#00000000";  // 3.0.18-beta.144
        if (it.starred) c.star = "\xE2\x98\x85", c.starOn = true;
        else if (at == sh.hover) c.star = "\xE2\x98\x86";
    }
    for (const iw::Flash& f : sh.flashes) {  // a use answering in its cell: gold fading, its word rising
        for (int i = 0; i < count && size_t(i) < L.cells.size(); ++i) {
            const panel::Box& b = L.cells[size_t(i)];
            if (std::fabs(b.x - f.cell.x) >= 1 || std::fabs(b.y - f.cell.y) >= 1) continue;
            CellV& c = m.cells[size_t(i)];
            const float k = std::clamp(f.t, 0.0f, 1.0f), fade = 1 - k;
            c.flash = fade;
            c.ring = 0.95f * fade;
            if (c.hit != "cell" && f.icon && f.icon->width > 0) {
                c.icon = src(f.icon);
                c.iconW = float(f.icon->width);
                c.iconH = float(f.icon->height);
                c.op = fade * fade;
            }
            if (!f.label.empty()) {
                c.rising = f.label;
                c.rise = 22 * k;
                c.risingColour = hex(toward(f.rgb, 0x10141C, k));
            }
        }
    }
    const int rows = int((shown.size() + size_t(columns) - 1) / size_t(columns));
    m.scrollable = rows > L.rows;
    if (m.scrollable) {
        m.thumbH = 100.0f * float(L.rows) / float(rows);
        m.thumbTop = (100 - m.thumbH) * float(sh.scroll) / float(rows - L.rows);
    }
    m.dropping = sh.dropping;
    m.also = sh.also;
    m.weightWord = t.weight;
    m.bar = sh.also.empty();
    m.weightText.clear();
    if (sh.weightMost > 0) {
        const int percent = int(int64_t(sh.weight) * 100 / sh.weightMost);
        m.weightText = iw::grouped(sh.weight) + " / " + iw::grouped(sh.weightMost) + " \xC2\xB7 " +
                       std::to_string(percent) + " %";
    }
    const float share = sh.weightMost > 0 ? std::clamp(float(sh.weight) / float(sh.weightMost), 0.0f, 1.0f) : 0.0f;
    m.fill = 100 * share;
    m.tone = (sh.overweight >= 0 ? sh.overweight == 2 : share >= 0.9f)   ? "red"
             : (sh.overweight >= 0 ? sh.overweight == 1 : share >= 0.5f) ? "gold"
                                                                         : "green";
    m.zeny = sh.zeny >= 0 ? iw::grouped(sh.zeny) + " z" : "";
    m.tip = sh.tip && sh.hover >= 0 && sh.hover < int(shown.size());
    m.tipColour = kTipName;
    if (m.tip) {
        const iw::Item& it = items[size_t(shown[size_t(sh.hover)])];
        m.tip1 = iw::tipFirst(it);
        m.tipColour = it.elementRgb ? hex(it.elementRgb) : kTipName;
        m.tip2 = iw::tipSecond(it, t);
        m.tip3 = it.compare;
        m.tip4 = it.market;
    }
}

bool refit(Rml::ElementDocument* doc, iw::Layout& L) {
    if (!doc || !doc->IsVisible()) return false;
    const std::vector<Placed> cells = placesOf(doc, "cellbox");
    const size_t count = size_t(std::max(1, L.columns) * std::max(1, L.rows));
    if (cells.size() != count) return false;
    const Rml::Vector2f size = doc->GetBox().GetSize(Rml::BoxArea::Border);
    if (size.x <= 0 || size.y <= 0) return false;
    const auto one = [&](const char* part, panel::Box& to) {
        const std::vector<Placed> p = placesOf(doc, part);
        to = p.empty() ? panel::Box{} : p.front().box;
    };
    L.glass = panel::Box{0, 0, size.x, size.y};
    L.w = int(std::ceil(size.x));
    L.h = int(std::ceil(size.y));
    one("title", L.title);
    L.title = panel::Box{0, 0, size.x, L.title.y + L.title.h};  // the whole band over the tabs
    one("close", L.close);
    one("tabsrow", L.tabsRow);
    one("search", L.search);
    one("grid", L.grid);
    one("footer", L.footer);
    one("bar", L.bar);
    const std::vector<Placed> tabs = placesOf(doc, "tab");
    L.tabs.clear();
    for (const Placed& t : tabs) L.tabs.push_back(t.box);
    L.cells.clear();
    for (const Placed& c : cells) L.cells.push_back(c.box);
    return true;
}

void placeTip(Rml::ElementDocument* doc, Model& m, const iw::Layout& L, int cell) {
    if (!m.tip || cell < 0 || cell >= int(L.cells.size())) return;
    const float s = L.s > 0 ? L.s : 1;
    const std::vector<Placed> tip = placesOf(doc, "tip");  // its size as laid out last (the first frame: a guess)
    const float w = tip.empty() ? 220 * s : tip.front().box.w, h = tip.empty() ? 44 * s : tip.front().box.h;
    const panel::Box& c = L.cells[size_t(cell)];
    m.tipX = std::round(std::clamp(c.x + c.w / 2 - w / 2, 6 * s, std::max(6 * s, float(L.w) - w - 6 * s)));
    const bool under = c.y - h - 6 * s < L.grid.y;
    m.tipY = std::round(under ? c.y + c.h + 6 * s : c.y - h - 6 * s);
}

}  // namespace uagfx::uikit::itemgrid
