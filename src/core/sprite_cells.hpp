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
};

class Cells {
public:
    static constexpr int kMost = 1024;
    void written(const Rect& r, uint8_t source, bool card);
    // The kind of the frame at (x, y) of the texture, 0 where none is known; card: whether it is a card
    uint8_t at(float x, float y, bool* card) const;
    size_t size() const { return cells_.size(); }

private:
    std::vector<Cell> cells_;
};

}  // namespace uagfx::sprite_cells
