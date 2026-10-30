#include "win/window_shadow.hpp"

#include <cmath>

#include "core/ui_style.hpp"
#include "win/picture_draw.hpp"

namespace uagfx::win {

void WindowShadow::draw(FrameRecord& record, gpu::Renderer& renderer, int x, int y, int width, int height,
                        float radius, float s, float k) {
    if (width <= 0 || height <= 0 || k <= 0) return;
    if (width != w || height != h || s != scale) {  // the concept's: 18 round, a half dark at the edge, 4 down
        w = width;
        h = height;
        scale = s;
        spread = int(std::lround(18 * s));
        picture = uistyle::shadow(width, height, radius, float(spread), 0.45f);
        upload = true;
    }
    drawPicture(record, renderer, picture, texture, texW, texH, upload, x - spread,
                y - spread + int(std::lround(4 * s)), k);
}

}  // namespace uagfx::win
