#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::gnd {

// 3.0.4-beta.9 (the user: «сделай 1» - the own world's prototype): a map's ground file (.gnd 1.7) whole - its
// textures (names under data\texture\, cp949 bytes), its lightmaps (8 x 8: brightness, then colour), its surfaces
// (a texture's corners, a lightmap, a colour) and its cells (four corner heights, up along -y, and the surfaces of
// its top, its front and its right side). core/ground_heights.hpp keeps the heights alone, for the walls' checks.
struct Lightmap {
    uint8_t brightness[64];  // 8 x 8, row by row
    uint8_t rgb[192];        // 8 x 8 colours: red, green, blue
};
struct Surface {
    float u[4], v[4];  // the corners (x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1)
    int16_t texture = -1;
    uint16_t lightmap = 0;
    uint32_t color = 0xFFFFFFFF;  // ARGB
};
struct Cell {
    float h[4];  // corners as the surface's; up along -y
    int32_t top = -1, front = -1, right = -1;  // surfaces, -1 none
};
struct Ground {
    int width = 0, height = 0;
    float zoom = 10.0f;  // world units a cell
    std::vector<std::string> textures;
    std::vector<Lightmap> lightmaps;
    std::vector<Surface> surfaces;
    std::vector<Cell> cells;  // row by row
};

// False, with why, for what is not a .gnd 1.7 or ends short
bool parse(const uint8_t* data, size_t size, Ground& out, std::string& error);

}  // namespace uagfx::gnd
