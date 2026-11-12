#pragma once
#include <string>

#include "core/window_panel.hpp"

namespace uagfx::loadingbar {

// 3.0.18-beta.180 (the owner «давай» to «what else»: the old window the game still draws most - in 57 of 70 reports,
// at every map change): uaRO.exe's map loading bar, its window 7 (FUN_0046b440: 240 x 15 under the loading picture,
// its bar FUN_00449450 - a yellow frame, a grey track, the fill and «%3ld%%» in the game's small font) in the new
// look at its place: the game's own per cent, the words sharp. The parts that need no Windows (win/loading_bar).
struct Layout {
    panel::Box track, fill;  // in the target's pixels
    std::string percent;     // «45%» - the game's own: its whole per cent
    float px = 12;           // the per cent's size
    float radius = 0, edge = 1;
};
// The game's bar at (x, y), w x h of its pixels; `k` the target's pixels a game's one (none: 1); `progress` 0-100
Layout layout(int x, int y, int w, int h, double k, float progress);

}  // namespace uagfx::loadingbar
