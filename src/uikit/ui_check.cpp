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

// The grounds from the document's body down to `e`, laid over `world`
Rgbf groundUnder(Rml::Element* e, Rgbf world) {
    std::vector<Rml::Element*> chain;
    for (Rml::Element* p = e; p; p = p->GetParentNode()) chain.push_back(p);
    Rgbf c = world;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) c = over(c, (*it)->GetComputedValues().background_color());
    return c;
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

void walk(Rml::Element* e, Rgbf world, std::vector<Word>& out) {
    for (int i = 0; i < e->GetNumChildren(true); ++i) {
        Rml::Element* c = e->GetChild(i);
        if (auto* t = rmlui_dynamic_cast<Rml::ElementText*>(c)) {
            const std::string text = t->GetText();
            if (text.find_first_not_of(" \n\t") == std::string::npos || !e->IsVisible(true)) continue;
            const Rgbf g = groundUnder(e, world);
            const Rgbf f = over(g, e->GetComputedValues().color());
            const float l1 = luminance(f), l2 = luminance(g);
            Word w;
            w.text = text.substr(0, 60);
            const Rml::Vector2f at = e->GetAbsoluteOffset(Rml::BoxArea::Border);
            const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Border);
            w.x = at.x, w.y = at.y, w.w = size.x, w.h = size.y;
            w.colour = hex(f), w.ground = hex(g);
            w.contrast = (std::max(l1, l2) + 0.05f) / (std::min(l1, l2) + 0.05f);
            w.cut = clipped(e);
            out.push_back(std::move(w));
            continue;
        }
        walk(c, world, out);
    }
}

}  // namespace

std::vector<Word> words(Rml::ElementDocument* doc, Rgbf world) {
    std::vector<Word> out;
    if (doc && doc->IsVisible()) walk(doc, world, out);
    return out;
}

std::vector<Word> problems(Rml::ElementDocument* doc, Rgbf world) {
    std::vector<Word> out;
    for (Word& w : words(doc, world))
        if (w.contrast < kLeastContrast || w.cut) out.push_back(std::move(w));
    return out;
}

}  // namespace uagfx::uikit::uicheck
