#include <cstdint>

#include "core/d3d7.hpp"
#include "core/effect_exe.hpp"
#include "core/ftol_exe.hpp"
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
    // 0.63: what /aura turns on - the ring and bubbles, not the circles
    for (int id : {200, 201, 397, 398}) CHECK(effect_exe::auraExtra(id));
    for (int id : {202, 362, 45, 0}) CHECK(!effect_exe::auraExtra(id));
    // 0.37: the rest are skills and the like - but the maps' own (torches, smoke, bubbles, bats...) and nothing.
    for (int id : {47, 44, 109, 353, 359, 557, -1, 0}) CHECK(effect_exe::kindOf(id) == Kind::None);
    for (int id : {89, 199, 203, 361, 363, 396, 399, 629}) CHECK(effect_exe::kindOf(id) == Kind::Skill);
}

TEST(effect_exe_knows_blindnesss_darkness) {
    // 2.6.18: blindness (334) and the darkness's ten levels (475-484): not a skill made softer (R-MCVUC8 - at the
    // skills' 50 % the walls showed through it), laid over the world once its effects are drawn
    CHECK(effect_exe::kindOf(334) == Kind::Darkness);
    for (int id = 475; id <= 484; ++id) CHECK(effect_exe::kindOf(id) == Kind::Darkness);
    for (int id : {333, 335, 474, 485, 487}) CHECK(effect_exe::kindOf(id) != Kind::Darkness);
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

TEST(effect_exe_knows_an_orbs_sprites_from_their_shadows) {
    // 2.7.27 (2.7.26's log: «18 of Ruwach's sprites, 0 the orb's own» - the sprite cache's cells know no effect's
    // file): a primitive's height over its effect (kPrimLift: Ruwach's orbs -10, their shadows 0, uaRO.exe 0x5CC5D0)
    // goes with its faces, with the effect's number
    CHECK_EQ(effect_exe::kPrimLift, 0x35Cu);
    effect_exe::Faces f;
    effect_exe::Tag orb{Kind::Skill, 5, false, 33};
    orb.raised = true;
    f.note(0x3000000, orb);
    f.note(0x3000020, effect_exe::Tag{Kind::Skill, 6, false, 33});
    CHECK(f.of(0x3000000).raised && f.of(0x3000000).id == 33);
    CHECK(!f.of(0x3000020).raised && f.of(0x3000020).id == 33);
}

TEST(effect_exe_faces_of_a_crowded_frame_and_the_next) {
    // 2.5.20: a flat table, cleared by the frame's stamp - thousands of faces, grown on the way, frame after frame
    effect_exe::Faces f;
    for (int frame = 0; frame < 3; ++frame) {
        for (uint32_t i = 1; i <= 5000; ++i)
            f.note(0x400000 + i * 0x60 + uint32_t(frame) * 4, effect_exe::Tag{Kind::Aura, uint8_t(i + frame)});
        bool all = true;
        for (uint32_t i = 1; i <= 5000; ++i) {
            const effect_exe::Tag t = f.of(0x400000 + i * 0x60 + uint32_t(frame) * 4);
            all = all && t.kind == Kind::Aura && t.seed == uint8_t(i + frame);
        }
        CHECK(all);
        CHECK(f.of(0x400000 + 0x60 + uint32_t(frame) * 4 + 2).kind == Kind::None);
        f.note(0x400060 + uint32_t(frame) * 4, effect_exe::Tag{Kind::Firefly, 9});  // noted again: the last tag
        CHECK(f.of(0x400060 + uint32_t(frame) * 4).kind == Kind::Firefly);
        f.clear();
        CHECK(f.of(0x400060 + uint32_t(frame) * 4).kind == Kind::None);  // the last frame's: gone
    }
}

TEST(effect_exe_knows_a_characters_faces_by_its_shadow) {
    // 2.6.3: uaRO.exe asks for a character's round shadow (FUN_00547b80) right before it sorts the character's
    // sprites into the blended lists (FUN_00548190, FUN_005aa130, the players' FUN_005ae540): those faces are the
    // character's - not an effect's (kind None), with its shadow's seed - until the next character's shadow.
    effect_exe::Owner o;
    const effect_exe::Tag none{};
    CHECK_EQ(int(o.faceTag(none, 0x101).seed), 0);  // before any shadow: nobody's
    o.shadow(0x1234560);
    const uint8_t a = effect_exe::ownerSeed(0x1234560);
    CHECK(a != 0);
    CHECK(effect_exe::ownerSeed(0) != 0);  // never 0: that is nobody's
    const effect_exe::Tag t = o.faceTag(none, 0x101);
    CHECK(t.kind == Kind::None && t.seed == a);
    CHECK_EQ(int(o.faceTag(none, 1).seed), int(a));
    CHECK_EQ(int(o.faceTag(none, 0).seed), 0);  // an opaque face (a model of the map's): not a sprite
    const effect_exe::Tag skill{Kind::Skill, 9};
    CHECK(o.faceTag(skill, 0x101).kind == Kind::Skill && o.faceTag(skill, 0x101).seed == 9);  // an effect's own
    o.shadow(0x7654320);
    CHECK(effect_exe::ownerSeed(0x7654320) != a);
    CHECK_EQ(int(o.faceTag(none, 1).seed), int(effect_exe::ownerSeed(0x7654320)));
    o.clear();  // a new frame
    CHECK_EQ(int(o.faceTag(none, 1).seed), 0);
    // The faces' table keeps them: a character's face (kind None, a seed) is noted; one with neither is not.
    effect_exe::Faces f;
    f.note(0x3000000, effect_exe::Tag{Kind::None, a});
    f.note(0x3000020, effect_exe::Tag{});
    CHECK(f.of(0x3000000).kind == Kind::None && f.of(0x3000000).seed == a);
    CHECK_EQ(int(f.of(0x3000020).seed), 0);
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

TEST(ftol_exe_fast_one_fits_in_the_games_place) {
    // 2.5.20: fisttp in __ftol's own 39 bytes: the code first, then int3 to the end; the game's as this build has it
    CHECK_EQ(int(uagfx::ftol_exe::kOriginal[0]), 0x55);  // push ebp
    CHECK_EQ(int(uagfx::ftol_exe::kOriginal[uagfx::ftol_exe::kSize - 1]), 0xC3);  // ret
    CHECK_EQ(int(uagfx::ftol_exe::kFast[3]), 0xDD);  // fisttp m64int: DD /1
    CHECK_EQ(int(uagfx::ftol_exe::kFast[4]) >> 3 & 7, 1);
    CHECK_EQ(int(uagfx::ftol_exe::kFast[16]), 0xC3);
    bool padded = true;
    for (size_t i = 17; i < uagfx::ftol_exe::kSize; ++i) padded = padded && uagfx::ftol_exe::kFast[i] == 0xCC;
    CHECK(padded);
}

TEST(effect_exe_soft_particles_but_not_a_hit_flash) {
    // 0.67: added light, a skill's or a portal's picture fade into what is behind them. 2.7.13 (the tester: «не
    // вспыхивает» though every hit found its monster, 2.7.12): a monster's hit flash - its sprite drawn again in white,
    // added - was taken for one, and the frame's depth copied with the monster in it put it out wholly
    using effect_exe::Kind;
    CHECK(effect_exe::softEffect(true, uint8_t(Kind::None)));
    CHECK(effect_exe::softEffect(false, uint8_t(Kind::Skill)));
    CHECK(effect_exe::softEffect(false, uint8_t(Kind::Portal)));
    CHECK(!effect_exe::softEffect(false, uint8_t(Kind::None)));
    CHECK(!effect_exe::softEffect(true, uint8_t(Kind::HitFlash)));
    // 2.7.14: nor a card's star on the ground - added light at the card's own depth, put out the same way
    CHECK(!effect_exe::softEffect(true, uint8_t(Kind::Glint)));
}

TEST(effect_exe_skills_softer_at_night) {
    // 2.7.37 (the tester on 2.7.36's Magnus at night: «не нравится, слишком ярко, надо сделать спокойнее»): at night
    // the world goes dark and the skills did not - «Skills' brightness» down to 60 % of itself in the whole night
    // (nightNow 100; «Deep night» 200 the same), by day as it is
    CHECK_EQ(effect_exe::skillPercent(50, 0), 50);
    CHECK_EQ(effect_exe::skillPercent(50, 100), 30);
    CHECK_EQ(effect_exe::skillPercent(50, 200), 30);
    CHECK_EQ(effect_exe::skillPercent(80, 50), 64);
    CHECK_EQ(effect_exe::skillPercent(100, 100), 60);  // softer at night even when the slider is full
    CHECK_EQ(effect_exe::skillPercent(100, 0), 100);
}
