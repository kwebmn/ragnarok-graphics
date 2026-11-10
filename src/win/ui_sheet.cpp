#include "win/ui_sheet.hpp"

#include <algorithm>
#include <cmath>
#include <set>

#include "core/pad_glyphs.hpp"
#include "core/class_art.hpp"
#include "win/code_page.hpp"
#include "win/log.hpp"
#include "win/window_stack.hpp"
#include "core/pad_nav.hpp"

namespace uagfx::win::uisheet {
namespace {

const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="sh" data-style-width="w + 'px'" data-style-height="h + 'px'" data-style-opacity="op">
<div class="glass" data-if="glass.w > 0" data-style-left="glass.x + 'px'" data-style-top="glass.y + 'px'"
  data-style-width="glass.w + 'px'" data-style-height="glass.h + 'px'" data-style-border-radius="glass.r + 'px'"
  data-style-border-width="glass.e + 'px'" data-style-background-color="ground"></div>
<div class="a" data-for="b : under">
 <div data-if="!b.paint" data-attr-class="b.cls" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'"
   data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'"
   data-style-border-width="b.e + 'px'" data-style-opacity="b.op"></div>
 <div data-if="b.paint" class="paint" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'"
   data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'"
   data-style-border-width="b.e + 'px'" data-style-background-color="b.bg" data-style-border-color="b.bd"
   data-style-opacity="b.op"></div>
</div>
<ua-ring class="a" data-for="r : rings" data-style-left="r.x + 'px'" data-style-top="r.y + 'px'"
  data-style-width="r.size + 'px'" data-style-height="r.size + 'px'" data-attr-value="r.value" data-attr-start="r.start"
  data-attr-width="r.width" data-attr-sector="r.sector ? 1 : 0" data-attr-fill="r.fill" data-attr-track="r.track"
  style="position: absolute;"></ua-ring>
<div class="clip" data-if="clip.w > 0" data-style-left="clip.x + 'px'" data-style-top="clip.y + 'px'"
  data-style-width="clip.w + 'px'" data-style-height="clip.h + 'px'">
 <div class="a" data-for="b : clipped">
  <div class="paint" data-style-left="b.x - clip.x + 'px'" data-style-top="b.y - clip.y + 'px'"
    data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'"
    data-style-border-width="b.e + 'px'" data-style-background-color="b.bg" data-style-border-color="b.bd"></div>
 </div>
 <div class="a" data-for="i : clipped_icons">
 <img class="a" data-attr-src="i.src" data-style-left="i.x - clip.x + 'px'"
   data-style-top="i.y - clip.y + 'px'" data-style-width="i.w + 'px'" data-style-height="i.h + 'px'"
   data-style-opacity="i.op"/></div>
 <span data-for="t : clipped_words" data-attr-class="t.cls" data-style-left="t.x - clip.x + 'px'"
   data-style-top="t.y - clip.y + 'px'" data-style-width="t.w + 'px'" data-style-height="t.h + 'px'"
   data-style-font-size="t.px + 'px'" data-style-color="t.colour" data-style-line-height="t.lh + 'px'"
   >{{t.text}}</span>
</div>
<div class="a" data-for="i : icons">
 <img class="a" data-attr-src="i.src" data-style-left="i.x + 'px'" data-style-top="i.y + 'px'"
  data-style-width="i.w + 'px'" data-style-height="i.h + 'px'" data-style-opacity="i.op"/></div>
<div class="a" data-for="b : over">
 <div data-if="!b.paint" data-attr-class="b.cls" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'"
   data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'"
   data-style-border-width="b.e + 'px'" data-style-opacity="b.op"></div>
 <div data-if="b.paint" class="paint" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'"
   data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'"
   data-style-border-width="b.e + 'px'" data-style-background-color="b.bg" data-style-border-color="b.bd"
   data-style-opacity="b.op"></div>
</div>
<span data-for="t : words" data-attr-class="t.cls" data-style-left="t.x + 'px'" data-style-top="t.y + 'px'"
  data-style-width="t.w + 'px'" data-style-height="t.h + 'px'"
  data-style-font-size="t.px + 'px'" data-style-color="t.colour" data-style-line-height="t.lh + 'px'"
  >{{t.text}}</span>
<div class="mark"></div>
<div class="a" data-for="b : top_boxes">
 <div class="paint" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'" data-style-width="b.w + 'px'"
   data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'" data-style-border-width="b.e + 'px'"
   data-style-background-color="b.bg" data-style-border-color="b.bd" data-style-opacity="b.op"></div>
</div>
<span data-for="t : top_words" data-attr-class="t.cls" data-style-left="t.x + 'px'" data-style-top="t.y + 'px'"
  data-style-width="t.w + 'px'" data-style-height="t.h + 'px'" data-style-font-size="t.px + 'px'"
  data-style-color="t.colour" data-style-line-height="t.lh + 'px'">{{t.text}}</span>
<div class="a" data-for="i : top">
 <img class="a" data-attr-src="i.src" data-style-left="i.x + 'px'" data-style-top="i.y + 'px'"
  data-style-width="i.w + 'px'" data-style-height="i.h + 'px'" data-style-opacity="i.op"/></div>
<div class="padfocus" data-if="pf.w > 0" data-style-left="pf.x + 'px'" data-style-top="pf.y + 'px'"
  data-style-width="pf.w + 'px'" data-style-height="pf.h + 'px'" data-style-border-radius="pf.r + 'px'"
  data-style-border-width="pf.e + 'px'"></div>
</body></rml>)RML";

}  // namespace

std::string Sheet::ground(uint32_t rgb, float opacity) {
    char b[16];
    std::snprintf(b, sizeof b, "#%06x%02x", unsigned(uaui::themed(rgb) & 0xFFFFFF),
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

void sayGdi(Content& c, const std::vector<Words>& in) {
    char colour[16];
    for (const Words& x : in) {
        std::snprintf(colour, sizeof colour, "#%06x", unsigned(uaui::themed(x.rgb) & 0xFFFFFF));
        std::string cls = x.bold ? "word bold" : "word";
        if (x.align & DT_RIGHT) cls += " right";
        else if (x.align & DT_CENTER) cls += " centre";
        if (x.wrap) cls += " wrap";
        if (x.halo) cls += " halo";
        c.words.push_back(Word{x.box.x, x.box.y, x.box.w, x.box.h, x.px,
                               x.wrap ? std::round(x.px * 1.3f) : x.box.h, utf8Of(x.text), colour, cls});
    }
}

void fromModel(Content& c, const sheet::Model& m, Sheet& sheet) {
    c.w = m.w, c.h = m.h;
    const auto boxes = [&](std::vector<Box>& to, const std::vector<sheet::Box>& in) {
        for (const sheet::Box& b : in) c.rect(to, b.box, b.r, b.fill, b.fillA, b.edge, b.edgeA, b.edgeW);
    };
    const auto icons = [&](std::vector<Icon>& to, const sheet::Layer& in) {
        for (const sheet::Art& a : in.arts) {  // 3.0.18-beta.41: a portrait covering its box, made once a size
            if (!a.art || a.art->width <= 0) continue;
            const int w = std::max(1, int(std::ceil(a.box.w))), h = std::max(1, int(std::ceil(a.box.h)));
            const std::string key = "art" + std::to_string(uintptr_t(a.art)) + ':' + std::to_string(w) + 'x' +
                                    std::to_string(h) + ':' + std::to_string(a.corners) + ':' +
                                    std::to_string(int(a.r * 10));
            to.push_back(Icon{a.box.x, a.box.y, float(w), float(h), 1, sheet.made(key, w, h, [&](panel::Picture& p) {
                                  classart::cover(p, panel::Box{0, 0, float(w), float(h)}, a.art->px, a.art->width,
                                                  a.art->height, a.r, a.corners);
                              })});
        }
        for (const sheet::Image& i : in.images)
            to.push_back(Icon{i.x, i.y, float(i.picture->width), float(i.picture->height), i.opacity,
                              sheet.icon(i.picture)});
        for (const sheet::Glyph& g : in.glyphs) {
            const int px = std::max(4, int(std::ceil(g.side)));
            const std::string key = "g" + std::to_string(int(g.icon)) + ':' + std::to_string(px) + ':' +
                                    std::to_string(g.rgb) + ':' + std::to_string(int(g.alpha * 100)) + ':' +
                                    std::to_string(int(g.stroke * 10));
            to.push_back(Icon{g.x, g.y, float(px), float(px), 1, sheet.made(key, px, px, [&](panel::Picture& p) {
                                  uiicons::draw(p, g.icon, 0, 0, float(px), g.rgb, g.alpha, g.stroke);
                              })});
        }
    };
    boxes(c.under, m.under.boxes);
    icons(c.icons, m.under);
    c.clip = Box{m.clip.x, m.clip.y, m.clip.w, m.clip.h, 0, 0, 1, "clip"};
    boxes(c.clipped, m.clipped.boxes);
    icons(c.clippedIcons, m.clipped);
    boxes(c.over, m.over.boxes);
    icons(c.top, m.over);
    char colour[16];
    const auto say = [&](std::vector<Word>& to, const std::vector<sheet::Text>& in) {
        for (const sheet::Text& t : in) {
            std::snprintf(colour, sizeof colour, "#%06x", unsigned(uaui::themed(t.rgb) & 0xFFFFFF));
            std::string cls = t.bold ? "word bold" : "word";
            if (t.align == sheet::Align::Right) cls += " right";
            else if (t.align == sheet::Align::Center) cls += " centre";
            if (t.wrap) cls += " wrap";
            to.push_back(Word{t.box.x, t.box.y, t.box.w, t.box.h, t.px, t.wrap ? std::round(t.px * 1.3f) : t.box.h,
                              t.text, colour, cls});
        }
    };
    say(c.clippedWords, m.clippedWords);
    for (const sheet::Text& t : m.words) {
        std::snprintf(colour, sizeof colour, "#%06x", unsigned(uaui::themed(t.rgb) & 0xFFFFFF));
        std::string cls = t.bold ? "word bold" : "word";
        if (t.align == sheet::Align::Right) cls += " right";
        else if (t.align == sheet::Align::Center) cls += " centre";
        if (t.wrap) cls += " wrap";
        c.words.push_back(Word{t.box.x, t.box.y, t.box.w, t.box.h, t.px, t.wrap ? std::round(t.px * 1.3f) : t.box.h,
                               t.text, colour, cls});
    }
}

void Sheet::glyph(Content& c, int button, bool sony, float x, float y, float h) {
    const padglyph::Button b = padglyph::Button(button);
    const padglyph::Family f = sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
    const int pw = std::max(1, int(std::ceil(padglyph::widthOf(b, h)))), ph = std::max(1, int(std::ceil(h)));
    const std::string key = "pad" + std::to_string(button) + (sony ? "s" : "x") + std::to_string(ph);
    c.icons.push_back(Icon{x, y, float(pw), float(ph), 1,
                           made(key, pw, ph, [&](panel::Picture& p) { padglyph::paint(p, b, f, 0, 0, h); })});
}

// 3.0.18-beta.57 (docs/superpowers/plans/2026-11-10-uaui-possibilities.md, stage 1: «текст всегда влезает»): a line
// wider than its box as uaUI draws it shrinks to fit, down to 3/4 of its size (told once a text)
static void fitWords(std::vector<Word>& words) {
    static std::set<std::string> told;
    for (Word& t : words) {
        if (t.w <= 0 || t.text.empty() || t.cls.find("wrap") != std::string::npos ||
            t.text.find('\n') != std::string::npos)
            continue;
        const bool bold = t.cls.find("bold") != std::string::npos;
        const float wide = uaui::textWidth(t.text, t.px, bold);
        if (wide <= t.w) continue;
        const float px = std::max(t.px * 0.75f, t.px * t.w / wide);
        if (told.size() < 200 && told.insert(t.text).second)
            LOG_INFO("uaui: \"%s\" %.0f px wide in a %.0f px box - drawn at %.1f px, not %.1f", t.text.c_str(),
                     double(wide), double(t.w), double(px), double(t.px));
        t.px = px;
    }
}

bool Sheet::frame(const char* name, bool on, bool window, int x, int y, int gen,
                  const std::function<void(Content&)>& fill) {
    if (!(anywhere ? uaui::readyAnywhere() : uaui::ready()) || !on) {
        doc_.show(false);
        return false;
    }
    if (!doc_.made()) {
        doc_.passive = true;
        doc_.window = window;
        doc_.anywhere = anywhere;
        doc_.world = world;
        doc_.top = top;
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
                    b.RegisterMember("paint", &Box::paint);
                    b.RegisterMember("bg", &Box::bg);
                    b.RegisterMember("bd", &Box::bd);
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
                    t.RegisterMember("lh", &Word::lh);
                    t.RegisterMember("text", &Word::text);
                    t.RegisterMember("colour", &Word::colour);
                    t.RegisterMember("cls", &Word::cls);
                }
                if (auto r = m.RegisterStruct<Ring>()) {
                    r.RegisterMember("x", &Ring::x);
                    r.RegisterMember("y", &Ring::y);
                    r.RegisterMember("size", &Ring::size);
                    r.RegisterMember("value", &Ring::value);
                    r.RegisterMember("start", &Ring::start);
                    r.RegisterMember("width", &Ring::width);
                    r.RegisterMember("sector", &Ring::sector);
                    r.RegisterMember("fill", &Ring::fill);
                    r.RegisterMember("track", &Ring::track);
                }
                m.RegisterArray<std::vector<Ring>>();
                m.RegisterArray<std::vector<Box>>();
                m.RegisterArray<std::vector<Icon>>();
                m.RegisterArray<std::vector<Word>>();
                m.Bind("w", &c_.w);
                m.Bind("h", &c_.h);
                m.Bind("op", &c_.op);
                m.Bind("glass", &c_.glass);
                m.Bind("ground", &c_.ground);
                m.Bind("under", &c_.under);
                m.Bind("icons", &c_.icons);
                m.Bind("over", &c_.over);
                m.Bind("words", &c_.words);
                m.Bind("rings", &c_.rings);
                m.Bind("top", &c_.top);
                m.Bind("top_boxes", &c_.topBoxes);
                m.Bind("top_words", &c_.topWords);
                m.Bind("clip", &c_.clip);
                m.Bind("clipped", &c_.clipped);
                m.Bind("clipped_icons", &c_.clippedIcons);
                m.Bind("clipped_words", &c_.clippedWords);
                m.Bind("pf", &pf_);  // 3.0.18-beta.60: the pad's ring
            }))
            return false;
    }
    if (gen != gen_) {
        gen_ = gen;
        Content next;
        next.ground = "#10141cff";
        fill(next);
        fitWords(next.words);
        fitWords(next.clippedWords);
        fitWords(next.topWords);
        // 3.0.18-beta.70 (the big six, C): only the lists that changed evaluated again (a sheet said anew each
        // frame - the monsters' bars, the carried thing - mostly says the same)
        const auto changed = [&](const char* name, auto& was, auto& now) {
            if (was == now) return;
            was = std::move(now);
            doc_.dirty(name);
        };
        changed("w", c_.w, next.w);
        changed("h", c_.h, next.h);
        changed("op", c_.op, next.op);
        changed("glass", c_.glass, next.glass);
        changed("ground", c_.ground, next.ground);
        changed("under", c_.under, next.under);
        changed("icons", c_.icons, next.icons);
        changed("over", c_.over, next.over);
        changed("words", c_.words, next.words);
        changed("rings", c_.rings, next.rings);
        changed("top", c_.top, next.top);
        changed("top_boxes", c_.topBoxes, next.topBoxes);
        changed("top_words", c_.topWords, next.topWords);
        changed("clip", c_.clip, next.clip);
        changed("clipped", c_.clipped, next.clipped);
        changed("clipped_icons", c_.clippedIcons, next.clippedIcons);
        changed("clipped_words", c_.clippedWords, next.clippedWords);
        c_.opacity = next.opacity;
    }
    x_ = x, y_ = y;
    doc_.place(x, y);
    doc_.show(true);
    return true;
}

namespace {
std::map<int, Sheet*>& navs() {
    static std::map<int, Sheet*> m;
    return m;
}
}  // namespace

void Sheet::nav(int kind, std::vector<panel::Box> targets) {
    navs()[kind] = this;
    if (targets.size() != targets_.size()) focus_ = -1;  // another set: the ring comes back on the next press
    targets_.clear();
    for (const panel::Box& b : targets)
        if (b.w > 0 && b.h > 0) targets_.push_back(b);
    if (focus_ >= int(targets_.size())) focus_ = -1;
    showFocus();
}

void Sheet::showFocus() {
    Box f;
    if (focus_ >= 0 && focus_ < int(targets_.size())) {
        const panel::Box& b = targets_[size_t(focus_)];
        const float d = 3 * uaui::dp();
        f = Box{b.x - d, b.y - d, b.w + 2 * d, b.h + 2 * d, 6 * uaui::dp(), 2 * uaui::dp(), 1, "padfocus"};
    }
    if (f.x != pf_.x || f.y != pf_.y || f.w != pf_.w || f.h != pf_.h) {
        pf_ = f;
        doc_.dirty("pf");
    }
}

bool hasNav(int kind) {
    const auto it = navs().find(kind);
    return it != navs().end() && it->second->doc().shown() && !it->second->targets_.empty();
}

bool padNav(int kind, NavCommand c) {
    if (!hasNav(kind)) return false;
    Sheet& s = *navs()[kind];
    if (c == NavCommand::Press) {
        if (s.focus_ < 0) {
            s.focus_ = padnav::first(s.targets_);
            s.showFocus();
            return true;
        }
        const panel::Box& b = s.targets_[size_t(s.focus_)];
        const double k = uaui::targetScale() > 0 ? uaui::targetScale() : 1;
        const int gx = int((s.x_ + b.x + b.w / 2) / k), gy = int((s.y_ + b.y + b.h / 2) / k);
        const LPARAM lp = MAKELPARAM(gx, gy);
        LRESULT r = 0;
        HWND game = uaui::gameWindow();
        windowstack::mouse(game, WM_MOUSEMOVE, 0, lp, r);
        windowstack::mouse(game, WM_LBUTTONDOWN, MK_LBUTTON, lp, r);
        windowstack::mouse(game, WM_LBUTTONUP, 0, lp, r);
        LOG_INFO("pad: A on box %d of window %d at %d, %d", s.focus_, kind, gx, gy);
        return true;
    }
    const padnav::Dir d = c == NavCommand::Up     ? padnav::Dir::Up
                          : c == NavCommand::Down ? padnav::Dir::Down
                          : c == NavCommand::Left ? padnav::Dir::Left
                                                  : padnav::Dir::Right;
    s.focus_ = padnav::next(s.targets_, s.focus_, d);
    s.showFocus();
    return true;
}

}  // namespace uagfx::win::uisheet
