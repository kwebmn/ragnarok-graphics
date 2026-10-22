#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/ground_heights.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A .gnd 1.7 of width x height cells, ten units a cell: one texture, no lightmaps, one surface; each cell flat at
// heights[y * width + x] (up along -y), its top that surface.
std::vector<uint8_t> gndOf(int width, int height, const std::vector<float>& heights) {
    std::vector<uint8_t> d;
    const auto put = [&](const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        d.insert(d.end(), b, b + n);
    };
    const auto i32 = [&](int32_t v) { put(&v, 4); };
    const auto f32 = [&](float v) { put(&v, 4); };
    put("GRGN", 4);
    d.push_back(1);
    d.push_back(7);
    i32(width);
    i32(height);
    f32(10.0f);
    i32(1);   // textures
    i32(80);  // a name's length
    char name[80] = "sand.bmp";
    put(name, sizeof name);
    i32(0);  // lightmaps
    i32(8);
    i32(8);
    i32(1);
    i32(1);  // surfaces
    for (int k = 0; k < 8; ++k) f32(0.0f);
    int16_t texture = 0;
    uint16_t lightmap = 0;
    put(&texture, 2);
    put(&lightmap, 2);
    i32(-1);  // BGRA
    for (float h : heights) {
        for (int k = 0; k < 4; ++k) f32(h);
        i32(0);  // top
        i32(-1);
        i32(-1);
    }
    return d;
}

// 2.7.57: as gndOf, flat, each cell's top a surface of its texture texture[y * width + x] (-1: no top)
std::vector<uint8_t> gndWithTextures(int width, int height, const std::vector<int>& texture) {
    std::vector<uint8_t> d;
    const auto put = [&](const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        d.insert(d.end(), b, b + n);
    };
    const auto i32 = [&](int32_t v) { put(&v, 4); };
    const auto f32 = [&](float v) { put(&v, 4); };
    put("GRGN", 4);
    d.push_back(1);
    d.push_back(7);
    i32(width);
    i32(height);
    f32(10.0f);
    i32(8);   // textures
    i32(80);  // a name's length
    for (int k = 0; k < 8; ++k) {
        char name[80] = {};
        std::snprintf(name, sizeof name, "t%d.bmp", k);
        put(name, sizeof name);
    }
    i32(0);  // lightmaps
    i32(8);
    i32(8);
    i32(1);
    i32(8);  // surfaces: surface k of texture k
    for (int k = 0; k < 8; ++k) {
        for (int j = 0; j < 8; ++j) f32(0.0f);
        const int16_t t = int16_t(k);
        const uint16_t lightmap = 0;
        put(&t, 2);
        put(&lightmap, 2);
        i32(-1);  // BGRA
    }
    for (int c = 0; c < width * height; ++c) {
        for (int k = 0; k < 4; ++k) f32(0.0f);
        i32(texture[size_t(c)]);  // top
        i32(-1);
        i32(-1);
    }
    return d;
}

}  // namespace

TEST(ground_heights_reads_a_maps_cells) {
    // 4 x 3 cells, a wall 60 high along the third column
    std::vector<float> h(12, 0.0f);
    for (int y = 0; y < 3; ++y) h[size_t(y * 4 + 2)] = -60.0f;
    const std::vector<uint8_t> d = gndOf(4, 3, h);
    ground_heights::Ground g;
    std::string error;
    CHECK(ground_heights::parse(d.data(), d.size(), g, error));
    CHECK(g.width == 4 && g.height == 3 && g.zoom == 10.0f && g.corners.size() == size_t(48));
    float y = 1.0f;
    CHECK(ground_heights::heightAt(g, 5.0f, 0.0f, y) && y == -60.0f);  // the map's middle at 0: cell 2 is x 0-10
    CHECK(ground_heights::heightAt(g, -15.0f, 12.0f, y) && y == 0.0f);
    CHECK(!ground_heights::heightAt(g, 25.0f, 0.0f, y));  // past the map
    CHECK(!ground_heights::parse(d.data(), d.size() - 30, g, error));  // cut short
}

TEST(ground_heights_walls_stand_between_a_torch_and_whom_it_lights) {
    // R-QPYCS9: "a fire behind the wall, its shadow goes through the wall"
    std::vector<float> h(12, 0.0f);
    for (int y = 0; y < 3; ++y) h[size_t(y * 4 + 2)] = -60.0f;
    const std::vector<uint8_t> d = gndOf(4, 3, h);
    ground_heights::Ground g;
    std::string error;
    CHECK(ground_heights::parse(d.data(), d.size(), g, error));
    const map::Point torch{-1.0f, -30.0f, 0.0f};  // on the wall's west face, 30 up
    CHECK(ground_heights::clear(g, torch, map::Point{-15.0f, -5.0f, 3.0f}, 6.0f, 1.5f, 2.0f));  // its own side
    CHECK(!ground_heights::clear(g, torch, map::Point{15.0f, -5.0f, -3.0f}, 6.0f, 1.5f, 2.0f));  // behind the wall
    // a torch hanging a little into its wall still lights its own side
    CHECK(ground_heights::clear(g, map::Point{0.5f, -30.0f, 0.0f}, map::Point{-15.0f, -5.0f, 0.0f}, 6.0f, 1.5f, 2.0f));
    // a light high over the wall reaches past it
    CHECK(ground_heights::clear(g, map::Point{-1.0f, -200.0f, 0.0f}, map::Point{15.0f, -5.0f, 0.0f}, 6.0f, 1.5f,
                                2.0f));
    // no ground known, or a way off the map: nothing stands in it
    CHECK(ground_heights::clear(ground_heights::Ground{}, torch, map::Point{15.0f, -5.0f, 0.0f}, 6.0f, 1.5f, 2.0f));
    CHECK(ground_heights::clear(g, map::Point{-30.0f, -30.0f, 30.0f}, map::Point{-60.0f, -5.0f, 30.0f}, 6.0f, 1.5f,
                                2.0f));
}

namespace {
// 12 x 10 cells of texture 0 but: a square of 4 x 4 of texture 1 (x 1-4, y 1-4), a mat of 2 x 4 of texture 2 (x 7-8,
// y 1-4), an L of texture 3 (x 6-8 at y 7, x 8 at y 6), texture 4 in a square of 2 x 2 (x 10-11, y 8-9) and an L of
// three (x 1-2 at y 8, x 2 at y 9)
std::vector<int> patchedField() {
    std::vector<int> t(12 * 10, 0);
    const auto set = [&](int x, int y, int v) { t[size_t(y * 12 + x)] = v; };
    for (int y = 1; y <= 4; ++y)
        for (int x = 1; x <= 4; ++x) set(x, y, 1);
    for (int y = 1; y <= 4; ++y)
        for (int x = 7; x <= 8; ++x) set(x, y, 2);
    set(6, 7, 3), set(7, 7, 3), set(8, 7, 3), set(8, 6, 3);
    set(10, 8, 4), set(11, 8, 4), set(10, 9, 4), set(11, 9, 4);
    set(1, 8, 4), set(2, 8, 4), set(2, 9, 4);
    return t;
}
}  // namespace

TEST(ground_heights_laid_patches_are_the_makers_rectangles) {
    // 2.7.57 (R-8YJSCR: Prontera's mats torn at their edges, «в версии первоначальной там они прямоугольные»; its
    // paving is squares of 4 x 4 of eight textures): a cell of one texture in a whole rectangle of it is laid by
    // hand - and so is every cell of a texture mostly laid so; the fields' irregular grass and roads are not
    const std::vector<int> t = patchedField();
    const std::vector<uint8_t> laid = ground_heights::laidPatches(12, 10, t);
    CHECK_EQ(laid.size(), size_t(120));
    const auto at = [&](int x, int y) { return laid[size_t(y * 12 + x)]; };
    CHECK(at(1, 1) && at(4, 4) && at(2, 3));      // the square
    CHECK(at(7, 1) && at(8, 4));                  // the mat
    CHECK(!at(6, 7) && !at(8, 6));                // the L: no rectangle, its texture nowhere else
    CHECK(!at(0, 0) && !at(5, 5) && !at(11, 0));  // the field round them
    CHECK(at(10, 8) && at(11, 9));                // a square...
    CHECK(at(1, 8) && at(2, 9));                  // ...and the L of its texture: 4 of its 7 cells laid so
    std::vector<int> none = t;
    none[0] = -1;  // a cell with no top is no part of any patch
    CHECK(!ground_heights::laidPatches(12, 10, none)[0]);
}

TEST(ground_heights_laid_patches_read_with_the_map) {
    const std::vector<int> t = patchedField();
    const std::vector<uint8_t> d = gndWithTextures(12, 10, t);
    ground_heights::Ground g;
    std::string why;
    CHECK(ground_heights::parse(d.data(), d.size(), g, why));
    CHECK_EQ(g.laid.size(), size_t(120));
    // the file's places: cell (x, z) = (x / zoom + width / 2, z / zoom + height / 2)
    const auto placeOf = [](int cx, int cz, float& x, float& z) {
        x = (float(cx) + 0.5f - 6.0f) * 10.0f;
        z = (float(cz) + 0.5f - 5.0f) * 10.0f;
    };
    float x = 0.0f, z = 0.0f;
    placeOf(2, 2, x, z);
    CHECK(ground_heights::laidAt(g, x, z));
    placeOf(0, 0, x, z);
    CHECK(!ground_heights::laidAt(g, x, z));
    placeOf(7, 3, x, z);
    CHECK(ground_heights::laidAt(g, x, z));
    CHECK(!ground_heights::laidAt(g, 1000.0f, 0.0f));  // off the map
}
