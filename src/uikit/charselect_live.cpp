#include "uikit/charselect_live.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "uikit/hits.hpp"

namespace uagfx::uikit::charpick {
namespace {

namespace cs = uagfx::charselect;
namespace ly = uagfx::uikit::layered;

std::string markup() {
    std::string s = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw csl" data-style-font-size="base + 'px'" data-style-width="width + 'px'"
  data-style-background-color="ground">
<div class="head" data-hit="title"><span class="title">{{title}}</span><span class="gap"></span><span class="pa"
  data-if="pages" data-hit="pageleft">&#x2039;</span><span class="pg" data-hit="page">{{page}}</span><span class="pa"
  data-if="pages" data-hit="pageright">&#x203A;</span></div>
)RML";
    s += ly::rml();
    s += R"RML(
<div class="cbtns"><div class="csb" data-for="b, i : buttons" data-hit="button" data-attr-data-index="i"
  data-class-off="!b.can" data-class-main="b.primary &amp;&amp; b.can" data-class-hot="b.hot">{{b.label}}</div></div>
</body></rml>)RML";
    size_t at;
    while ((at = s.find("&amp;&amp;")) != std::string::npos) s.replace(at, 10, "&&");
    return s;
}

float bodyTop(const cs::Layout& L) { return L.title.y + L.title.h + 8 * L.s; }

bool inside(const panel::Box& o, const panel::Box& b) {
    const float cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    return o.w > 0 && cx >= o.x && cx <= o.x + o.w && cy >= o.y && cy <= o.y + o.h;
}

}  // namespace

const char* rml() {
    static const std::string s = markup();
    return s.c_str();
}

void bind(Rml::DataModelConstructor& c, Model& m) {
    if (auto e = c.RegisterStruct<ButtonV>()) {
        e.RegisterMember("label", &ButtonV::label);
        e.RegisterMember("can", &ButtonV::can);
        e.RegisterMember("primary", &ButtonV::primary);
        e.RegisterMember("hot", &ButtonV::hot);
    }
    c.RegisterArray<std::vector<ButtonV>>();
    c.Bind("base", &m.base);
    c.Bind("width", &m.width);
    c.Bind("ground", &m.ground);
    c.Bind("title", &m.title);
    c.Bind("page", &m.page);
    c.Bind("pages", &m.pages);
    c.Bind("buttons", &m.buttons);
    ly::bind(c, m.layer);
}

void fill(Model& m, const cs::Layout& L, const std::vector<cs::Slot>& slots, const cs::Show& sh, const cs::Texts& t,
          const Source& src) {
    const float sc = L.s > 0 ? L.s : 1;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kHp = 0x4CC777, kSp = 0x3D7EEB;
    m.base = 12 * sc;
    m.width = float(L.wide > 0 ? L.wide : L.w);  // 3.0.19-beta.1: its size's, never refit's old one
    m.title = t.title;
    m.page = std::to_string(sh.page + 1) + " / " + std::to_string(std::max(1, sh.pages));
    m.pages = !L.carousel;
    const cs::Slot* chosen =
        sh.selected >= 0 && sh.selected < int(slots.size()) ? &slots[size_t(sh.selected)] : nullptr;
    const std::string* names[4] = {&t.enter, &t.make, &t.remove, &t.back};
    m.buttons.resize(4);
    for (int i = 0; i < 4; ++i) {
        const cs::Button b = cs::Button(i);
        ButtonV& v = m.buttons[size_t(i)];
        v.label = *names[i];
        v.can = chosen ? cs::allowed(*chosen, b) : b == cs::Button::Back;
        v.primary = b == cs::Button::Enter || (b == cs::Button::Make && chosen && !chosen->used);
        v.hot = i == sh.hoverButton;
    }
    const float top = bodyTop(L);
    m.layer.w = float(L.w);
    {
        ly::Builder b(m.layer, panel::Box{0, top, 0, 0});
        if (L.carousel)  // the carousel's arrows at its sides
            for (int i = 0; i < 2; ++i) {
                const panel::Box& a = i ? L.pageRight : L.pageLeft;
                b.box(a, 7 * sc, kWhite, 0.06f);
                b.hit(i ? "pageright" : "pageleft", -1, a);
            }
        const std::vector<size_t> order =
            L.carousel ? std::vector<size_t>{0, 4, 1, 3, 2} : std::vector<size_t>{0, 1, 2};
        for (const size_t i : order) {  // the farther first, the chosen over them
            if (i >= L.slots.size()) continue;
            const panel::Box& c = L.slots[i];
            const cs::Slot* sl = i < slots.size() ? &slots[i] : nullptr;
            if (sl && sl->none) continue;
            const bool on = int(i) == sh.selected, hot = int(i) == sh.hover;
            const float f = L.carousel ? L.scales[i] : 1.0f;
            b.box(c, 12 * sc * f, L.carousel ? kCard : kWhite,
                  L.carousel ? (f < 0.8f ? 0.82f : 0.92f) : hot ? 0.07f : 0.04f, on ? kGold : kWhite,
                  on ? 1.0f : hot ? 0.24f : 0.12f, (on ? 2.0f : 1.0f) * sc);
            b.hit("slot", int(i), c);
            if (!sl || !sl->used) continue;
            const cs::CardParts in = cs::cardParts(c, sc * f);
            b.box(in.face, in.face.w / 2, 0x000000, 0.30f);
            if (sl->face && sl->face->width > 0)
                b.icon(panel::Box{std::round(in.face.x + (in.face.w - float(sl->face->width)) / 2),
                                  std::round(in.face.y + (in.face.h - float(sl->face->height)) / 2),
                                  float(sl->face->width), float(sl->face->height)},
                       src(sl->face));
            const auto bar = [&](const panel::Box& bb, int now, int most, uint32_t rgb) {
                b.box(bb, bb.h / 2, kWhite, 0.08f);
                if (most > 0 && now > 0)
                    b.box(panel::Box{bb.x, bb.y, bb.w * std::min(1.0f, float(now) / float(most)), bb.h}, bb.h / 2,
                          rgb, 0.9f);
            };
            bar(in.hpBar, sl->hp, sl->maxHp, kHp);
            bar(in.spBar, sl->sp, sl->maxSp, kSp);
        }
        b.hit("summary", -1, L.summary);  // its row kept whether it says anything or not (the buttons never jump)
        for (const cs::Words& x : cs::words(L, slots, sh, t)) {  // the head's and the buttons' words their own
            if (x.box.y + x.box.h / 2 < top) continue;
            bool own = false;
            for (const panel::Box& bt : L.buttons) own = own || inside(bt, x.box);
            if (own) continue;
            b.word(x.box, x.text, x.rgb, x.px, x.bold,
                   x.align == cs::Align::Right    ? ly::Align::Right
                   : x.align == cs::Align::Center ? ly::Align::Center
                                                  : ly::Align::Left);
        }
    }
    m.layer.h = ly::contentHeight(m.layer, 6 * sc);
}

bool refit(Rml::ElementDocument* doc, cs::Layout& L) {
    if (!doc || !doc->IsVisible()) return false;
    const Rml::Vector2f size = doc->GetBox().GetSize(Rml::BoxArea::Border);
    const std::vector<Placed> buttons = placesOf(doc, "button");
    if (size.x <= 0 || size.y <= 0 || buttons.size() != L.buttons.size()) return false;
    float dx = 0, dy = 0;
    if (!layered::bodyShift(doc, panel::Box{0, bodyTop(L), 0, 0}, dx, dy)) return false;
    const auto shift = [&](panel::Box& b) {
        if (b.w > 0) b.x += dx, b.y += dy;
    };
    for (panel::Box& b : L.slots) shift(b);
    shift(L.summary);
    if (L.carousel) {
        shift(L.pageLeft);
        shift(L.pageRight);
    } else {
        const std::vector<Placed> left = placesOf(doc, "pageleft"), right = placesOf(doc, "pageright");
        L.pageLeft = left.empty() ? panel::Box{} : left.front().box;
        L.pageRight = right.empty() ? panel::Box{} : right.front().box;
    }
    for (size_t i = 0; i < buttons.size(); ++i) L.buttons[i] = buttons[i].box;
    const std::vector<Placed> head = placesOf(doc, "title"), page = placesOf(doc, "page");
    if (!head.empty()) L.title = head.front().box;
    if (!page.empty()) L.page = page.front().box;
    L.glass = panel::Box{0, 0, size.x, size.y};
    L.w = int(std::ceil(size.x));
    L.h = int(std::ceil(size.y));
    return true;
}

}  // namespace uagfx::uikit::charpick
