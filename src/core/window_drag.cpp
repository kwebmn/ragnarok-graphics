#include "core/window_drag.hpp"

#include <cmath>

namespace uagfx::windrag {
namespace {

Thing& current() {
    static Thing t;
    return t;
}

}  // namespace

void pressed(Press& p, int index, float x, float y) { p = Press{index, x, y, false}; }

bool moved(Press& p, float x, float y, float scale) {
    if (p.index >= 0 && !p.dragging && std::hypot(x - p.x, y - p.y) > 10.0f * scale) p.dragging = true;
    return p.index >= 0 && p.dragging;
}

int released(Press& p) {
    const int index = p.dragging ? p.index : -1;
    p = Press{};
    return index;
}

void set(const Thing& t) { current() = t; }

void clear() { current() = Thing{}; }

const Thing& now() { return current(); }

}  // namespace uagfx::windrag
