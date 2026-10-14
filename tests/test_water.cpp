#include <cstring>
#include <vector>

#include "core/water.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// Four TL1 vertices (32 bytes each) as the game's ground renderer fills them for the cell (x, y).
std::vector<uint8_t> cell(int x, int y, uint32_t diffuse = water::kDiffuse) {
    std::vector<uint8_t> v(4 * 32, 0);
    const float u = float(x % 4) * 0.25f, w = float(y % 4) * 0.25f;
    const float uv[4][2] = {{u, w}, {u + 0.25f, w}, {u, w + 0.25f}, {u + 0.25f, w + 0.25f}};
    for (int i = 0; i < 4; ++i) {
        const float xyzw[4] = {100.0f + 20.0f * float(i & 1), 200.0f + 15.0f * float(i >> 1), 0.9f, 0.004f};
        std::memcpy(v.data() + i * 32, xyzw, sizeof xyzw);
        std::memcpy(v.data() + i * 32 + 16, &diffuse, 4);
        const uint32_t specular = 0xFF000000;
        std::memcpy(v.data() + i * 32 + 20, &specular, 4);
        std::memcpy(v.data() + i * 32 + 24, uv[i], sizeof uv[i]);
    }
    return v;
}

}  // namespace

TEST(water_quad_is_every_cell_of_the_grid) {
    for (int x = 0; x < 4; ++x)
        for (int y = 0; y < 4; ++y) CHECK(water::isWaterQuad(cell(x, y).data(), 32, 4, 0, true));
}

TEST(water_quad_wants_the_games_strip_of_four) {
    const std::vector<uint8_t> v = cell(1, 2);
    CHECK(!water::isWaterQuad(v.data(), 32, 4, 0, false));  // a list
    CHECK(!water::isWaterQuad(v.data(), 32, 4, 6, true));   // indexed
    CHECK(!water::isWaterQuad(v.data(), 32, 3, 0, true));
    CHECK(!water::isWaterQuad(v.data(), 24, 4, 0, true));   // no room for the UV
    CHECK(!water::isWaterQuad(nullptr, 32, 4, 0, true));
}

TEST(water_quad_is_not_lava_nor_another_picture) {
    CHECK(!water::isWaterQuad(cell(0, 0, 0xFFFFFFFF).data(), 32, 4, 0, true));  // opaque: lava, or anything else
    CHECK(!water::isWaterQuad(cell(0, 0, 0x90FFFFFE).data(), 32, 4, 0, true));
    std::vector<uint8_t> v = cell(2, 1);
    const float off = 0.3f;  // a UV off the quarter grid: a sprite or a model with alpha 0x90
    std::memcpy(v.data() + 3 * 32 + 24, &off, 4);
    CHECK(!water::isWaterQuad(v.data(), 32, 4, 0, true));
    std::vector<uint8_t> turned = cell(2, 1);  // corners in another order: not the ground renderer's
    std::memcpy(turned.data() + 24, cell(2, 1).data() + 32 + 24, 8);
    std::memcpy(turned.data() + 32 + 24, cell(2, 1).data() + 24, 8);
    CHECK(!water::isWaterQuad(turned.data(), 32, 4, 0, true));
}

TEST(lava_quad_is_an_opaque_cell_in_the_maps_light) {
    // 0.31: lava (water type 4) - the same cells, opaque, every corner in the map's light (one colour).
    CHECK(water::isLavaQuad(cell(0, 0, 0xFFC8A078).data(), 32, 4, 0, true));
    CHECK(water::isLavaQuad(cell(3, 2, 0xFFFFFFFF).data(), 32, 4, 0, true));
    CHECK(!water::isLavaQuad(cell(1, 1).data(), 32, 4, 0, true));               // water
    CHECK(!water::isLavaQuad(cell(1, 1, 0xC0FFFFFF).data(), 32, 4, 0, true));  // not opaque
    std::vector<uint8_t> v = cell(2, 3, 0xFF806040);
    const uint32_t other = 0xFF806041;  // corners of another colour: lit, a model's face
    std::memcpy(v.data() + 2 * 32 + 16, &other, 4);
    CHECK(!water::isLavaQuad(v.data(), 32, 4, 0, true));
    CHECK(!water::isLavaQuad(cell(1, 1, 0xFF808080).data(), 32, 4, 0, false));  // a list
}
