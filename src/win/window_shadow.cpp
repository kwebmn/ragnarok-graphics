#include "win/window_shadow.hpp"

#include <cmath>

#include "core/ui_style.hpp"
#include "win/picture_draw.hpp"

namespace uagfx::win {

void WindowShadow::draw(FrameRecord& record, gpu::Renderer& renderer, int x, int y, int width, int height,
                        float radius, float s, float k) {
    if (width <= 0 || height <= 0 || k <= 0) return;
    // the concept's: 18 round, a half dark at the edge, 4 down; 3.0.11-beta.198 (the owner: «сузить. должно быть
    // акуратно»): 12 round, 40 % at the edge, dropped 3 - as dark right under the window's bottom edge (the picture
    // itself moved down left a light line there)
    if (width != w || height != h || s != scale) {
        w = width;
        h = height;
        scale = s;
        spread = int(std::lround(12 * s));
        picture = uistyle::shadow(width, height, radius, float(spread), 0.40f, int(std::lround(3 * s)));
        upload = true;
    }
    drawPart(record, renderer, picture, texture, texW, texH, upload, x - spread, y - spread, k);
}

void WindowEdge::draw(FrameRecord& record, gpu::Renderer& renderer, int x, int y, int width, int height, float radius,
                      float s, float k) {
    if (width <= 0 || height <= 0 || k <= 0) return;
    if (width != w || height != h || s != scale) {  // the interface's gold, 1.5 pixels a scale's
        w = width;
        h = height;
        scale = s;
        picture = uistyle::edge(width, height, radius, 1.5f * s, 0xE6CB8A, 0.85f);
        upload = true;
    }
    drawPart(record, renderer, picture, texture, texW, texH, upload, x, y, k);
}

}  // namespace uagfx::win
