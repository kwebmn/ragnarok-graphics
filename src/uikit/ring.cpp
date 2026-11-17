#include "uikit/ring.hpp"

#include <string>

namespace uagfx::uikit {

bool Ring::place(Rml::Element* ring, const panel::Box* box, float grow) {
    if (!ring) return false;
    const bool shown = box != nullptr;
    const float x = shown ? box->x - grow : 0, y = shown ? box->y - grow : 0;
    const float w = shown ? box->w + 2 * grow : 0, h = shown ? box->h + 2 * grow : 0;
    if (ring == on_ && shown == shown_ && (!shown || (x == x_ && y == y_ && w == w_ && h == h_))) return false;
    const bool show = ring != on_ || shown != shown_;
    on_ = ring, shown_ = shown, x_ = x, y_ = y, w_ = w, h_ = h;
    if (show) ring->SetProperty("display", shown ? "block" : "none");
    if (!shown) return true;
    ring->SetProperty("left", std::to_string(x) + "px");
    ring->SetProperty("top", std::to_string(y) + "px");
    ring->SetProperty("width", std::to_string(w) + "px");
    ring->SetProperty("height", std::to_string(h) + "px");
    return true;
}

}  // namespace uagfx::uikit
