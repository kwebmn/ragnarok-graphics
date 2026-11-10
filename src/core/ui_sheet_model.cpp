#include "core/ui_sheet_model.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::sheet {

void rect(Layer& to, const panel::Box& b, float r, uint32_t fill, float fillA, uint32_t edge, float edgeA,
          float edgeW) {
    if (b.w <= 0 || b.h <= 0 || (fillA <= 0 && edgeA <= 0)) return;
    to.boxes.push_back(Box{b, r, edgeA > 0 ? edgeW : 0, fill, edge, fillA, edgeA});
}

void circle(Layer& to, float cx, float cy, float r, uint32_t rgb, float a) {
    rect(to, panel::Box{cx - r, cy - r, 2 * r, 2 * r}, r, rgb, a);
}

void line(Layer& to, float x0, float y0, float x1, float y1, float width, uint32_t rgb, float a) {
    const float half = width / 2;
    if (std::fabs(y1 - y0) < 0.01f)
        rect(to, panel::Box{std::min(x0, x1) - half, y0 - half, std::fabs(x1 - x0) + width, width}, half, rgb, a);
    else if (std::fabs(x1 - x0) < 0.01f)
        rect(to, panel::Box{x0 - half, std::min(y0, y1) - half, width, std::fabs(y1 - y0) + width}, half, rgb, a);
}

}  // namespace uagfx::sheet
