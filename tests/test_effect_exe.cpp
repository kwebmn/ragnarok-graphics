#include <cstdint>

#include "core/d3d7.hpp"
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
    // 0.37: the rest are skills and the like - but the maps' own (torches, smoke, bubbles, bats...) and nothing.
    for (int id : {47, 44, 109, 353, 359, 557, -1, 0}) CHECK(effect_exe::kindOf(id) == Kind::None);
    for (int id : {89, 199, 203, 361, 363, 396, 399, 629}) CHECK(effect_exe::kindOf(id) == Kind::Skill);
}

TEST(effect_exe_softens_the_colour_and_alpha_of_a_draw) {
    // 25 % of an added draw's colour: a quarter of its light; its alpha, place, fog and texture as they were.
    Tl1 v[2] = {{1, 2, 3, 0.5f, 0xFFFFFFFF, 0xFF102030, 0.25f, 0.75f}, {4, 5, 6, 1, 0x80402010, 0, 0, 1}};
    effect_exe::soften(reinterpret_cast<uint8_t*>(v), 2, sizeof(Tl1), 25, effect_exe::Soften::Colour);
    CHECK_EQ(v[0].diffuse, 0xFF404040u);
    CHECK_EQ(v[1].diffuse, 0x80100804u);
    // Nothing else changes: the place, the specular colour (the fog), the texture's coordinates.
    CHECK(v[0].x == 1 && v[0].y == 2 && v[0].z == 3 && v[0].rhw == 0.5f);
    CHECK_EQ(v[0].specular, 0xFF102030u);
    CHECK(v[0].u == 0.25f && v[0].v == 0.75f && v[1].x == 4 && v[1].v == 1);
    // Two textures' coordinates: the colour is at the same place.
    Tl2 w = {7, 8, 9, 1, 0xFF808080, 0x11223344, 0.1f, 0.2f, 0.3f, 0.4f};
    effect_exe::soften(reinterpret_cast<uint8_t*>(&w), 1, sizeof(Tl2), 25, effect_exe::Soften::Colour);
    CHECK_EQ(w.diffuse, 0xFF202020u);
    CHECK(w.specular == 0x11223344u && w.u1 == 0.3f && w.v1 == 0.4f);
    // 100 %: as the game made it; 0 %: nothing left.
    Tl1 same = {0, 0, 0, 1, 0xC0A08060, 0, 0, 0};
    effect_exe::soften(reinterpret_cast<uint8_t*>(&same), 1, sizeof(Tl1), 100, effect_exe::Soften::Colour);
    CHECK_EQ(same.diffuse, 0xC0A08060u);
    effect_exe::soften(reinterpret_cast<uint8_t*>(&same), 1, sizeof(Tl1), 0, effect_exe::Soften::Colour);
    CHECK_EQ(same.diffuse, 0xC0000000u);
}

TEST(effect_exe_knows_the_faces_of_an_effect_by_their_vertices) {
    // 0.28: a face the game sorts into its lists while an effect's primitive renders is noted by its vertices - the
    // address its draw gets - with what the effect is and a seed of the primitive's own.
    effect_exe::Faces f;
    CHECK(f.of(0x1000040).kind == Kind::None);
    f.note(0x1000040, effect_exe::Tag{Kind::Firefly, 17});
    f.note(0x2000000, effect_exe::Tag{Kind::Aura, 3});
    CHECK(f.of(0x1000040).kind == Kind::Firefly);
    CHECK_EQ(int(f.of(0x1000040).seed), 17);
    CHECK(f.of(0x2000000).kind == Kind::Aura);
    CHECK(f.of(0x1000044).kind == Kind::None);  // the exact address only
    CHECK(f.of(0).kind == Kind::None);
    f.note(0, effect_exe::Tag{Kind::Aura, 1});  // no vertices: nothing
    CHECK(f.of(0).kind == Kind::None);
    // A new frame: the faces of the next one are noted anew - the memory may hold another face by then.
    f.clear();
    CHECK(f.of(0x1000040).kind == Kind::None);
    CHECK(f.of(0x2000000).kind == Kind::None);
}

TEST(effect_exe_knows_fireflies_and_the_sort_functions_entry) {
    CHECK(effect_exe::kindOf(45) == Kind::Firefly);
    CHECK(effect_exe::kindOf(44) == Kind::None);
    for (int id : {307, 322, 323, 324}) CHECK(effect_exe::kindOf(id) == Kind::Ray);  // 0.30: the forest's light
    CHECK(effect_exe::kindOf(719) == Kind::None);  // forestlight5: the game does not show it (ids past 629)
    // 0.38: the portals - the warp NPC's (321, warpzone2), the skill's (35, 36) and the like.
    for (int id : {321, 61, 35, 36, 316, 317, 341, 389, 402, 561, 562})
        CHECK(effect_exe::kindOf(id) == Kind::Portal);
    // The face sort's first instructions (push ebp; mov ebp, esp; sub esp, 0xC): moved whole into the hook's
    // trampoline. Another build's bytes are refused.
    const uint8_t entry[6] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C};
    CHECK(effect_exe::isSortEntry(entry));
    const uint8_t other[6] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10};
    CHECK(!effect_exe::isSortEntry(other));
    // 0.32: the characters' shadow's (push ebp; mov ebp, esp; sub esp, 0x64).
    const uint8_t shadow[6] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x64};
    CHECK(effect_exe::isShadowEntry(shadow));
    CHECK(!effect_exe::isShadowEntry(entry));
}

TEST(effect_exe_softens_by_how_the_draw_blends) {
    // 0.39.1: an added draw (ONE, ONE) loses colour; one added by its alpha (SRCALPHA, ONE) or laid over the picture
    // (SRCALPHA, INVSRCALPHA) loses alpha - its colour kept; one that multiplies the picture (ZERO, SRCCOLOR or
    // DESTCOLOR, ZERO) goes towards white, which changes nothing - never a darker square; others are left alone.
    using effect_exe::Soften;
    CHECK(effect_exe::softenFor(true, d3d7::BLEND_ONE, d3d7::BLEND_ONE) == Soften::Colour);
    CHECK(effect_exe::softenFor(true, d3d7::BLEND_SRCALPHA, d3d7::BLEND_ONE) == Soften::Alpha);
    CHECK(effect_exe::softenFor(true, d3d7::BLEND_SRCALPHA, d3d7::BLEND_INVSRCALPHA) == Soften::Alpha);
    CHECK(effect_exe::softenFor(true, d3d7::BLEND_ZERO, d3d7::BLEND_SRCCOLOR) == Soften::White);
    CHECK(effect_exe::softenFor(true, d3d7::BLEND_DESTCOLOR, d3d7::BLEND_ZERO) == Soften::White);
    CHECK(effect_exe::softenFor(false, d3d7::BLEND_ONE, d3d7::BLEND_ZERO) == Soften::None);  // opaque
    CHECK(effect_exe::softenFor(true, d3d7::BLEND_INVDESTCOLOR, d3d7::BLEND_ONE) == Soften::None);
    Tl1 v = {0, 0, 0, 1, 0x80C04020, 0, 0, 0};
    effect_exe::soften(reinterpret_cast<uint8_t*>(&v), 1, sizeof(Tl1), 50, Soften::Colour);
    CHECK_EQ(v.diffuse, 0x80602010u);  // colour halved, alpha kept
    v.diffuse = 0x80C04020;
    effect_exe::soften(reinterpret_cast<uint8_t*>(&v), 1, sizeof(Tl1), 50, Soften::Alpha);
    CHECK_EQ(v.diffuse, 0x40C04020u);  // alpha halved, colour kept
    v.diffuse = 0x80000000 | 0x40C020;  // a multiplying draw: dark where it darkens
    effect_exe::soften(reinterpret_cast<uint8_t*>(&v), 1, sizeof(Tl1), 50, Soften::White);
    CHECK_EQ(v.diffuse, 0x80A0E090u);  // half way to white; alpha kept
    v.diffuse = 0xFFFFFFFF;  // white multiplies nothing: stays so
    effect_exe::soften(reinterpret_cast<uint8_t*>(&v), 1, sizeof(Tl1), 50, Soften::White);
    CHECK_EQ(v.diffuse, 0xFFFFFFFFu);
}
