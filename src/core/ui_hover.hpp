#pragma once
#include <vector>

namespace uagfx::uihover {

// 3.0.22-beta.40 (the owner's interface over the MCP: the character block's tips showed through «Навички» and
// «Статус»): uaUI's windows are passive documents - their parts hit-test them, RmlUi sees through them - so the live
// documents under a window took the pointer's hover. Which document is on top at a point, as uaUI draws them: the
// top layer, then the windows by their z-index, then the panels.
enum class Layer { Hud, Windows, Top };
struct DocAt {
    int x = 0, y = 0, w = 0, h = 0;  // the target's pixels
    Layer layer = Layer::Hud;
    int z = -1;  // its z-index (-1 never raised)
    bool passive = false;
};
// The topmost document at (x, y) is a passive window (or a passive part of the top layer): no hover for the ones under
bool passiveOver(const std::vector<DocAt>& docs, int x, int y);

}  // namespace uagfx::uihover
