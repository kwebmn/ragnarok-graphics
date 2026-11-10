#include "win/ui_sheet.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::win::uisheet {
namespace {

const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="sh" data-style-width="w + 'px'" data-style-height="h + 'px'">
<div class="glass" data-if="glass.w > 0" data-style-left="glass.x + 'px'" data-style-top="glass.y + 'px'"
  data-style-width="glass.w + 'px'" data-style-height="glass.h + 'px'" data-style-border-radius="glass.r + 'px'"
  data-style-border-width="glass.e + 'px'" data-style-background-color="ground"></div>
<div data-for="b : under" data-attr-class="b.cls" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'"
  data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'"
  data-style-border-width="b.e + 'px'" data-style-opacity="b.op"></div>
<img class="a" data-for="i : icons" data-attr-src="i.src" data-style-left="i.x + 'px'" data-style-top="i.y + 'px'"
  data-style-width="i.w + 'px'" data-style-height="i.h + 'px'" data-style-opacity="i.op"/>
<div data-for="b : over" data-attr-class="b.cls" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'"
  data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'"
  data-style-border-width="b.e + 'px'" data-style-opacity="b.op"></div>
<span data-for="t : words" data-attr-class="t.cls" data-style-left="t.x + 'px'" data-style-top="t.y + 'px'"
  data-style-width="t.w + 'px'" data-style-height="t.h + 'px'" data-style-line-height="t.h + 'px'"
  data-style-font-size="t.px + 'px'" data-style-color="t.colour">{{t.text}}</span>
</body></rml>)RML";

}  // namespace

std::string Sheet::ground(uint32_t rgb, float opacity) {
    char b[16];
    std::snprintf(b, sizeof b, "#%06x%02x", unsigned(rgb & 0xFFFFFF),
                  int(std::lround(std::clamp(opacity, 0.0f, 1.0f) * 255)) & 0xFF);
    return b;
}

std::string Sheet::icon(const panel::Picture* p) {
    if (!p || p->width <= 0) return {};
    return uaui::picture(p, uaui::stamp(p), sources_["i:" + std::to_string(uintptr_t(p))]);
}

std::string Sheet::made(const std::string& key, int w, int h, const std::function<void(panel::Picture&)>& paint) {
    auto it = made_.find(key);
    if (it == made_.end()) {
        panel::Picture p;
        p.width = std::max(1, w), p.height = std::max(1, h);
        p.px.assign(size_t(p.width) * size_t(p.height), 0u);
        paint(p);
        it = made_.emplace(key, std::move(p)).first;
    }
    return uaui::picture(&it->second, uaui::stamp(&it->second), sources_["m:" + key]);
}

bool Sheet::frame(const char* name, bool on, bool window, int x, int y, int gen,
                  const std::function<void(Content&)>& fill) {
    if (!uaui::ready() || !on) {
        doc_.show(false);
        return false;
    }
    if (!doc_.made()) {
        doc_.passive = true;
        doc_.window = window;
        if (!doc_.make(name, kDoc, [&](Rml::DataModelConstructor& m) {
                if (auto b = m.RegisterStruct<Box>()) {
                    b.RegisterMember("x", &Box::x);
                    b.RegisterMember("y", &Box::y);
                    b.RegisterMember("w", &Box::w);
                    b.RegisterMember("h", &Box::h);
                    b.RegisterMember("r", &Box::r);
                    b.RegisterMember("e", &Box::e);
                    b.RegisterMember("op", &Box::op);
                    b.RegisterMember("cls", &Box::cls);
                }
                if (auto i = m.RegisterStruct<Icon>()) {
                    i.RegisterMember("x", &Icon::x);
                    i.RegisterMember("y", &Icon::y);
                    i.RegisterMember("w", &Icon::w);
                    i.RegisterMember("h", &Icon::h);
                    i.RegisterMember("op", &Icon::op);
                    i.RegisterMember("src", &Icon::src);
                }
                if (auto t = m.RegisterStruct<Word>()) {
                    t.RegisterMember("x", &Word::x);
                    t.RegisterMember("y", &Word::y);
                    t.RegisterMember("w", &Word::w);
                    t.RegisterMember("h", &Word::h);
                    t.RegisterMember("px", &Word::px);
                    t.RegisterMember("text", &Word::text);
                    t.RegisterMember("colour", &Word::colour);
                    t.RegisterMember("cls", &Word::cls);
                }
                m.RegisterArray<std::vector<Box>>();
                m.RegisterArray<std::vector<Icon>>();
                m.RegisterArray<std::vector<Word>>();
                m.Bind("w", &c_.w);
                m.Bind("h", &c_.h);
                m.Bind("glass", &c_.glass);
                m.Bind("ground", &c_.ground);
                m.Bind("under", &c_.under);
                m.Bind("icons", &c_.icons);
                m.Bind("over", &c_.over);
                m.Bind("words", &c_.words);
            }))
            return false;
    }
    if (gen != gen_) {
        gen_ = gen;
        c_ = Content{};
        c_.ground = "#10141cff";
        fill(c_);
        doc_.dirtyAll();
    }
    doc_.place(x, y);
    doc_.show(true);
    return true;
}

}  // namespace uagfx::win::uisheet
