#include "uikit/guild_live.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "uikit/hits.hpp"

namespace uagfx::uikit::guild {
namespace {

namespace gc = uagfx::guildcard;
namespace ly = uagfx::uikit::layered;

std::string markup() {
    std::string s = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw gdl" data-style-font-size="base + 'px'" data-style-width="width + 'px'"
  data-style-background-color="ground">
<div class="head" data-hit="title"><span class="title">{{title}}</span></div>
<div class="x" data-hit="close" data-class-hot="closeHot">&#x2715;</div>
<div class="gtabs" data-hit="tabsrow"><div class="gtab" data-for="t, i : tabs" data-hit="tab" data-attr-data-index="i"
  data-class-on="t.on" data-class-hot="t.hot" data-class-off="t.off">{{t.name}}</div></div>
)RML";
    s += ly::rml();
    s += "\n</body></rml>";
    return s;
}

// The body's top in core guildcard's layout: under the tabs' second row
float bodyTop(const gc::Layout& L) { return L.tabs[gc::kTabs - 1].y + L.tabs[gc::kTabs - 1].h + 8 * L.s; }

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
        e.RegisterMember("off", &TabV::off);
    }
    c.RegisterArray<std::vector<TabV>>();
    c.Bind("base", &m.base);
    c.Bind("width", &m.width);
    c.Bind("ground", &m.ground);
    c.Bind("title", &m.title);
    c.Bind("closeHot", &m.closeHot);
    c.Bind("tabs", &m.tabs);
    ly::bind(c, m.layer);
}

void fill(Model& m, const gc::Layout& L, const gc::Info& g, const std::vector<gc::Member>& members,
          const gc::More& more, const gc::Show& sh, const gc::Texts& t, const std::string& emblem) {
    const float sc = L.s > 0 ? L.s : 1;
    constexpr uint32_t kWhite = 0xFFFFFF, kGold = 0xC9A55B, kOnline = 0x4CC777, kOffline = 0x5C6370;
    m.base = 12 * sc;
    m.width = float(L.wide > 0 ? L.wide : L.w);  // 3.0.19-beta.1: its size's, never refit's old one
    m.title = t.title;
    m.closeHot = sh.hoverClose;
    m.tabs.resize(gc::kTabs);
    for (int i = 0; i < gc::kTabs; ++i)
        m.tabs[size_t(i)] = TabV{t.tabs[i], int(sh.tab) == i, sh.hoverTab == i && gc::allowed(i, sh.rights),
                                 !gc::allowed(i, sh.rights)};
    const float top = bodyTop(L);
    m.layer.w = float(L.w);
    {
        ly::Builder b(m.layer, panel::Box{0, top, 0, 0});
        b.box(L.list, 8 * sc, 0x000000, 0.16f);
        if (L.emblem.w > 0) {  // 3.0.19-beta.1: the guild's emblem, a quiet frame till the game's file is read
            if (!emblem.empty()) b.icon(L.emblem, emblem);
            else b.box(L.emblem, 8 * sc, kWhite, 0.05f, kGold, 0.35f, 1.0f * sc);
        }
        if (L.edit.w > 0 && sh.canEdit) {
            b.box(L.edit, 8 * sc, kGold, sh.hoverEdit ? 0.34f : 0.22f, kGold, 0.8f, 1.0f * sc);
            b.hit("edit", -1, L.edit);
        }
        if (sh.editing && sh.tab == gc::Tab::Notice) {  // the notice's fields, the one typed into in gold; «Скасувати»
            for (int i = 0; i < 2; ++i)
                if (L.fields[i].w > 0) {
                    b.box(L.fields[i], 6 * sc, kWhite, sh.hoverField == i ? 0.10f : 0.06f,
                          sh.field == i ? kGold : kWhite, sh.field == i ? 0.8f : 0.16f, 1.0f * sc);
                    b.hit("field", i, L.fields[i]);
                }
            if (L.cancel.w > 0) {
                b.box(L.cancel, 8 * sc, kWhite, sh.hoverCancel ? 0.14f : 0.06f, kWhite, 0.18f, 1.0f * sc);
                b.hit("cancel", -1, L.cancel);
            }
        }
        if (sh.tab != gc::Tab::Members) {  // a skill's «+» where it can rise
            for (size_t i = 0; i < L.plus.size(); ++i) {
                const size_t at = i + size_t(std::max(0, sh.scroll));
                if (at < more.lines.size() && more.lines[at].plus)
                    b.box(L.plus[i], 6 * sc, kGold, sh.hoverPlus == int(i) ? 0.45f : 0.28f, kGold, 0.85f, 1.0f * sc);
                b.hit("plus", int(i), L.plus[i]);
            }
        } else {
            for (size_t i = 0; i < L.rows.size(); ++i) {  // each member's dot: in the game or not
                const size_t at = i + size_t(std::max(0, sh.scroll));
                if (at >= members.size()) break;
                const float d = 8 * sc;
                b.box(panel::Box{L.rows[i].x + 8 * sc, L.rows[i].y + (L.rows[i].h - d) / 2, d, d}, d / 2,
                      members[at].online ? kOnline : kOffline, 0.95f);
            }
        }
        for (const gc::Words& x : gc::words(L, g, members, more, sh, t)) {  // the body's words (the head's its own)
            if (x.box.y + x.box.h / 2 < top) continue;
            b.word(x.box, x.text, x.rgb, x.px, x.bold,
                   x.align == gc::Align::Right    ? ly::Align::Right
                   : x.align == gc::Align::Center ? ly::Align::Center
                                                  : ly::Align::Left);
        }
    }
    m.layer.h = ly::contentHeight(m.layer, 8 * sc);  // 3.0.18-beta.104: from what it holds
}

bool refit(Rml::ElementDocument* doc, gc::Layout& L) {
    if (!doc || !doc->IsVisible()) return false;
    const Rml::Vector2f size = doc->GetBox().GetSize(Rml::BoxArea::Border);
    const std::vector<Placed> tabs = placesOf(doc, "tab");
    if (size.x <= 0 || size.y <= 0 || tabs.size() != size_t(gc::kTabs)) return false;
    float dx = 0, dy = 0;
    if (!layered::bodyShift(doc, panel::Box{0, bodyTop(L), 0, 0}, dx, dy)) return false;
    const auto shift = [&](panel::Box& b) {
        if (b.w > 0) b.x += dx, b.y += dy;
    };
    for (panel::Box* b : {&L.head, &L.list, &L.notice, &L.edit, &L.fields[0], &L.fields[1], &L.cancel}) shift(*b);
    for (panel::Box& b : L.facts) shift(b);
    for (auto* list : {&L.rows, &L.plus})
        for (panel::Box& b : *list) shift(b);
    for (int i = 0; i < gc::kTabs; ++i) L.tabs[i] = tabs[size_t(i)].box;
    const std::vector<Placed> close = placesOf(doc, "close"), head = placesOf(doc, "title");
    if (!close.empty()) L.close = close.front().box;
    if (!head.empty()) L.title = head.front().box;
    L.glass = panel::Box{0, 0, size.x, size.y};
    L.w = int(std::ceil(size.x));
    L.h = int(std::ceil(size.y));
    return true;
}

}  // namespace uagfx::uikit::guild
