#include <cstdint>
#include <vector>

#include "core/atlas_tiles.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

map_textures::GroundUse useOf(const std::vector<map_textures::UvRect>& rects, uint32_t pairs = 0,
                              uint32_t joined = 0) {
    map_textures::GroundUse u;
    u.rects = rects;
    u.pairs = pairs;
    u.joined = joined;
    return u;
}

// 64 x 64, each 16 x 16 cell of its own colour, and inside each cell a gradient
std::vector<uint32_t> cells64() {
    std::vector<uint32_t> p(64 * 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            p[size_t(y * 64 + x)] = 0xFF000000u | uint32_t((y / 16) * 4 + x / 16) << 16 | uint32_t(x % 16) << 8 |
                                    uint32_t(y % 16);
    return p;
}

std::vector<uint32_t> nearest4(const std::vector<uint32_t>& p, int w, int h) {
    std::vector<uint32_t> out(size_t(w) * 4 * size_t(h) * 4);
    for (int y = 0; y < h * 4; ++y)
        for (int x = 0; x < w * 4; ++x) out[size_t(y) * size_t(w * 4) + size_t(x)] = p[size_t(y / 4 * w + x / 4)];
    return out;
}

}  // namespace

TEST(atlas_grid_four_by_four) {
    // 2.3 (T6): a ground texture whose surfaces take one cell of a 4x4 (or 2x2) grid each - an atlas
    using R = map_textures::UvRect;
    CHECK(atlas_tiles::gridOf(useOf({R{0, 0, 0.25f, 0.25f}, R{0.75f, 0.5f, 1, 0.75f}})).cols == 4);
    CHECK(atlas_tiles::gridOf(useOf({R{0, 0, 0.25f, 0.25f}, R{0.75f, 0.5f, 1, 0.75f}})).rows == 4);
    // across a quarter's line, inside a half: 2 that way
    const atlas_tiles::Grid half = atlas_tiles::gridOf(useOf({R{0.2f, 0, 0.45f, 0.25f}}));
    CHECK(half.cols == 2 && half.rows == 4);
    const atlas_tiles::Grid whole = atlas_tiles::gridOf(useOf({R{0, 0, 1, 1}}));
    CHECK(whole.cols == 1 && whole.rows == 1);
    // a hair past the line is no crossing (1/512)
    CHECK(atlas_tiles::gridOf(useOf({R{0.249f, 0, 0.5f, 0.25f}})).cols == 4);
    // a texture spread over cells (most neighbours go on across their edge) is one picture, not an atlas
    const atlas_tiles::Grid spread = atlas_tiles::gridOf(useOf({R{0, 0, 0.25f, 0.25f}}, 10, 9));
    CHECK(spread.cols == 1 && spread.rows == 1);
    CHECK(atlas_tiles::gridOf(useOf({R{0, 0, 0.25f, 0.25f}}, 10, 2)).cols == 4);
    CHECK(atlas_tiles::gridOf(useOf({})).cols == 1);
}

TEST(atlas_tile_wraps_itself) {
    const std::vector<uint32_t> p = cells64();
    int tw = 0, th = 0;
    const std::vector<uint32_t> t = atlas_tiles::cutTile(p.data(), 64, 64, atlas_tiles::Grid{4, 4}, 1, 2, 4, tw, th);
    CHECK(tw == 24 && th == 24 && t.size() == 24u * 24u);
    const auto cell = [&](int x, int y) { return p[size_t((32 + y) * 64 + 16 + x)]; };  // cell (1, 2)
    CHECK_EQ(t[size_t(4 * 24 + 4)], cell(0, 0));
    CHECK_EQ(t[size_t(4 * 24 + 0)], cell(12, 0));  // left of it: its own right edge, not the neighbour's
    CHECK_EQ(t[size_t(0 * 24 + 4)], cell(0, 12));  // above it: its own bottom
    CHECK_EQ(t[size_t(23 * 24 + 23)], cell(3, 3));  // past the bottom right corner: its top left
    for (uint32_t v : t) CHECK_EQ(v >> 16 & 0xFF, uint32_t(9));  // every texel of cell 9
}

TEST(atlas_tile_goes_back_in_place) {
    const std::vector<uint32_t> p = cells64();
    const std::vector<uint32_t> whole = nearest4(p, 64, 64);
    std::vector<uint32_t> out(whole.size(), 0);
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            int tw = 0, th = 0;
            const std::vector<uint32_t> t =
                atlas_tiles::cutTile(p.data(), 64, 64, atlas_tiles::Grid{4, 4}, col, row, 4, tw, th);
            atlas_tiles::placeTile(out, 256, 256, atlas_tiles::Grid{4, 4}, col, row, nearest4(t, tw, th), tw * 4,
                                   th * 4, 16);
        }
    CHECK(out == whole);
}
