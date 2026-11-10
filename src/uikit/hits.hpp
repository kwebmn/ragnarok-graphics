#pragma once
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace Rml {
class Element;
class ElementDocument;
}  // namespace Rml

namespace uagfx::uikit {

// 3.0.18-beta.94 (the drop protocol of the live windows with drag and drop): what is under a point of a live
// window - the element RmlUi laid out there (its own z-order, its visibility, pointer-events), walked up to the first
// one with «data-hit»: its word and its «data-index». The windows' clicks, drags and drops ask this in place of
// their core layouts' hit tests, so a press lands where the cell is drawn. No Windows headers: the host tests lay the
// markup out and ask it too.
struct Hit {
    std::string part;  // "" nothing of the window's
    int index = -1;
    panel::Box box;    // the element's, in the document's pixels
    bool inside = false;  // over the document at all (its ground counts)
};
// (x, y) in the document's own pixels (its top left 0, 0)
Hit hitAt(Rml::ElementDocument* doc, float x, float y);
// Every element with data-hit = `part` (visible), by its data-index: their boxes in the document's pixels
struct Placed {
    int index = -1;
    panel::Box box;
};
std::vector<Placed> placesOf(Rml::ElementDocument* doc, const char* part);

}  // namespace uagfx::uikit
