#pragma once
#include <cstdint>
#include <vector>

#include "core/frame.hpp"
#include "core/image.hpp"

namespace uagfx::ai {

// 0.49.3: the game's sprite cache as the game has it - a texture cut into cells of one size from its top left corner
// (sprite_exe.hpp) - and what a piece of it changed. A frame written through the game's one call of its writer comes
// with its cell (SpriteCell); one that came another way (there before the driver saw the texture to be a cache, or
// written past that call) shows here as a whole cell that changed: it is looked up in the pack like the rest. The
// game caches a frame once and draws it from there until its cell is taken: missed, it stayed blocky for that long.
class CellGrid {
public:
    void reset(int width, int height);
    bool empty() const { return pixels_.empty(); }
    // A frame the game wrote: the cells' size from now on (a texture the game takes back may get another size).
    void noteCell(const Rect& cell);
    int cellWidth() const { return cellW_; }
    int cellHeight() const { return cellH_; }
    // The piece r of the texture (argb, r.width() a row) against what it held: the whole cells that changed except
    // those in written, and the bounds of the other texels that changed (empty: none). Then the piece is kept.
    void update(const Rect& r, const uint32_t* argb, const SpriteCell* written, uint32_t writtenCount,
                std::vector<Rect>& cells, Rect& rest);

private:
    int width_ = 0, height_ = 0;
    int cellW_ = 0, cellH_ = 0;
    std::vector<uint32_t> pixels_;
};

// Whether every texel of the piece is transparent black (a cell the game cleared).
bool cleared(const uint32_t* argb, int width, int height, int pitch);

}  // namespace uagfx::ai
