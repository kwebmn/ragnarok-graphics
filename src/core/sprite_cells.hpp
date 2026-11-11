#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/image.hpp"

namespace uagfx::sprite_cells {

// 0.93, the top level of a sprite cache: which frame each of its cells holds now - its kind (sprite_exe::Source) and
// whether it is a card on the ground (0.95). 2.7.12: a frame written where older ones lay - the cache's space used
// again - takes their place; until now they stayed ahead of it in the list, and a monster written over a cursor's or a
// player's old frame was taken for that (the tester on 2.7.11: the hit flash's probe found the monster's quad at its
// place, 37x48, «interface»).
struct Cell {
    Rect rect;
    uint8_t source = 0;
    bool card = false;
    // 3.0.18-beta.178: a number's frame (core/sharp_frames.hpp) - its sharp copy's key and the frame's size, the
    // frame in the cell's top left corner; 0: none
    uint64_t sharp = 0;
    int16_t frameW = 0, frameH = 0;
};

class Cells {
public:
    static constexpr int kMost = 1024;
    void written(const Rect& r, uint8_t source, bool card, uint64_t sharp = 0, int frameW = 0, int frameH = 0);
    // 3.0.18-beta.178: the cell at (x, y) of the texture; nullptr where none is known
    const Cell* find(float x, float y) const;
    // ... among the numbers' cells alone (a few: every sprite's draw from the cache asks)
    const Cell* number(float x, float y) const;
    bool anyNumber() const { return !numbers_.empty(); }
    // The kind of the frame at (x, y) of the texture, 0 where none is known; card: whether it is a card
    uint8_t at(float x, float y, bool* card) const;
    size_t size() const { return cells_.size(); }

private:
    std::vector<Cell> cells_;
    std::vector<Cell> numbers_;  // 3.0.18-beta.178: the cells with a sharp copy, as cells_ has them
};

}  // namespace uagfx::sprite_cells
