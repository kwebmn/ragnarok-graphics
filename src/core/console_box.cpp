#include "core/console_box.hpp"

#include <algorithm>

namespace uagfx::consolebox {

Box box(int clientW, int clientH, int systemFontH) {
    Box b;
    b.fontH = std::max(12, std::min(std::max(12, systemFontH), clientH / 46));
    b.line = b.fontH * 3 / 2;
    b.x = b.fontH;
    b.y = b.fontH * 3;
    b.w = std::min(std::max(b.fontH * 26, clientW * 2 / 5), std::max(b.fontH * 10, clientW - 2 * b.fontH));
    b.h = std::min(std::max(b.line * 8, clientH * 3 / 4), std::max(b.line * 4, clientH - b.y - b.fontH));
    return b;
}

int visibleRows(const Box& b, int footHeight) {
    return std::max(3, (b.h - b.line * 3 / 2 - footHeight - b.line / 4) / b.line);
}

uint8_t groundAlpha(int panelOpacity) {
    const float a = std::clamp(float(panelOpacity) * 0.0105f, 0.45f, 0.95f);
    return uint8_t(a * 255.0f + 0.5f);
}

}  // namespace uagfx::consolebox
