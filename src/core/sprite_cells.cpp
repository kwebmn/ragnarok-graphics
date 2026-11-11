#include "core/sprite_cells.hpp"

#include <algorithm>

namespace uagfx::sprite_cells {

void Cells::written(const Rect& r, uint8_t source, bool card, uint64_t sharp, int frameW, int frameH) {
    // the cells it lies over hold frames no more
    const auto under = [&](const Cell& c) {
        return c.rect.left < r.right && r.left < c.rect.right && c.rect.top < r.bottom && r.top < c.rect.bottom;
    };
    cells_.erase(std::remove_if(cells_.begin(), cells_.end(), under), cells_.end());
    numbers_.erase(std::remove_if(numbers_.begin(), numbers_.end(), under), numbers_.end());
    if (cells_.size() >= size_t(kMost)) {  // the oldest first
        const Rect gone = cells_.front().rect;
        numbers_.erase(std::remove_if(numbers_.begin(), numbers_.end(), [&](const Cell& c) { return c.rect == gone; }),
                       numbers_.end());
        cells_.erase(cells_.begin());
    }
    cells_.push_back(Cell{r, source, card, sharp, int16_t(frameW), int16_t(frameH)});
    if (sharp) numbers_.push_back(cells_.back());
}

const Cell* Cells::number(float x, float y) const {
    for (const Cell& c : numbers_)
        if (x >= float(c.rect.left) && x < float(c.rect.right) && y >= float(c.rect.top) && y < float(c.rect.bottom))
            return &c;
    return nullptr;
}

const Cell* Cells::find(float x, float y) const {
    for (const Cell& c : cells_)
        if (x >= float(c.rect.left) && x < float(c.rect.right) && y >= float(c.rect.top) && y < float(c.rect.bottom))
            return &c;
    return nullptr;
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
