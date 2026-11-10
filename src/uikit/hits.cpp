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

int indexOf(Rml::Element* e) {
    const Rml::Variant* v = e->GetAttribute("data-index");
    return v ? v->Get<int>(-1) : -1;
}

}  // namespace

Hit hitAt(Rml::ElementDocument* doc, float x, float y) {
    Hit h;
    if (!doc || !doc->IsVisible() || !doc->GetContext()) return h;
    const Rml::Vector2f origin = doc->GetAbsoluteOffset(Rml::BoxArea::Border);
    Rml::Element* e = doc->GetContext()->GetElementAtPoint(Rml::Vector2f(x, y) + origin, nullptr, doc);
    if (!e) return h;
    h.inside = true;
    for (; e && e != doc; e = e->GetParentNode()) {
        const Rml::Variant* part = e->GetAttribute("data-hit");
        if (!part) continue;
        h.part = part->Get<Rml::String>();
        h.index = indexOf(e);
        h.box = boxOf(doc, e);
        return h;
    }
    return h;
}

std::vector<Placed> placesOf(Rml::ElementDocument* doc, const char* part) {
    std::vector<Placed> out;
    if (!doc) return out;
    Rml::ElementList found;
    doc->QuerySelectorAll(found, std::string("[data-hit=") + part + "]");
    for (Rml::Element* e : found)
        if (e->IsVisible()) out.push_back(Placed{indexOf(e), boxOf(doc, e)});
    return out;
}

}  // namespace uagfx::uikit
