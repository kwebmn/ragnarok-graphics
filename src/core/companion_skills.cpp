#include "core/companion_skills.hpp"

#include <algorithm>

namespace uagfx::companion {

int useOf(int stored, int level) { return stored >= 1 && stored <= level ? stored : std::max(level, 0); }

int stepped(int use, int level, int by) { return level < 1 ? use : std::clamp(use + by, 1, level); }

RowParts rowParts(const panel::Box& row, float s, bool raise) {
    RowParts p;
    const float right = row.x + row.w - (raise ? 26 * s : 4 * s), side = 16 * s, words = 58 * s;
    const float y = row.y + (row.h - side) / 2;
    p.more = panel::Box{right - side, y, side, side};
    p.level = panel::Box{p.more.x - words, row.y, words, row.h};
    p.less = panel::Box{p.level.x - side, y, side, side};
    return p;
}

bool dragged(float x0, float y0, float x, float y, float s) {
    const float dx = x - x0, dy = y - y0, reach = 6 * s;
    return dx * dx + dy * dy > reach * reach;
}

}  // namespace uagfx::companion
