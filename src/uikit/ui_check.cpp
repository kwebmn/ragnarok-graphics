#include "uikit/ui_check.hpp"

#include <algorithm>
#include <cmath>

#include <RmlUi/Core.h>

namespace uagfx::uikit::uicheck {
namespace {

float channel(float c) { return c <= 0.03928f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
float luminance(Rgbf c) { return 0.2126f * channel(c.r) + 0.7152f * channel(c.g) + 0.0722f * channel(c.b); }

Rgbf over(Rgbf under, const Rml::Colourb& c) {
    const float a = c.alpha / 255.0f;
    return {under.r + (c.red / 255.0f - under.r) * a, under.g + (c.green / 255.0f - under.g) * a,
            under.b + (c.blue / 255.0f - under.b) * a};
}

unsigned hex(Rgbf c) {
    const auto b = [](float v) { return unsigned(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    return b(c.r) << 16 | b(c.g) << 8 | b(c.b);
}

// A box drawn with a ground: its place on the screen, its colour, its turn in the document (drawn in that order)
struct Painted {
    float x = 0, y = 0, w = 0, h = 0;
    Rml::Colourb colour;
    int order = 0;
};

// The grounds drawn before the word (its order) under its middle - its parents' and the boxes beside it alike (the
// live sheets' grounds are boxes of their own), laid over `world`
Rgbf groundUnder(const std::vector<Painted>& painted, int order, float cx, float cy, Rgbf world) {
    Rgbf c = world;
    for (const Painted& p : painted)
        if (p.order < order && cx >= p.x && cx < p.x + p.w && cy >= p.y && cy < p.y + p.h) c = over(c, p.colour);
    return c;
}

void paintedOf(Rml::Element* e, std::vector<Painted>& out, int& order) {
    for (int i = 0; i < e->GetNumChildren(true); ++i) {
        Rml::Element* c = e->GetChild(i);
        ++order;
        if (!rmlui_dynamic_cast<Rml::ElementText*>(c) && c->IsVisible(true)) {
            const Rml::Colourb g = c->GetComputedValues().background_color();
            if (g.alpha > 0) {
                const Rml::Vector2f at = c->GetAbsoluteOffset(Rml::BoxArea::Border);
                const Rml::Vector2f size = c->GetBox().GetSize(Rml::BoxArea::Border);
                out.push_back(Painted{at.x, at.y, size.x, size.y, g, order});
            }
        }
        paintedOf(c, out, order);
    }
}

// Its box clips what it holds: wider than the box, the box's overflow not shown
bool clipped(Rml::Element* e) {
    for (Rml::Element* p = e; p; p = p->GetParentNode()) {
        const Rml::Style::ComputedValues& v = p->GetComputedValues();
        if (v.overflow_x() != Rml::Style::Overflow::Visible) return p->GetScrollWidth() > p->GetClientWidth() + 1.5f;
        if (p == e->GetOwnerDocument()) break;
    }
    return false;
}

void walk(Rml::Element* e, Rgbf world, const std::vector<Painted>& painted, int& order, std::vector<Word>& out) {
    for (int i = 0; i < e->GetNumChildren(true); ++i) {
        Rml::Element* c = e->GetChild(i);
        ++order;
        if (auto* t = rmlui_dynamic_cast<Rml::ElementText*>(c)) {
            const std::string text = t->GetText();
            if (text.find_first_not_of(" \n\t") == std::string::npos || !e->IsVisible(true)) continue;
            const Rml::Vector2f mid = e->GetAbsoluteOffset(Rml::BoxArea::Border) +
                                      e->GetBox().GetSize(Rml::BoxArea::Border) * 0.5f;
            const Rgbf g = groundUnder(painted, order, mid.x, mid.y, world);
            const Rgbf f = over(g, e->GetComputedValues().color());
            const float l1 = luminance(f), l2 = luminance(g);
            Word w;
            size_t cut = std::min<size_t>(text.size(), 60);  // at a letter's edge: whole UTF-8
            while (cut > 0 && cut < text.size() && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;
            w.text = text.substr(0, cut);
            const Rml::Vector2f at = e->GetAbsoluteOffset(Rml::BoxArea::Border);
            const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Border);
            w.x = at.x, w.y = at.y, w.w = size.x, w.h = size.y;
            w.colour = hex(f), w.ground = hex(g);
            w.contrast = (std::max(l1, l2) + 0.05f) / (std::min(l1, l2) + 0.05f);
            w.cut = clipped(e);
            out.push_back(std::move(w));
            continue;
        }
        walk(c, world, painted, order, out);
    }
}

}  // namespace

std::vector<Word> words(Rml::ElementDocument* doc, Rgbf world) {
    std::vector<Word> out;
    if (!doc || !doc->IsVisible()) return out;
    std::vector<Painted> painted;
    int order = 0;
    const Rml::Colourb body = doc->GetComputedValues().background_color();
    if (body.alpha > 0) {
        const Rml::Vector2f at = doc->GetAbsoluteOffset(Rml::BoxArea::Border);
        const Rml::Vector2f size = doc->GetBox().GetSize(Rml::BoxArea::Border);
        painted.push_back(Painted{at.x, at.y, size.x, size.y, body, 0});
    }
    paintedOf(doc, painted, order);
    order = 0;
    walk(doc, world, painted, order, out);
    return out;
}

std::vector<Word> problems(Rml::ElementDocument* doc, Rgbf world) {
    std::vector<Word> out;
    for (Word& w : words(doc, world))
        if (w.contrast < kLeastContrast || w.cut) out.push_back(std::move(w));
    return out;
}

}  // namespace uagfx::uikit::uicheck
