#include "uikit/trade_live.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "uikit/hits.hpp"

namespace uagfx::uikit::trade {
namespace {

namespace tw = uagfx::tradewin;

const char* kRml = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw trl" data-style-font-size="base + 'px'" data-style-background-color="ground">
<div class="head" data-hit="title"><span class="title">{{title}}</span></div>
<div class="x" data-hit="close" data-class-hot="closeHot">&#x2715;</div>
<div class="sides"><div class="side" data-for="sd : sides" data-class-them="sd.them"><div class="shead"
  data-hit="sidehead"><span class="who">{{sd.name}}</span><span class="ready" data-if="sd.ready">&#x2713;
  {{readyWord}}</span></div><div class="cells"><div class="dropframe" data-if="sd.dropping"></div><div
  class="cell"
  data-for="c : sd.cells" data-hit="cellbox" data-attr-data-index="c.index" data-class-hot="c.hot"
  data-class-late="c.late" data-class-thing="c.thing"><img data-if="c.icon != ''" data-attr-src="c.icon"
  data-style-width="c.iconW + 'px'" data-style-height="c.iconH + 'px'" data-style-opacity="c.op"/><span class="cnt"
  data-if="c.count != ''">{{c.count}}</span><span class="ref" data-if="c.refine != ''">{{c.refine}}</span></div>
</div><div class="zrow" data-hit="zrow"><span class="zl">{{zenyWord}}</span><div class="zfield" data-if="!sd.them"
  data-hit="zeny" data-class-typing="typing"><span class="zv">{{sd.zeny}}</span><span class="caret"
  data-if="caret"></span></div><span class="zv them" data-if="sd.them"
  data-class-late="sd.lateZeny">{{sd.zeny}}</span></div></div></div>
<div class="warn" data-hit="warn" data-class-hide="warn == ''">&#x26A0; {{warn}}</div>
<div class="btns"><div class="btn" data-for="b, i : buttons" data-hit="button" data-attr-data-index="i"
  data-class-main="b.main" data-class-hot="b.hot" data-class-off="!b.on">{{b.label}}</div></div>
<div class="hint" data-hit="hint" data-class-hide="hint == ''">{{hint}}</div>
<div class="tip" data-hit="tip" data-if="tip" data-style-left="tipX + 'px'" data-style-top="tipY + 'px'"><div
  class="t1">{{tip1}}</div><div class="t2">{{tip2}}</div></div>
</body></rml>)RML";

const tw::Thing* thingAt(const tw::Side& me, const tw::Side& them, int cell) {
    if (cell < 0 || cell >= 2 * tw::kSlots) return nullptr;
    const tw::Thing& x = cell < tw::kSlots ? me.slots[cell] : them.slots[cell - tw::kSlots];
    return x.count > 0 ? &x : nullptr;
}

}  // namespace

const char* rml() { return kRml; }

void bind(Rml::DataModelConstructor& c, Model& m) {
    if (auto e = c.RegisterStruct<CellV>()) {
        e.RegisterMember("index", &CellV::index);
        e.RegisterMember("icon", &CellV::icon);
        e.RegisterMember("count", &CellV::count);
        e.RegisterMember("refine", &CellV::refine);
        e.RegisterMember("thing", &CellV::thing);
        e.RegisterMember("hot", &CellV::hot);
        e.RegisterMember("late", &CellV::late);
        e.RegisterMember("op", &CellV::op);
        e.RegisterMember("iconW", &CellV::iconW);
        e.RegisterMember("iconH", &CellV::iconH);
    }
    c.RegisterArray<std::vector<CellV>>();
    if (auto e = c.RegisterStruct<SideV>()) {
        e.RegisterMember("name", &SideV::name);
        e.RegisterMember("zeny", &SideV::zeny);
        e.RegisterMember("them", &SideV::them);
        e.RegisterMember("ready", &SideV::ready);
        e.RegisterMember("lateZeny", &SideV::lateZeny);
        e.RegisterMember("dropping", &SideV::dropping);
        e.RegisterMember("cells", &SideV::cells);
    }
    if (auto e = c.RegisterStruct<ButtonV>()) {
        e.RegisterMember("label", &ButtonV::label);
        e.RegisterMember("on", &ButtonV::on);
        e.RegisterMember("main", &ButtonV::main);
        e.RegisterMember("hot", &ButtonV::hot);
    }
    c.RegisterArray<std::vector<SideV>>();
    c.RegisterArray<std::vector<ButtonV>>();
    c.Bind("ground", &m.ground);
    c.Bind("base", &m.base);
    c.Bind("title", &m.title);
    c.Bind("readyWord", &m.readyWord);
    c.Bind("zenyWord", &m.zenyWord);
    c.Bind("warn", &m.warn);
    c.Bind("hint", &m.hint);
    c.Bind("tip1", &m.tip1);
    c.Bind("tip2", &m.tip2);
    c.Bind("closeHot", &m.closeHot);
    c.Bind("typing", &m.typing);
    c.Bind("caret", &m.caret);
    c.Bind("tip", &m.tip);
    c.Bind("tipX", &m.tipX);
    c.Bind("tipY", &m.tipY);
    c.Bind("sides", &m.sides);
    c.Bind("buttons", &m.buttons);
}

void fill(Model& m, const tw::Side& me, const tw::Side& them, const tw::Show& sh, const tw::Texts& t, float scale,
          const Source& src) {
    m.base = 12 * (scale > 0 ? scale : 1);
    m.title = t.title;
    m.readyWord = t.ready;
    m.zenyWord = t.zeny;
    m.closeHot = sh.hoverClose;
    m.typing = sh.typing;
    m.caret = sh.typing && sh.caretOn;
    m.sides.resize(2);
    for (int sideNo = 0; sideNo < 2; ++sideNo) {
        const tw::Side& who = sideNo == 0 ? me : them;
        SideV& v = m.sides[size_t(sideNo)];
        v.name = sideNo == 0 ? t.you : t.partner;
        v.them = sideNo == 1;
        v.ready = who.ready;
        v.dropping = sideNo == 0 && sh.dropping && !me.ready;
        v.lateZeny = sideNo == 1 && sh.late.zeny;
        if (sideNo == 0)
            v.zeny = !sh.typing        ? itemwin::grouped(me.zeny)
                     : sh.typed.empty() ? std::string()
                                        : itemwin::grouped(std::atoll(sh.typed.c_str()));
        else
            v.zeny = itemwin::grouped(them.zeny);
        v.cells.assign(size_t(tw::kSlots), CellV{});
        for (int i = 0; i < tw::kSlots; ++i) {
            CellV& c = v.cells[size_t(i)];
            const int at = sideNo * tw::kSlots + i;
            c.index = at;
            c.hot = at == sh.hover;
            c.late = sideNo == 1 && sh.late.slots[i];
            const tw::Thing* x = thingAt(me, them, at);
            if (!x) continue;
            c.thing = true;
            if (x->icon && x->icon->width > 0) {
                c.icon = src(x->icon);
                c.iconW = float(x->icon->width);
                c.iconH = float(x->icon->height);
                c.op = x->identified ? 1.0f : 0.45f;
            }
            if (x->count > 1) c.count = itemwin::grouped(x->count);
            if (x->refine > 0) c.refine = "+" + std::to_string(x->refine);
        }
    }
    m.warn = sh.late.any() ? t.late : "";
    m.hint = me.ready ? "" : t.hint;
    const bool on[3] = {tw::canReady(me), tw::canTrade(me, them, sh.traded), true};
    const std::string labels[3] = {t.ready, t.trade, t.cancel};
    m.buttons.resize(3);
    for (int i = 0; i < 3; ++i)
        m.buttons[size_t(i)] = ButtonV{labels[i], on[i], (i == 0 && on[0]) || (i == 1 && on[1]),
                                       i == sh.hoverButton && on[i]};
    const tw::Thing* pointed = sh.tip ? thingAt(me, them, sh.hover) : nullptr;
    m.tip = pointed != nullptr;
    if (pointed) {
        m.tip1 = tw::tipFirst(*pointed);
        m.tip2 = t.info;
    }
}

bool refit(Rml::ElementDocument* doc, tw::Layout& L) {
    if (!doc || !doc->IsVisible()) return false;
    const std::vector<Placed> cells = placesOf(doc, "cellbox"), buttons = placesOf(doc, "button"),
                              heads = placesOf(doc, "sidehead"), zrows = placesOf(doc, "zrow");
    if (cells.size() != size_t(2 * tw::kSlots) || buttons.size() != 3 || heads.size() != 2 || zrows.size() != 2)
        return false;
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
    L.title = panel::Box{0, 0, size.x, L.title.y + L.title.h};
    one("close", L.close);
    one("zeny", L.zenyField);
    one("warn", L.warn);
    one("hint", L.hint);
    L.mineHead = heads[0].box;
    L.theirHead = heads[1].box;
    L.mineZeny = zrows[0].box;
    L.theirZeny = zrows[1].box;
    for (size_t i = 0; i < cells.size(); ++i) L.cells[i] = cells[i].box;
    for (size_t i = 0; i < 3; ++i) L.buttons[i] = buttons[i].box;
    return true;
}

void placeTip(Rml::ElementDocument* doc, Model& m, const tw::Layout& L, int cell) {
    if (!m.tip || cell < 0 || cell >= 2 * tw::kSlots) return;
    const float s = L.s > 0 ? L.s : 1;
    const std::vector<Placed> tip = placesOf(doc, "tip");
    const float w = tip.empty() ? 200 * s : tip.front().box.w, h = tip.empty() ? 44 * s : tip.front().box.h;
    const panel::Box& c = L.cells[cell];
    m.tipX = std::round(std::clamp(c.x + c.w / 2 - w / 2, 6 * s, std::max(6 * s, float(L.w) - w - 6 * s)));
    const bool upper = (cell % tw::kSlots) < tw::kColumns;  // the upper row: over it, the lower: under it
    m.tipY = std::round(std::max(2 * s, upper ? c.y - h - 6 * s : c.y + c.h + 6 * s));
}

}  // namespace uagfx::uikit::trade
