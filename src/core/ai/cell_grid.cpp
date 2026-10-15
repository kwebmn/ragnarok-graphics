#include "core/ai/cell_grid.hpp"

#include <algorithm>
#include <cstring>

namespace uagfx::ai {
namespace {

bool inside(const Rect& a, const Rect& b) {  // a within b
    return a.left >= b.left && a.top >= b.top && a.right <= b.right && a.bottom <= b.bottom;
}

}  // namespace

void CellGrid::reset(int width, int height) {
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    cellW_ = cellH_ = 0;
    pixels_.assign(size_t(width_) * size_t(height_), 0);
}

void CellGrid::noteCell(const Rect& cell) {
    const int w = cell.width(), h = cell.height();
    if (w <= 0 || h <= 0 || cell.left % w || cell.top % h || cell.right > width_ || cell.bottom > height_) return;
    cellW_ = w;
    cellH_ = h;
}

void CellGrid::update(const Rect& r, const uint32_t* argb, const SpriteCell* written, uint32_t writtenCount,
                      std::vector<Rect>& cells, Rect& rest) {
    cells.clear();
    rest = Rect{};
    if (!argb || r.empty() || r.left < 0 || r.top < 0 || r.right > width_ || r.bottom > height_) return;
    const int pitch = r.width();
    const auto at = [&](int x, int y) { return argb[size_t(y - r.top) * size_t(pitch) + size_t(x - r.left)]; };
    const auto mine = [&](int x, int y) { return pixels_[size_t(y) * size_t(width_) + size_t(x)]; };
    const auto isWritten = [&](const Rect& c) {
        for (uint32_t i = 0; i < writtenCount; ++i)
            if (written[i].rect == c) return true;
        return false;
    };
    std::vector<uint8_t> done;  // texels a whole cell (changed or written) answers for: not the rest's
    if (cellW_ > 0 && cellH_ > 0) {
        done.assign(size_t(r.width()) * size_t(r.height()), 0);
        for (int cy = r.top / cellH_ * cellH_; cy < r.bottom; cy += cellH_)
            for (int cx = r.left / cellW_ * cellW_; cx < r.right; cx += cellW_) {
                const Rect c{cx, cy, cx + cellW_, cy + cellH_};
                if (!inside(c, r)) continue;
                for (int y = c.top; y < c.bottom; ++y)
                    std::memset(done.data() + size_t(y - r.top) * size_t(r.width()) + size_t(c.left - r.left), 1,
                                size_t(cellW_));
                if (isWritten(c)) continue;
                bool changed = false;
                for (int y = c.top; y < c.bottom && !changed; ++y)
                    changed = std::memcmp(&pixels_[size_t(y) * size_t(width_) + size_t(c.left)],
                                          argb + size_t(y - r.top) * size_t(pitch) + size_t(c.left - r.left),
                                          size_t(cellW_) * 4) != 0;
                if (changed) cells.push_back(c);
            }
    }
    // The rest: texels changed outside the whole cells, and in a written cell that is no cell of the grid.
    Rect box{r.right, r.bottom, r.left, r.top};
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x) {
            if (!done.empty() && done[size_t(y - r.top) * size_t(r.width()) + size_t(x - r.left)]) continue;
            bool skip = false;
            for (uint32_t i = 0; i < writtenCount && !skip; ++i) {
                const Rect& w = written[i].rect;
                skip = x >= w.left && x < w.right && y >= w.top && y < w.bottom;
            }
            if (skip || at(x, y) == mine(x, y)) continue;
            box.left = std::min(box.left, x);
            box.top = std::min(box.top, y);
            box.right = std::max(box.right, x + 1);
            box.bottom = std::max(box.bottom, y + 1);
        }
    if (!box.empty()) rest = box;
    for (int y = r.top; y < r.bottom; ++y)
        std::memcpy(&pixels_[size_t(y) * size_t(width_) + size_t(r.left)], argb + size_t(y - r.top) * size_t(pitch),
                    size_t(pitch) * 4);
}

bool cleared(const uint32_t* argb, int width, int height, int pitch) {
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (argb[size_t(y) * size_t(pitch) + size_t(x)]) return false;
    return true;
}

}  // namespace uagfx::ai
