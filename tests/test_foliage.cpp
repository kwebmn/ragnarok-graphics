#include <algorithm>
#include <cmath>
#include <vector>

#include "core/foliage.hpp"
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

TEST(foliage_sways_the_top_not_the_trunk) {
    CHECK(foliage::sway(100, 200, 200, 100, 2.0f, 1.0f) == 0.0f);  // at the bottom
    float most = 0.0f;
    for (float t = 0.0f; t < 10.0f; t += 0.1f)
        most = std::max(most, std::fabs(foliage::sway(100, 100, 200, 100, 2.0f, t)));
    CHECK(most > 1.0f && most <= 2.0f);
    CHECK(foliage::sway(100, 100, 200, 0.5f, 2.0f, 1.0f) == 0.0f);  // no height
}

TEST(foliage_swings_far_enough_to_be_seen) {
    // 2.7.30 (the tester in Payon's forest: «Листя на вітрі не вижу»): a tree's top swung 1.2 game pixels at 100 % -
    // under a pixel or two of the picture; now 4
    CHECK(foliage::kSwing == 4.0f);
}

