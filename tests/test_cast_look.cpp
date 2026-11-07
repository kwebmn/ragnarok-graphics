// 3.0.15-beta.2: [effects] new_casting - the casting's walls held low, its circle from the feet walls of the frame
// before
#include <cstring>
#include <vector>

#include "core/cast_look.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
void put(std::vector<uint8_t>& v, int i, float x, float y, float u, float vv, uint32_t alpha) {
    const float pos[4] = {x, y, 0.5f, 0.01f}, uv[2] = {u, vv};
    const uint32_t diffuse = (alpha << 24) | 0xC3C3FFu;
    std::memcpy(v.data() + i * 32, pos, 16);
    std::memcpy(v.data() + i * 32 + 16, &diffuse, 4);
    std::memcpy(v.data() + i * 32 + 24, uv, 8);
}
float yOf(const std::vector<uint8_t>& v, int i) {
    float f[2];
    std::memcpy(f, v.data() + i * 32, 8);
    return f[1];
}
}  // namespace

TEST(cast_look_holds_the_tall_wall_low) {
    castlook::Looks l;
    std::vector<uint8_t> tri(3 * 32);
    put(tri, 0, 100, 600, 0.0f, 1.0f, 60);   // a wall's foot
    put(tri, 1, 110, 600, 0.05f, 1.0f, 60);
    put(tri, 2, 110, -1400, 0.05f, 0.0f, 60);  // its top 2000 pixels up the screen
    l.shorten(tri.data(), 3, 32, 7, 100.0f);
    CHECK(yOf(tri, 2) >= 499.9f && yOf(tri, 2) < 600.0f);  // at most 100 over its foot
    CHECK(yOf(tri, 0) == 600.0f);
    std::vector<uint8_t> low(3 * 32);  // a short wall stays nearly as it was
    put(low, 0, 100, 600, 0.0f, 1.0f, 180);
    put(low, 1, 110, 600, 0.05f, 1.0f, 180);
    put(low, 2, 110, 590, 0.05f, 0.0f, 180);
    l.shorten(low.data(), 3, 32, 7, 100.0f);
    CHECK(yOf(low, 2) < 591.0f && yOf(low, 2) > 589.0f);
}

TEST(cast_look_circle_under_the_feet_walls_once_a_frame) {
    castlook::Looks l;
    std::vector<uint8_t> tri(3 * 32);
    put(tri, 0, 80, 590, 0.0f, 1.0f, 180);
    put(tri, 1, 120, 610, 0.5f, 1.0f, 180);
    put(tri, 2, 100, 560, 0.5f, 0.0f, 180);
    uint8_t quad[4 * 32];
    CHECK(!l.circleQuad(7, quad));  // nothing seen the frame before
    l.shorten(tri.data(), 3, 32, 7, 100.0f);
    l.frame();
    CHECK(l.circleQuad(7, quad));
    CHECK(!l.circleQuad(7, quad));  // once a frame
    float a[2], d[2];
    std::memcpy(a, quad, 8);
    std::memcpy(d, quad + 3 * 32, 8);
    CHECK(a[0] < 80.0f && d[0] > 120.0f && a[1] < 590.0f && d[1] > 610.0f);  // larger than the feet walls, round them
    CHECK(!l.circleQuad(8, quad));  // another caster's own
}

TEST(cast_look_a_target_casting_is_one_circle_and_the_games_walls) {
    // .6: a casting on no actor - its rings under one key, noted as they are, one circle the next frame
    castlook::Looks l;
    const uint32_t key = castlook::keyOf(0, 54);
    CHECK(key == castlook::keyOf(0, 54) && key != castlook::keyOf(7, 54));
    std::vector<uint8_t> a(3 * 32), b(3 * 32);
    put(a, 0, 80, 590, 0.0f, 1.0f, 180);
    put(a, 1, 120, 610, 0.5f, 1.0f, 180);
    put(a, 2, 100, 400, 0.5f, 0.0f, 180);
    put(b, 0, 60, 580, 0.0f, 1.0f, 180);  // another ring of the same effect
    put(b, 1, 140, 620, 0.5f, 1.0f, 180);
    put(b, 2, 100, 400, 0.5f, 0.0f, 180);
    l.note(a.data(), 3, 32, key);
    l.note(b.data(), 3, 32, key);
    CHECK(yOf(a, 2) == 400.0f);  // its wall as the game made it
    l.frame();
    uint8_t quad[4 * 32];
    CHECK(l.circleQuad(key, quad) && !l.circleQuad(key, quad));
    float first[2];
    std::memcpy(first, quad, 8);
    CHECK(first[0] < 60.0f);  // round both rings
}
