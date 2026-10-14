#include <cstdint>

#include "core/effect_exe.hpp"
#include "test.hpp"

using namespace uagfx;
using effect_exe::Kind;

namespace {

struct Tl1 {  // FVF 0x1C4
    float x, y, z, rhw;
    uint32_t diffuse, specular;
    float u, v;
};
struct Tl2 {  // FVF 0x2C4
    float x, y, z, rhw;
    uint32_t diffuse, specular;
    float u0, v0, u1, v1;
};

}  // namespace

TEST(effect_exe_knows_the_level_99_aura) {
    // 0.27: the ring, the bubbles and the circle, and the transcendent classes' ones.
    for (int id : {200, 201, 202, 362, 397, 398}) CHECK(effect_exe::kindOf(id) == Kind::Aura);
    for (int id : {0, 47, 199, 203, 361, 363, 396, 399, 629, -1}) CHECK(effect_exe::kindOf(id) == Kind::None);
}

TEST(effect_exe_softens_the_colour_and_alpha_of_a_draw) {
    // 25 %: colour and alpha each at half, so that an effect's added light (colour times alpha) is a quarter.
    Tl1 v[2] = {{1, 2, 3, 0.5f, 0xFFFFFFFF, 0xFF102030, 0.25f, 0.75f}, {4, 5, 6, 1, 0x80402010, 0, 0, 1}};
    effect_exe::soften(reinterpret_cast<uint8_t*>(v), 2, sizeof(Tl1), 25);
    CHECK_EQ(v[0].diffuse, 0x80808080u);
    CHECK_EQ(v[1].diffuse, 0x40201008u);
    // Nothing else changes: the place, the specular colour (the fog), the texture's coordinates.
    CHECK(v[0].x == 1 && v[0].y == 2 && v[0].z == 3 && v[0].rhw == 0.5f);
    CHECK_EQ(v[0].specular, 0xFF102030u);
    CHECK(v[0].u == 0.25f && v[0].v == 0.75f && v[1].x == 4 && v[1].v == 1);
    // Two textures' coordinates: the colour is at the same place.
    Tl2 w = {7, 8, 9, 1, 0xFF808080, 0x11223344, 0.1f, 0.2f, 0.3f, 0.4f};
    effect_exe::soften(reinterpret_cast<uint8_t*>(&w), 1, sizeof(Tl2), 25);
    CHECK_EQ(w.diffuse, 0x80404040u);
    CHECK(w.specular == 0x11223344u && w.u1 == 0.3f && w.v1 == 0.4f);
    // 100 %: as the game made it; 0 %: nothing left.
    Tl1 same = {0, 0, 0, 1, 0xC0A08060, 0, 0, 0};
    effect_exe::soften(reinterpret_cast<uint8_t*>(&same), 1, sizeof(Tl1), 100);
    CHECK_EQ(same.diffuse, 0xC0A08060u);
    effect_exe::soften(reinterpret_cast<uint8_t*>(&same), 1, sizeof(Tl1), 0);
    CHECK_EQ(same.diffuse, 0u);
}

TEST(effect_exe_finds_the_primitive_a_draws_vertices_are_in) {
    // A face's vertices lie inside its primitive (CEffectPrim, kPrimSize bytes).
    effect_exe::Prims p;
    CHECK(p.of(0x2000000) == Kind::None);
    p.note(0x3000000, Kind::Aura);
    p.note(0x1000000, Kind::Aura);
    CHECK(p.of(0x3000000) == Kind::Aura);
    CHECK(p.of(0x3000000 + 0x1F04 + 5 * 0x6C) == Kind::Aura);
    CHECK(p.of(0x3000000 + effect_exe::kPrimSize - 1) == Kind::Aura);
    CHECK(p.of(0x3000000 + effect_exe::kPrimSize) == Kind::None);
    CHECK(p.of(0x2FFFFFF) == Kind::None);
    CHECK(p.of(0x1000000 + 0x40) == Kind::Aura);
    CHECK(p.of(0) == Kind::None);
    // The same primitive again (drawn twice in a frame) changes nothing.
    p.note(0x1000000, Kind::Aura);
    CHECK(p.of(0x1000000 + 0x40) == Kind::Aura);
    // A new frame: the primitives drawn in it are noted anew - one gone may hold anything by now.
    p.clear();
    CHECK(p.of(0x3000000) == Kind::None);
    CHECK(p.of(0x1000000) == Kind::None);
}
