#include <algorithm>
#include <cmath>
#include <vector>

#include "core/config.hpp"
#include "core/foliage.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(foliage_knows_leaves_from_walls_and_fences) {
    // 0.64: green with holes - leaves; a solid wall, a grey fence with holes, a green solid field - not.
    std::vector<uint32_t> leaves(32 * 32), wall(32 * 32, 0xFF806040u), fence(32 * 32), field(32 * 32, 0xFF308020u);
    for (int i = 0; i < 32 * 32; ++i) {
        const bool hole = (i * 7 % 10) < 4;
        leaves[size_t(i)] = hole ? 0x00000000u : 0xFF2E7A22u;
        fence[size_t(i)] = hole ? 0x00000000u : 0xFF707070u;
    }
    CHECK(foliage::leafy(leaves.data(), 32, 32));
    CHECK(!foliage::leafy(wall.data(), 32, 32));
    CHECK(!foliage::leafy(fence.data(), 32, 32));
    CHECK(!foliage::leafy(field.data(), 32, 32));
    CHECK(!foliage::leafy(leaves.data(), 4, 4));
}

TEST(foliage_a_sprite_cache_page_is_never_leaves) {
    // 2.6.19: "standing in the Pyramids, some leaves are falling" - a page of the game's sprite cache full of green
    // Poporings on transparent cells looked like leaves (their frames' mean colours about 187, 203, 144, a hair on
    // either side of the line) and they shed them. The characters' and monsters' frames are never a tree's.
    std::vector<uint32_t> page(32 * 32);
    for (int i = 0; i < 32 * 32; ++i) page[size_t(i)] = (i * 7 % 10) < 4 ? 0x00000000u : 0xFFBBCB90u;
    CHECK(foliage::leafy(page.data(), 32, 32));
    CHECK(foliage::leafyTexture(page.data(), 32, 32, false));
    CHECK(!foliage::leafyTexture(page.data(), 32, 32, true));
}

TEST(foliage_swings_far_enough_to_be_seen) {
    // 2.7.30 (the tester in Payon's forest: «Листя на вітрі не вижу»): a tree's top swung 1.2 game pixels at 100 % -
    // under a pixel or two of the picture; now 4
    CHECK(foliage::kSwing == 4.0f);
}


// 3.0.11-beta.300 (the players: «в пронтере лавочка вместе с травой движется»): Prontera's wrought bench, painted a
// blue green with its gaps keyed out (pron-bench1.bmp: 28, 78, 44), is no leaves; a yellow green one with the same
// holes (mo-leaves1.bmp: 41, 73, 18) and the bluest real leaves (mo-v-3.bmp: 27, 47, 29) still are
TEST(foliage_a_blue_green_bench_is_no_leaves) {
    const auto with = [](uint32_t colour) {
        std::vector<uint32_t> v(32 * 32);
        for (int i = 0; i < 32 * 32; ++i) v[size_t(i)] = (i * 7 % 10) < 4 ? 0x00000000u : colour;
        return v;
    };
    CHECK(!foliage::leafy(with(0xFF1C4E2Cu).data(), 32, 32));
    CHECK(foliage::leafy(with(0xFF294912u).data(), 32, 32));
    CHECK(foliage::leafy(with(0xFF1B2F1Du).data(), 32, 32));
}

// 3.0.20-beta.48 (the owner's «делай 1-9», 6; the audit's question 4): the swing is a number of the game's pixels,
// sized for the top view - every tree some 350 from its narrow lens. From the eyes' level a tree at 30 and a tree at
// 800 swung as many pixels: the far crowns tossed about. In the 3D view a tree swings as far in the world as in the
// top view, wherever it stands
TEST(foliage_a_tree_swings_as_far_in_the_world_wherever_it_stands) {
    const float swing = foliage::kSwing;
    // the top view's own
    CHECK(std::fabs(foliage::swingAt(swing, foliage::kUsualDepth, foliage::kUsualFov) - swing) < 1e-3f);
    const float wide = foliage::swingAt(swing, 350.0f, 60.0f);  // a wide lens shows the same swing in fewer pixels
    CHECK(std::fabs(wide - swing * 0.228f) < 0.02f);
    CHECK(std::fabs(foliage::swingAt(swing, 175.0f, 60.0f) - 2.0f * wide) < 1e-3f);  // half as far: twice the pixels
    CHECK(std::fabs(foliage::swingAt(swing, 1400.0f, 60.0f) - 0.25f * wide) < 1e-3f);
    CHECK(foliage::swingAt(swing, 800.0f, 60.0f) < foliage::swingAt(swing, 30.0f, 60.0f) * 0.1f);
    // a crown at the eye: no more than kMostSwing times the top view's pixels; a depth of nothing likewise
    CHECK(std::fabs(foliage::swingAt(swing, 5.0f, 60.0f) - swing * foliage::kMostSwing) < 1e-3f);
    CHECK(std::fabs(foliage::swingAt(swing, 0.0f, 60.0f) - swing * foliage::kMostSwing) < 1e-3f);
    CHECK(foliage::swingAt(0.0f, 100.0f, 60.0f) == 0.0f);
}

// 3.0.20-beta.51 (the owner's «делай 1-9», 4 - denser crowns in the 3D view): a tree's crown is a few flat cards of
// leaves - from above they cover one another, from the eyes' level their edges and the gaps between them show. Its
// leaves drawn twice more, turned about the crown's middle (its upright line) a sixth and a third of a turn and a
// little smaller - on prt_fild08's commonest tree (나무01, 64 faces) the gaps close
// (/root/uagfx-tools/audit/sunshadow/crown_tree01.png)
TEST(foliage_a_crowns_leaves_again_turned_about_its_middle) {
    CHECK(foliage::kCrownCopies == 2);
    float turn[3];
    foliage::crownTurn(0, turn);
    CHECK(std::fabs(turn[0] - 0.5f) < 1e-5f && std::fabs(turn[1] - 0.8660254f) < 1e-5f && turn[2] == 0.94f);
    foliage::crownTurn(1, turn);
    CHECK(std::fabs(turn[0] + 0.5f) < 1e-5f && std::fabs(turn[1] - 0.8660254f) < 1e-5f && turn[2] == 0.88f);
    const float middle[3] = {100.0f, -30.0f, 50.0f};
    float p[3] = {110.0f, -40.0f, 50.0f};  // ten aside, ten over the middle (up along -y)
    foliage::crownTurn(0, turn);
    foliage::turnedAbout(middle, turn, p);
    // a sixth of a turn about the upright line, 0.94 of the way from the middle
    CHECK(std::fabs(p[0] - (100.0f + 9.4f * 0.5f)) < 1e-4f && std::fabs(p[2] - (50.0f + 9.4f * 0.8660254f)) < 1e-4f);
    CHECK(std::fabs(p[1] - (-30.0f - 9.4f)) < 1e-4f);
    float at[3] = {100.0f, -30.0f, 50.0f};  // the middle stays
    foliage::turnedAbout(middle, turn, at);
    CHECK(at[0] == 100.0f && at[1] == -30.0f && at[2] == 50.0f);
    const float none[3] = {1.0f, 0.0f, 0.0f};  // no size: as it is (the leaves' first drawing)
    float same[3] = {7.0f, 8.0f, 9.0f};
    foliage::turnedAbout(middle, none, same);
    CHECK(same[0] == 7.0f && same[1] == 8.0f && same[2] == 9.0f);
}

TEST(config_view3d_dense_crowns_off_by_default_and_live) {
    CHECK(!parseConfig("").effects.denseCrowns);
    CHECK(parseConfig("[view3d]\ndense_crowns = 1\n").effects.denseCrowns);
    const settings::IniSetting* row = settings::findSetting("view3d.dense_crowns");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && row->control == settings::Control::Check);
    CHECK(appliesNow("view3d.dense_crowns"));
}

// 3.0.20-beta.61: the models' dump tells a texture's share of holes with a face of it not found in the picture
TEST(foliage_a_pictures_share_of_holes) {
    std::vector<uint32_t> px(100, 0xFF808080u);
    CHECK_EQ(foliage::holesPercent(px.data(), 10, 10), 0);
    for (int i = 0; i < 77; ++i) px[size_t(i)] = 0x00000000u;
    CHECK_EQ(foliage::holesPercent(px.data(), 10, 10), 77);
    px[99] = 0x7F123456u;  // under a half: a hole too
    CHECK_EQ(foliage::holesPercent(px.data(), 10, 10), 78);
    CHECK_EQ(foliage::holesPercent(nullptr, 10, 10), 0);
}

// 3.0.20-beta.73 (the owner on the 3D view: «деревья очень примитивные, когда сверху не так, но когда в 3д там видно
// все недостатки - это не деревья, можно что-то сделать?»): a tree's leaves are flat plates in tiers, made for the
// view from above - from the eyes' level they are seen edge-on, the sky between them. Each leaf face is drawn twice
// more, stood up on the line from the trunk through its own middle (one copy up, one down), a little smaller: the
// tiers get a body (tried on the game's own trees: /root/uagfx-tools/audit/trees/crowns2.py)
TEST(foliage_a_crowns_leaf_face_stood_on_the_trunks_line) {
    CHECK(foliage::kCrownStood == 2);
    float turn[3];
    foliage::crownStood(0, turn);
    CHECK(turn[0] == 1.0f && turn[1] == 0.0f && turn[2] == -foliage::kStoodSize);  // a size under 0: stood, not turned
    foliage::crownStood(1, turn);
    CHECK(turn[0] == -1.0f && turn[2] == -foliage::kStoodSize);
    // a flat face of a tier, ten out from the trunk along +x (the crown's middle at 100, -30, 50; up along -y)
    const float middle[3] = {100.0f, -30.0f, 50.0f};
    float a[3] = {108.0f, -30.0f, 50.0f}, b[3] = {114.0f, -30.0f, 44.0f}, c[3] = {114.0f, -30.0f, 56.0f};
    const float mid[3] = {(a[0] + b[0] + c[0]) / 3, -30.0f, (a[2] + b[2] + c[2]) / 3};
    foliage::crownStood(0, turn);
    CHECK(foliage::stoodAbout(middle, turn, a, b, c));
    // its middle stays; it stands: its corners keep their place along the trunk's line and leave the tier's plane
    CHECK(std::fabs((a[0] + b[0] + c[0]) / 3 - mid[0]) < 1e-4f);
    CHECK(std::fabs((a[1] + b[1] + c[1]) / 3 - mid[1]) < 1e-4f);
    CHECK(std::fabs((a[2] + b[2] + c[2]) / 3 - mid[2]) < 1e-4f);
    CHECK(std::fabs(b[2] - 50.0f) < 1e-4f && std::fabs(c[2] - 50.0f) < 1e-4f);  // no longer aside: over and under
    CHECK(std::fabs(std::fabs(b[1] - c[1]) - 12.0f * foliage::kStoodSize) < 1e-3f);  // its width now its height
    CHECK(std::fabs(a[1] + 30.0f) < 1e-4f);  // the corner on the line stays in the tier
    CHECK(std::fabs((b[0] - mid[0]) - (114.0f - mid[0]) * foliage::kStoodSize) < 1e-3f);  // and a little smaller
    // the other copy: the other way up
    float a2[3] = {108.0f, -30.0f, 50.0f}, b2[3] = {114.0f, -30.0f, 44.0f}, c2[3] = {114.0f, -30.0f, 56.0f};
    foliage::crownStood(1, turn);
    CHECK(foliage::stoodAbout(middle, turn, a2, b2, c2));
    CHECK(std::fabs((b2[1] + 30.0f) + (b[1] + 30.0f)) < 1e-3f);
    // no copy (false, the corners as they are): a face on the trunk's own line (no way out from it); a face that
    // stands already - a bush's upright leaf, a hanging frond (its plane steeper than kStoodFlat tells); a turn that
    // is no stood copy's
    float t0[3] = {100.0f, -40.0f, 50.0f}, t1[3] = {100.0f, -41.0f, 50.0f}, t2[3] = {100.0f, -42.0f, 50.0f};
    CHECK(!foliage::stoodAbout(middle, turn, t0, t1, t2));
    CHECK(t1[1] == -41.0f);
    float s0[3] = {110.0f, -30.0f, 50.0f}, s1[3] = {110.0f, -40.0f, 50.0f}, s2[3] = {116.0f, -35.0f, 50.0f};  // upright
    CHECK(!foliage::stoodAbout(middle, turn, s0, s1, s2));
    CHECK(s1[1] == -40.0f && s2[0] == 116.0f);
    float d0[3] = {108.0f, -30.0f, 50.0f}, d1[3] = {114.0f, -27.0f, 44.0f}, d2[3] = {114.0f, -27.0f, 56.0f};  // drooping
    CHECK(foliage::stoodAbout(middle, turn, d0, d1, d2));  // a tier's plate hangs a little and is flat enough
    float plain[3] = {0.5f, 0.8660254f, 0.94f};
    float p0[3] = {108.0f, -30.0f, 50.0f}, p1[3] = {114.0f, -30.0f, 44.0f}, p2[3] = {114.0f, -30.0f, 56.0f};
    CHECK(!foliage::stoodAbout(middle, plain, p0, p1, p2));
    CHECK(p1[2] == 44.0f);
}

TEST(config_view3d_crown_volume_off_by_default_and_live) {
    CHECK(!parseConfig("").effects.crownVolume);
    CHECK(parseConfig("[view3d]\ncrown_volume = 1\n").effects.crownVolume);
    const settings::IniSetting* row = settings::findSetting("view3d.crown_volume");
    CHECK(row && row->flag == &EffectSettings::crownVolume);
}
