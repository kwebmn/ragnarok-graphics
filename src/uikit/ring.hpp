#pragma once
#include <RmlUi/Core.h>

#include "core/window_panel.hpp"

namespace uagfx::uikit {

// 3.0.21-beta.9 (R-MSAM96's log: «Налаштування» open - the driver's own work 8 ms a frame against 0.9 before it):
// the pad's gold ring of a live window - the element «#ring» of its markup - shown round a control or hidden. Its
// keeper is asked every frame; it sets the element's properties only when the ring shows, hides or moves. RmlUi lays
// the whole document out again when a property that can change the layout is set, even to the value it has: hidden
// anew each frame, the ring cost a layout of the window each frame.
struct Ring {
    // Round `box` grown by `grow` each way (in the document's pixels); null - hidden. True: the element was set
    bool place(Rml::Element* ring, const panel::Box* box, float grow);

private:
    Rml::Element* on_ = nullptr;  // another element (the document made again): placed anew
    bool shown_ = false;
    float x_ = 0, y_ = 0, w_ = 0, h_ = 0;
};

}  // namespace uagfx::uikit
