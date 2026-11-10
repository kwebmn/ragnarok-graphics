#include "uikit/layered.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "uikit/hits.hpp"

namespace uagfx::uikit::layered {
namespace {

const char* kRml = R"RML(<div class="lay" data-hit="body" data-style-width="layer.w + 'px'"
  data-style-height="layer.h + 'px'"><div class="lb" data-for="b : layer.boxes" data-style-left="b.x + 'px'"
  data-style-top="b.y + 'px'" data-style-width="b.w + 'px'" data-style-height="b.h + 'px'"
  data-style-border-radius="b.r + 'px'" data-style-border-width="b.e + 'px'" data-style-border-color="b.edge"
  data-style-background-color="b.fill"></div><div class="lh" data-for="h : layer.hits" data-attr-data-hit="h.part"
  data-attr-data-index="h.index" data-style-left="h.x + 'px'" data-style-top="h.y + 'px'"
  data-style-width="h.w + 'px'" data-style-height="h.h + 'px'"></div><span data-for="x : layer.words"
  data-attr-class="x.cls" data-style-left="x.x + 'px'" data-style-top="x.y + 'px'" data-style-width="x.w + 'px'"
  data-style-height="x.h + 'px'" data-style-line-height="x.h + 'px'" data-style-font-size="x.px + 'px'"
  data-style-color="x.colour">{{x.text}}</span></div>)RML";

}  // namespace

const char* rml() { return kRml; }

void bind(Rml::DataModelConstructor& c, Layer& l) {
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
    if (auto e = c.RegisterStruct<HitV>()) {
        e.RegisterMember("x", &HitV::x);
        e.RegisterMember("y", &HitV::y);
        e.RegisterMember("w", &HitV::w);
        e.RegisterMember("h", &HitV::h);
        e.RegisterMember("index", &HitV::index);
        e.RegisterMember("part", &HitV::part);
    }
    c.RegisterArray<std::vector<WordV>>();
    c.RegisterArray<std::vector<BoxV>>();
    c.RegisterArray<std::vector<HitV>>();
    if (auto e = c.RegisterStruct<Layer>()) {
        e.RegisterMember("w", &Layer::w);
        e.RegisterMember("h", &Layer::h);
        e.RegisterMember("boxes", &Layer::boxes);
        e.RegisterMember("hits", &Layer::hits);
        e.RegisterMember("words", &Layer::words);
    }
    c.Bind("layer", &l);
}

std::string hexa(uint32_t rgb, float alpha) {
    char b[16];
    std::snprintf(b, sizeof b, "#%06x%02x", unsigned(rgb & 0xFFFFFF),
                  unsigned(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255)) & 0xFF);
    return b;
}

Builder::Builder(Layer& l, const panel::Box& o)
    : layer(l), origin(o), hadBoxes(l.boxes.size()), hadHits(l.hits.size()), hadWords(l.words.size()) {
    l.boxes.clear();
    l.hits.clear();
    l.words.clear();
}

Builder::~Builder() {
    if (layer.boxes.size() < hadBoxes) layer.boxes.resize(hadBoxes);
    if (layer.hits.size() < hadHits) layer.hits.resize(hadHits, HitV{0, 0, 0, 0, -1, "none"});
    if (layer.words.size() < hadWords) layer.words.resize(hadWords);
}

void Builder::box(const panel::Box& b, float radius, uint32_t fill, float fillA, uint32_t edge, float edgeA,
                  float edgeW) {
    if (b.w <= 0 || b.h <= 0) return;
    layer.boxes.push_back(BoxV{b.x - origin.x, b.y - origin.y, b.w, b.h, radius, edgeW, hexa(fill, fillA),
                               hexa(edge, edgeA)});
}

void Builder::hit(const char* part, int index, const panel::Box& b) {
    if (b.w <= 0 || b.h <= 0) return;
    layer.hits.push_back(HitV{b.x - origin.x, b.y - origin.y, b.w, b.h, index, part});
}

void Builder::word(const panel::Box& b, const std::string& text, uint32_t rgb, float px, bool bold, Align a) {
    if (text.empty()) return;
    const char* align = a == Align::Right ? " right" : a == Align::Center ? " centre" : "";
    layer.words.push_back(WordV{b.x - origin.x, b.y - origin.y, b.w, b.h, px, text, hexa(rgb, 1),
                                std::string(bold ? "w bold" : "w") + align});
}

bool bodyShift(Rml::ElementDocument* doc, const panel::Box& coreOrigin, float& dx, float& dy) {
    const std::vector<Placed> body = placesOf(doc, "body");
    if (body.empty()) return false;
    dx = body.front().box.x - coreOrigin.x;
    dy = body.front().box.y - coreOrigin.y;
    return true;
}

}  // namespace uagfx::uikit::layered
