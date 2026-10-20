#include <cstdint>
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
