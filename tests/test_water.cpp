#include <cmath>
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

TEST(water_waves_as_the_game_lifts_them) {
    // 3.0.4-beta.23 (R-W7A5XC «полоска стала меньше»): uaRO.exe's water rises and falls a cell corner at a time
    // (0x4e59d0): its level plus the sine of (gx + gy - 1) * pitch + phase whole degrees times the waves' height - the
    // own far water flat beside it left a broken line where the two met. The game's sine (FUN_0040ae60, a table filled
    // by 0x40ad90): whole degrees, past +-360 their remainder, below 0 one degree off (its table read a place late)
    const float d = 3.14159265f / 180.0f;
    CHECK(std::fabs(water::gameSin(0)) < 1e-6f && std::fabs(water::gameSin(90) - 1.0f) < 1e-6f);
    CHECK(std::fabs(water::gameSin(450) - 1.0f) < 1e-6f);             // 450 % 360
    CHECK(std::fabs(water::gameSin(-90) - std::sin(-89.0f * d)) < 1e-6f);  // one degree off below 0
    CHECK(std::fabs(water::gameSin(-1)) < 1e-6f);
    CHECK(std::fabs(water::gameSin(-360) - std::sin(1.0f * d)) < 1e-6f);
    // a corner's height: level 43, pitch 50, phase 30, waves 0.6 - the corner (3, 4): (3 + 4 - 1) * 50 + 30 = 330
    CHECK(std::fabs(water::waveAt(3, 4, 50, 30, 43.0f, 0.6f) - (43.0f + 0.6f * std::sin(330.0f * d))) < 1e-5f);
    CHECK(water::waveAt(3, 4, 50, 30, 43.0f, 0.0f) == 43.0f);  // no waves: flat
    // the phase back from the heights of a quad's corners as the game drew them
    for (int phase : {-180, -91, -1, 0, 37, 179}) {
        const int gx[4] = {10, 11, 10, 11}, gy[4] = {20, 20, 21, 21};
        float h[4];
        for (int k = 0; k < 4; ++k) h[k] = water::waveAt(gx[k], gy[k], 50, phase, 43.0f, 0.6f) + 0.0005f;
        CHECK_EQ(water::wavePhaseOf(gx, gy, h, 4, 50, 43.0f, 0.6f), phase);
    }
    // the phase on: its speed a game tick (16 ms), past 180 a turn back (FUN_004e3b80)
    CHECK_EQ(water::phaseAfter(170, 3, 4), -178);
    CHECK_EQ(water::phaseAfter(10, 3, 2), 16);
}

