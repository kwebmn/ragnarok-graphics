#include "core/sprite_cells.hpp"

#include <algorithm>

namespace uagfx::sprite_cells {

void Cells::written(const Rect& r, uint8_t source, bool card) {
    // the cells it lies over hold frames no more
    cells_.erase(std::remove_if(cells_.begin(), cells_.end(),
                                [&](const Cell& c) {
                                    return c.rect.left < r.right && r.left < c.rect.right && c.rect.top < r.bottom &&
                                           r.top < c.rect.bottom;
                                }),
                 cells_.end());
    if (cells_.size() >= size_t(kMost)) cells_.erase(cells_.begin());  // the oldest first
    cells_.push_back(Cell{r, source, card});
}

uint8_t Cells::at(float x, float y, bool* card) const {
    for (const Cell& c : cells_)
        if (x >= float(c.rect.left) && x < float(c.rect.right) && y >= float(c.rect.top) && y < float(c.rect.bottom)) {
            if (card) *card = c.card;
            return c.source;
        }
    if (card) *card = false;
    return 0;
}

}  // namespace uagfx::sprite_cells
