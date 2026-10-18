#pragma once
#include <cstdint>
#include <vector>

#include "core/map_textures.hpp"

namespace uagfx::atlas_tiles {

// 2.3, uaRO Studio (T6): a ground texture the maps take as an atlas - tiles of a 4x4 (or 2x2) grid, each cell of the
// map one of them, whatever lies beside it - made into a picture tile by tile: the network sees each tile wrapped
// about its own edges, never the neighbour it has in the atlas but not on the map (the seams of local padding,
// arXiv 2309.02340; the seamless modes of ESRGAN tools).
struct Grid {
    int cols = 1, rows = 1;
};
// The grid every rectangle the maps take lies in one cell of (1/512 of slack at the lines): 4, else 2, else 1 each
// way. A texture most of whose neighbouring cells go on across their edges is one picture spread over cells: 1x1.
Grid gridOf(const map_textures::GroundUse& use);
// The tile (col, row) of a w x h texture with `pad` texels of itself wrapped about it: tw x th = its size + 2 pad.
std::vector<uint32_t> cutTile(const uint32_t* argb, int w, int h, Grid g, int col, int row, int pad, int& tw, int& th);
// An enlarged tile (tw x th with padScaled about it) put into its place of the enlarged texture (W x H).
void placeTile(std::vector<uint32_t>& out, int W, int H, Grid g, int col, int row, const std::vector<uint32_t>& tile,
               int tw, int th, int padScaled);

}  // namespace uagfx::atlas_tiles
