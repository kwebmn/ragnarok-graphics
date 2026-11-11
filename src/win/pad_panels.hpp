#pragma once
#include <functional>
#include <vector>

#include "core/closed_windows.hpp"
#include "core/window_panel.hpp"

namespace uagfx::win::padpanels {

// 3.0.18-beta.135 (the pad analysis: our panels on the controller): the Start tile of an open panel (hunt,
// boss timers, tracker, recap) gives the controller its buttons - the D-pad walks them (a gold ring), A presses
// one, B leaves. A panel's buttons: their boxes in the target's pixels, what a press does.
struct Button {
    panel::Box box;
    std::function<void()> press;
};
bool focus(closedwin::Own o);  // false: that panel shows no buttons now
bool active();
enum class Command : uint8_t { Prev, Next, Press, Leave };
void command(Command c);
void frame();  // each frame: the buttons again, the ring drawn (left when the panel is gone)

}  // namespace uagfx::win::padpanels
