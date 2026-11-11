#include "uikit/hits.hpp"

#include <RmlUi/Core.h>

#include <cstdlib>

namespace uagfx::uikit {
namespace {

panel::Box boxOf(Rml::ElementDocument* doc, Rml::Element* e) {
    const Rml::Vector2f origin = doc->GetAbsoluteOffset(Rml::BoxArea::Border);
    const Rml::Vector2f at = e->GetAbsoluteOffset(Rml::BoxArea::Border) - origin;
    const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Border);
    return panel::Box{at.x, at.y, size.x, size.y};
}

// 3.0.18-beta.107: shown - itself and every parent up to the document laid out (a data-if's display none above it
// leaves it at 0, 0 with no size; the login's refusal line under the other field)
bool shown(Rml::ElementDocument* doc, Rml::Element* e) {
    for (Rml::Element* p = e; p && p != doc; p = p->GetParentNode())
        if (!p->IsVisible() || p->GetComputedValues().display() == Rml::Style::Display::None) return false;
    return true;
}

int indexOf(Rml::Element* e) {
    const Rml::Variant* v = e->GetAttribute("data-index");
    return v ? v->Get<int>(-1) : -1;
}

}  // namespace

std::vector<Placed> placesOf(Rml::ElementDocument* doc, const char* part) {
    std::vector<Placed> out;
    if (!doc) return out;
    Rml::ElementList found;
    doc->QuerySelectorAll(found, std::string("[data-hit=") + part + "]");
    for (Rml::Element* e : found)
        if (shown(doc, e)) out.push_back(Placed{indexOf(e), boxOf(doc, e)});
    return out;
}

}  // namespace uagfx::uikit
