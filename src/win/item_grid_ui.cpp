#include "win/item_grid_ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::win::itemgridui {
namespace {

namespace iw = uagfx::itemwin;

const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="ig" data-style-width="w + 'px'" data-style-height="h + 'px'">
<div class="glass" data-style-left="glass.x + 'px'" data-style-top="glass.y + 'px'" data-style-width="glass.w + 'px'"
  data-style-height="glass.h + 'px'" data-style-border-radius="glass.r + 'px'" data-style-border-width="glass.e + 'px'"
  data-style-background-color="ground"></div>
<div data-for="b : under" data-attr-class="b.cls" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'"
  data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'"
  data-style-border-width="b.e + 'px'" data-style-opacity="b.op"></div>
<div class="a" data-for="i : icons">
 <img class="a" data-attr-src="i.src" data-style-left="i.x + 'px'" data-style-top="i.y + 'px'"
  data-style-width="i.w + 'px'" data-style-height="i.h + 'px'" data-style-opacity="i.op"/></div>
<div data-for="b : over" data-attr-class="b.cls" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'"
  data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'"
  data-style-border-width="b.e + 'px'" data-style-opacity="b.op"></div>
<span data-for="t : words" data-attr-class="t.cls" data-style-left="t.x + 'px'" data-style-top="t.y + 'px'"
  data-style-width="t.w + 'px'" data-style-height="t.h + 'px'" data-style-line-height="t.h + 'px'"
  data-style-font-size="t.px + 'px'" data-style-color="t.colour">{{t.text}}</span>
<div class="mark"></div>
</body></rml>)RML";

std::string hexOf(uint32_t rgb, int alpha = -1) {
    char b[16];
    if (alpha < 0) std::snprintf(b, sizeof b, "#%06x", unsigned(rgb & 0xFFFFFF));
    else std::snprintf(b, sizeof b, "#%06x%02x", unsigned(rgb & 0xFFFFFF), alpha & 0xFF);
    return b;
}

std::string source(Ui& u, const std::string& key, const panel::Picture* p) {
    if (!p || p->width <= 0) return {};
    return uaui::picture(p, uaui::stamp(p), u.sources[key]);
}

}  // namespace

bool Ui::frame(const char* name, bool on, int x, int y, int generation, const iw::Layout& L,
               const std::vector<iw::Item>& items, const std::vector<int>& shown, const iw::Show& sh,
               const iw::Texts& t, const iw::Measure& m) {
    if (!uaui::ready() || !on || L.w <= 0) {
        doc.show(false);
        return false;
    }
    if (!doc.made()) {
        doc.passive = true;
        doc.window = true;
        if (!doc.make(name, kDoc, [&](Rml::DataModelConstructor& c) {
                if (auto b = c.RegisterStruct<BoxV>()) {
                    b.RegisterMember("x", &BoxV::x);
                    b.RegisterMember("y", &BoxV::y);
                    b.RegisterMember("w", &BoxV::w);
                    b.RegisterMember("h", &BoxV::h);
                    b.RegisterMember("r", &BoxV::r);
                    b.RegisterMember("e", &BoxV::e);
                    b.RegisterMember("op", &BoxV::op);
                    b.RegisterMember("cls", &BoxV::cls);
                }
                if (auto i = c.RegisterStruct<IconV>()) {
                    i.RegisterMember("x", &IconV::x);
                    i.RegisterMember("y", &IconV::y);
                    i.RegisterMember("w", &IconV::w);
                    i.RegisterMember("h", &IconV::h);
                    i.RegisterMember("op", &IconV::op);
                    i.RegisterMember("src", &IconV::src);
                }
                if (auto t = c.RegisterStruct<WordV>()) {
                    t.RegisterMember("x", &WordV::x);
                    t.RegisterMember("y", &WordV::y);
                    t.RegisterMember("w", &WordV::w);
                    t.RegisterMember("h", &WordV::h);
                    t.RegisterMember("px", &WordV::px);
                    t.RegisterMember("text", &WordV::text);
                    t.RegisterMember("colour", &WordV::colour);
                    t.RegisterMember("cls", &WordV::cls);
                }
                c.RegisterArray<std::vector<BoxV>>();
                c.RegisterArray<std::vector<IconV>>();
                c.RegisterArray<std::vector<WordV>>();
                c.Bind("w", &w);
                c.Bind("h", &h);
                c.Bind("ground", &ground);
                c.Bind("glass", &glass);
                c.Bind("under", &under);
                c.Bind("icons", &icons);
                c.Bind("over", &over);
                c.Bind("words", &words);
            }))
            return false;
    }
    if (generation != gen) {
        gen = generation;
        const float s = L.s;
        const iw::View v = iw::view(L, items, shown, sh, t, m);
        w = float(L.w), h = float(L.h);
        ground = hexOf(0x10141C, int(std::lround(uistyle::window() * 255)));
        glass = BoxV{L.glass.x, L.glass.y, L.glass.w, L.glass.h, 12 * s, 1.0f * s, 1, "glass"};
        under.clear(), over.clear(), icons.clear(), words.clear();
        const auto box = [](std::vector<BoxV>& to, const panel::Box& b, float r, const char* cls, float e = 0,
                            float op = 1) {
            if (b.w > 0 && b.h > 0) to.push_back(BoxV{b.x, b.y, b.w, b.h, r, e, op, cls});
        };
        if (sh.hoverClose) box(under, L.close, 7 * s, "hot");
        box(under, panel::Box{L.tabsRow.x, L.tabsRow.y + L.tabsRow.h - 1 * s, L.tabsRow.w, 1 * s}, 0, "rule");
        box(under, v.tabLine, 1 * s, "tab");
        box(under, L.search, 8 * s, sh.typing ? "field typing" : "field", 1.0f * s);
        box(under, v.caret, 0, "caret");
        for (const iw::CellView& c : v.cells)
            box(under, c.box, 8 * s, c.hot ? "cell hot" : "cell", (c.hot ? 1.5f : 1) * s);
        const int side = std::max(8, int(std::ceil(v.searchIcon.w)));
        auto it = made.find("search" + std::to_string(side));
        if (it == made.end()) {
            panel::Picture p;
            p.width = p.height = side;
            p.px.assign(size_t(side) * size_t(side), 0u);
            uiicons::draw(p, uiicons::Icon::Search, 0, 0, float(side), 0x9AA3B5, 0.9f, 2.2f);
            it = made.emplace("search" + std::to_string(side), std::move(p)).first;
        }
        icons.push_back(IconV{v.searchIcon.x, v.searchIcon.y, float(side), float(side), 1,
                              source(*this, it->first, &it->second)});
        for (const iw::CellView& c : v.cells)
            if (c.icon)
                icons.push_back(IconV{c.iconX, c.iconY, float(c.icon->width), float(c.icon->height), c.iconOpacity,
                                      source(*this, "i:" + std::to_string(uintptr_t(c.icon)), c.icon)});
        for (const iw::FlashView& f : v.flashes)
            if (f.icon)
                icons.push_back(IconV{std::round(f.cell.x + (f.cell.w - float(f.icon->width)) / 2),
                                      std::round(f.cell.y + (f.cell.h - float(f.icon->height)) / 2),
                                      float(f.icon->width), float(f.icon->height), f.iconAlpha,
                                      source(*this, "i:" + std::to_string(uintptr_t(f.icon)), f.icon)});
        for (const iw::CellView& c : v.cells) {
            box(over, c.count, 3 * s, "plate");
            box(over, c.refine, 3 * s, "plate");
        }
        box(over, v.scroll, 1.5f * s, "thumb");
        box(over, panel::Box{0, L.footer.y, L.glass.w, 1 * s}, 0, "rule");
        if (v.bar) box(over, L.bar, 3 * s, "bar");
        const char* tone = v.fillTone == iw::Tone::Red ? "fill red" : v.fillTone == iw::Tone::Gold ? "fill gold"
                                                                                                   : "fill green";
        box(over, v.fill, 3 * s, tone);
        for (const iw::CellView& c : v.cells) box(over, c.fresh, c.fresh.h / 2, "fresh");
        if (sh.dropping) box(over, L.grid, 8 * s, "dropping", 2.0f * s);
        for (const iw::FlashView& f : v.flashes) {
            box(over, f.cell, 8 * s, "flash", 0, f.fill / 0.30f);
            box(over, f.ring, 8 * s + (f.ring.x < f.cell.x ? f.cell.x - f.ring.x : 0), "ring", 2.0f * s,
                f.ringAlpha);
        }
        box(over, v.tip, 8 * s, "tip", 1.0f * s);
        for (const iw::Words& x : iw::words(L, items, shown, sh, t, m)) {
            const char* align = x.align == iw::Align::Right ? " right" : x.align == iw::Align::Center ? " centre" : "";
            words.push_back(WordV{x.box.x, x.box.y, x.box.w, x.box.h, x.px, x.text, hexOf(x.rgb),
                                  std::string(x.bold ? "word bold" : "word") + align});
        }
        doc.dirtyAll();
    }
    doc.place(x, y);
    doc.show(true);
    return true;
}

}  // namespace uagfx::win::itemgridui
