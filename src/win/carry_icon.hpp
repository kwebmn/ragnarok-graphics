#pragma once
#include <cstdint>

#include "core/window_panel.hpp"

namespace uagfx {
class FrameRecord;
namespace gpu { class Renderer; }
}  // namespace uagfx

namespace uagfx::win::carryicon {

// 3.0.18-beta.91 (the owner: «тащу итем ... внутри окна вещи вещь мышкой не тянется с курсором, но когда вывожу мышку
// за окно то вещь под курсором»): a dragged thing's icon is drawn over all the windows - the window drew it with the
// stack, under uaUI's layer of the windows drawn after it. Put from a window's draw (its picture and texture kept by
// the window), drawn once after the windows' layers, just before the cursor.
void put(panel::Picture& pic, uint32_t& texture, int& texW, int& texH, bool& upload, int x, int y, float k);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::carryicon
