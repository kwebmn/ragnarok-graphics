#include "uikit/equip_live.hpp"

#include "core/item_elements.hpp"

#include <RmlUi/Core.h>

#include <cstdio>

#include <algorithm>
#include <cmath>

#include "core/pad_glyphs.hpp"
#include "uikit/hits.hpp"

namespace uagfx::uikit::equip {
namespace {

namespace ew = uagfx::equipwin;

const char* kRml = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw eql" data-style-font-size="base + 'px'" data-style-background-color="ground">
<div class="head" data-hit="title"><span class="title">{{title}}</span><span class="hk">{{hotkey}}</span></div>
<div class="x" data-hit="close" data-class-hot="closeHot">&#x2715;</div>
<div class="cols">
<div class="col left"><div class="slot" data-for="sl : left" data-hit="slot" data-attr-data-index="sl.index"
  data-class-hot="sl.hot" data-class-focus="sl.focus" data-class-fits="sl.fits"><div class="tile" data-hit="tile"
  data-attr-data-index="sl.index"><img data-if="sl.icon != ''" data-attr-src="sl.icon"
  data-style-width="sl.iconW + 'px'" data-style-height="sl.iconH + 'px'" data-style-opacity="sl.op"/></div><div
  class="txt"><div class="lbl">{{sl.label}}</div><div class="nm" data-class-worn="sl.worn"
  data-class-spill="sl.spill"><span data-if="sl.eleWord == ''">{{sl.name}}</span><span data-if="sl.eleWord != ''"
  data-style-color="sl.ele">{{sl.name}}</span><span class="dot" data-for="d : sl.dots"
  data-class-card="d > 0"></span></div></div></div></div>
<div class="centre" data-hit="centre"><div class="stage"><img data-if="hero != ''" data-attr-src="hero"
  data-style-width="heroW + 'px'" data-style-height="heroH + 'px'"/><div class="turn"
  data-if="turns || mountOff != ''"><span
  class="tb" data-if="turns" data-hit="turnleft" data-class-hot="turnHot == 0">&#x25C0;</span><span class="mi"
  data-if="mountOff != ''" data-hit="mountoff" data-class-hot="mountHot"><img data-if="mountIcon != ''"
  data-attr-src="mountIcon" data-style-width="mountIconSide + 'px'"
  data-style-height="mountIconSide + 'px'"/></span><span
  class="tb" data-if="turns" data-hit="turnright" data-class-hot="turnHot == 1">&#x25B6;</span></div><div class="mtip"
  data-if="mountHot && !mountAsk" data-hit="mounttip"><span class="tp">{{mountOff}}</span></div><div class="mtip"
  data-if="mountAsk"><div class="ask"><span class="aq">{{mountOff}}?</span><div class="ab"><span class="ay"
  data-hit="mountyes" data-class-hot="askHot == 0">{{askYes}}</span><span class="an" data-hit="mountno"
  data-class-hot="askHot == 1">{{askNo}}</span></div></div></div></div>
<div class="picks" data-if="picking" data-hit="picklist"><div class="ptitle">{{pickTitle}}</div><div class="pnone"
  data-if="picks.size == 0">{{pickNone}}</div><div class="pick" data-for="p : picks" data-hit="pick"
  data-attr-data-index="p.index" data-class-hot="p.hot" data-class-focus="p.focus"><div class="pic"><img
  data-if="p.icon != ''" data-attr-src="p.icon" data-style-width="p.iconW + 'px'"
  data-style-height="p.iconH + 'px'"/></div><span class="pn">{{p.name}}</span></div></div></div>
<div class="col right"><div class="slot" data-for="sl : right" data-hit="slot" data-attr-data-index="sl.index"
  data-class-hot="sl.hot" data-class-focus="sl.focus" data-class-fits="sl.fits"><div class="tile" data-hit="tile"
  data-attr-data-index="sl.index"><img data-if="sl.icon != ''" data-attr-src="sl.icon"
  data-style-width="sl.iconW + 'px'" data-style-height="sl.iconH + 'px'" data-style-opacity="sl.op"/></div><div
  class="txt"><div class="lbl">{{sl.label}}</div><div class="nm" data-class-worn="sl.worn"
  data-class-spill="sl.spill"><span class="dot" data-for="d : sl.dots" data-class-card="d > 0"></span><span
  data-if="sl.eleWord == ''">{{sl.name}}</span><span data-if="sl.eleWord != ''"
  data-style-color="sl.ele">{{sl.name}}</span></div></div></div></div>
</div>
<div class="ammorow"><div class="slot ammo" data-hit="slot" data-attr-data-index="ammo.index" data-class-hot="ammo.hot"
  data-class-focus="ammo.focus" data-class-fits="ammo.fits"><div class="tile" data-hit="tile"
  data-attr-data-index="ammo.index"><img data-if="ammo.icon != ''" data-attr-src="ammo.icon"
  data-style-width="ammo.iconW + 'px'" data-style-height="ammo.iconH + 'px'" data-style-opacity="ammo.op"/></div><div
  class="txt"><div class="lbl">{{ammo.label}}</div><div class="nm" data-class-worn="ammo.worn">{{ammo.name}}</div>
</div></div></div>
<div class="foot" data-hit="footer"><span class="cards">{{cards}}</span><span class="drag"
  data-if="!pad">{{drag}}</span><div class="pads" data-if="pad"><div class="pad" data-for="p : padItems"><img
  data-attr-src="p.glyph" data-style-width="p.w + 'px'" data-style-height="p.h + 'px'"/><span>{{p.words}}</span></div>
</div></div>
</body></rml>)RML";

std::string filled(std::string form, const std::vector<std::string>& values) {
    for (size_t i = 0; i < values.size(); ++i) {
        const std::string key = "{" + std::to_string(i + 1) + "}";
        for (size_t at = form.find(key); at != std::string::npos; at = form.find(key, at + values[i].size()))
            form.replace(at, key.size(), values[i]);
    }
    return form;
}

std::string named(const ew::Worn& w) { return w.refine > 0 ? "+" + std::to_string(w.refine) + " " + w.name : w.name; }

void registerSlot(Rml::DataModelConstructor& c) {
    c.RegisterArray<std::vector<int>>();
    if (auto e = c.RegisterStruct<SlotV>()) {
        e.RegisterMember("index", &SlotV::index);
        e.RegisterMember("icon", &SlotV::icon);
        e.RegisterMember("label", &SlotV::label);
        e.RegisterMember("name", &SlotV::name);
        e.RegisterMember("worn", &SlotV::worn);
        e.RegisterMember("spill", &SlotV::spill);
        e.RegisterMember("hot", &SlotV::hot);
        e.RegisterMember("focus", &SlotV::focus);
        e.RegisterMember("fits", &SlotV::fits);
        e.RegisterMember("op", &SlotV::op);
        e.RegisterMember("iconW", &SlotV::iconW);
        e.RegisterMember("iconH", &SlotV::iconH);
        e.RegisterMember("dots", &SlotV::dots);
        e.RegisterMember("eleWord", &SlotV::eleWord);
        e.RegisterMember("ele", &SlotV::ele);
    }
}

}  // namespace

const char* rml() { return kRml; }

void bind(Rml::DataModelConstructor& c, Model& m) {
    registerSlot(c);
    if (auto e = c.RegisterStruct<StatV>()) {
        e.RegisterMember("name", &StatV::name);
        e.RegisterMember("value", &StatV::value);
    }
    if (auto e = c.RegisterStruct<PickV>()) {
        e.RegisterMember("index", &PickV::index);
        e.RegisterMember("icon", &PickV::icon);
        e.RegisterMember("name", &PickV::name);
        e.RegisterMember("hot", &PickV::hot);
        e.RegisterMember("focus", &PickV::focus);
        e.RegisterMember("iconW", &PickV::iconW);
        e.RegisterMember("iconH", &PickV::iconH);
    }
    if (auto e = c.RegisterStruct<PadV>()) {
        e.RegisterMember("glyph", &PadV::glyph);
        e.RegisterMember("words", &PadV::words);
        e.RegisterMember("w", &PadV::w);
        e.RegisterMember("h", &PadV::h);
    }
    c.RegisterArray<std::vector<SlotV>>();
    c.RegisterArray<std::vector<StatV>>();
    c.RegisterArray<std::vector<PickV>>();
    c.RegisterArray<std::vector<PadV>>();
    c.Bind("ground", &m.ground);
    c.Bind("base", &m.base);
    c.Bind("title", &m.title);
    c.Bind("hotkey", &m.hotkey);
    c.Bind("who", &m.who);
    c.Bind("job", &m.job);
    c.Bind("level", &m.level);
    c.Bind("hero", &m.hero);
    c.Bind("turns", &m.turns);
    c.Bind("mountOff", &m.mountOff);
    c.Bind("mountHot", &m.mountHot);
    c.Bind("turnHot", &m.turnHot);
    c.Bind("mountIcon", &m.mountIcon);
    c.Bind("mountIconSide", &m.mountIconSide);
    c.Bind("mountAsk", &m.mountAsk);
    c.Bind("askHot", &m.askHot);
    c.Bind("askYes", &m.askYes);
    c.Bind("askNo", &m.askNo);
    c.Bind("cards", &m.cards);
    c.Bind("drag", &m.drag);
    c.Bind("pickTitle", &m.pickTitle);
    c.Bind("pickNone", &m.pickNone);
    c.Bind("closeHot", &m.closeHot);
    c.Bind("picking", &m.picking);
    c.Bind("pad", &m.pad);
    c.Bind("heroW", &m.heroW);
    c.Bind("heroH", &m.heroH);
    c.Bind("left", &m.left);
    c.Bind("right", &m.right);
    c.Bind("ammo", &m.ammo);
    c.Bind("stats", &m.stats);
    c.Bind("picks", &m.picks);
    c.Bind("padItems", &m.padItems);
}

void fill(Model& m, const ew::Data& d, const ew::Show& sh, const ew::Texts& t, float scale, const Source& src,
          const Glyph& glyph) {
    m.base = 12 * (scale > 0 ? scale : 1);
    m.title = t.title;
    m.hotkey = t.hotkey;
    m.closeHot = sh.hoverClose;
    m.picking = sh.picking;
    m.pad = sh.pad;
    const auto slot = [&](int i) {
        SlotV v;
        v.index = i;
        v.label = t.labels[i];
        v.hot = i == sh.hoverSlot && !sh.picking;
        v.focus = sh.pad && i == sh.focus;
        v.fits = ((sh.dropFits >> i) & 1u) != 0;
        const ew::Worn& w = d.slots[i];
        v.worn = w.index > 0;
        v.spill = w.spill;
        v.name = !v.worn                         ? t.empty
                 : i == ew::Ammo && w.count > 0 ? named(w) + " \xC3\x97" + std::to_string(w.count)
                                                 : named(w);
        if (v.worn && w.icon && w.icon->width > 0) {
            v.icon = src(w.icon);
            v.iconW = float(w.icon->width);
            v.iconH = float(w.icon->height);
            v.op = w.spill ? 0.4f : 1.0f;
        }
        if (v.worn && !w.spill)
            for (int n = 0; n < std::min(w.slots, 4); ++n) v.dots.push_back(w.cards[n] > 0 ? 1 : 0);
        if (v.worn && w.element > 0) {  // 3.0.18-beta.146
            v.eleWord = itemelements::word(w.element);
            char hexc[8];
            std::snprintf(hexc, sizeof hexc, "#%06x", unsigned(itemelements::colour(w.element)));
            v.ele = hexc;
        }
        return v;
    };
    m.left.clear();
    m.right.clear();
    for (int r = 0; r < 5; ++r) {  // 3.0.18-beta.110: the original window's columns
        m.left.push_back(slot(ew::kLeftColumn[r]));
        m.right.push_back(slot(ew::kRightColumn[r]));
    }
    m.ammo = slot(ew::Ammo);
    m.who = d.who;
    m.job = d.job;
    m.level = d.baseLevel > 0 ? filled(t.level, {std::to_string(d.baseLevel), std::to_string(d.jobLevel)}) : "";
    // the character in the middle (3.0.18-beta.110: its portrait until its own sprites are drawn)
    if (!m.turns) {
        m.hero = d.portrait && d.portrait->width > 0 ? src(d.portrait) : "";
        m.heroW = d.portrait ? float(d.portrait->width) : 0;
        m.heroH = d.portrait ? float(d.portrait->height) : 0;
    }
    m.stats.clear();
    for (const auto& s : d.stats) m.stats.push_back(StatV{s.first, s.second});
    m.pickTitle = filled(t.pickTitle, {sh.pickSlot >= 0 && sh.pickSlot < ew::kSlots ? t.labels[sh.pickSlot] : ""});
    m.pickNone = t.pickNone;
    m.picks.clear();
    for (size_t i = 0; sh.picking && i < d.picks.size() && int(i) < kMostPicks; ++i) {
        const ew::Pick& p = d.picks[i];
        PickV v;
        v.index = int(i);
        v.name = p.refine > 0 ? "+" + std::to_string(p.refine) + " " + p.name : p.name;
        v.hot = int(i) == sh.hoverPick;
        v.focus = int(i) == sh.pickFocus;
        if (p.icon && p.icon->width > 0) {
            v.icon = src(p.icon);
            v.iconW = float(p.icon->width);
            v.iconH = float(p.icon->height);
        }
        m.picks.push_back(v);
    }
    m.cards = t.cards;
    m.drag = t.drag;
    m.padItems.clear();
    if (sh.pad) {  // as core equipwin's: what A, X, Y and B do now
        using B = padglyph::Button;
        std::vector<std::pair<B, const std::string*>> items;
        const bool worn = sh.focus >= 0 && sh.focus < ew::kSlots && d.slots[sh.focus].index > 0;
        if (sh.picking) items = {{B::A, &t.put}, {B::B, &t.back}};
        else if (!worn) items = {{B::A, &t.put}, {B::B, &t.close}};
        else items = {{B::A, &t.off}, {B::X, &t.change}, {B::Y, &t.card}, {B::B, &t.close}};
        const float h = std::round(17 * scale);
        for (const auto& [b, words] : items) {
            PadV v;
            v.h = h;
            v.glyph = glyph(int(b), h, v.w);
            v.words = *words;
            m.padItems.push_back(v);
        }
    }
}

bool refit(Rml::ElementDocument* doc, ew::Layout& L) {
    if (!doc || !doc->IsVisible()) return false;
    const std::vector<Placed> slots = placesOf(doc, "slot"), tiles = placesOf(doc, "tile");
    if (slots.size() != size_t(ew::kSlots) || tiles.size() != size_t(ew::kSlots)) return false;
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
    one("centre", L.centre);
    one("picklist", L.pickList);
    one("footer", L.footer);
    for (int i = 0; i < ew::kSlots; ++i) L.slots[i] = L.icons[i] = L.dots[i] = panel::Box{};
    for (const Placed& p : slots)
        if (p.index >= 0 && p.index < ew::kSlots) L.slots[p.index] = p.box;
    for (const Placed& p : tiles)
        if (p.index >= 0 && p.index < ew::kSlots) L.icons[p.index] = p.box;
    L.picks.clear();
    for (const Placed& p : placesOf(doc, "pick")) L.picks.push_back(p.box);
    return true;
}

}  // namespace uagfx::uikit::equip
