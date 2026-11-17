#include "core/world_words.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.20-beta.85 (R-H3S9MY «ник персонажа который ходит гдето не рядом с ним а на верху, также непонятно с мобами
// бывает такое»): in the 3D view the words over the heads and the hovered one's name placed as the game places them,
// from the actor's feet as the 3D view's camera puts them on the screen this frame

namespace {
const worldwords::Screen kScreen{2194, 1234};
}

TEST(world_words_a_shout_over_the_head_as_the_game_places_it) {
    // FUN_0054ac00: x = feet - w / 2, y = feet - (H * 81 / 480) * scale - h; 1234 * 81 / 480 = 208
    const worldwords::Point p = worldwords::wordsAt(1000.4f, 600.7f, 0.5f, 120, 20, kScreen);
    CHECK_EQ(p.x, 940);
    CHECK_EQ(p.y, 477);  // 601 - 104 - 20
    // kept on the screen as the game keeps it: 3 pixels out at most
    const worldwords::Point edge = worldwords::wordsAt(10.0f, 5.0f, 1.0f, 120, 20, kScreen);
    CHECK_EQ(edge.x, -3);
    CHECK_EQ(edge.y, -3);
}

TEST(world_words_the_hovered_name_by_the_game_s_four_places) {
    // FUN_00567690 by the game mode's + 0x40C: 0 under the feet, 1 to the left, 2 to the right, 3 over the head
    const worldwords::Point under = worldwords::nameAt(0, 1000.4f, 600.7f, 0.5f, 120, 20, kScreen);
    CHECK_EQ(under.x, 940);
    CHECK_EQ(under.y, 626);  // 601 + (20 * 1234 / 480 = 51) * 0.5
    const worldwords::Point left = worldwords::nameAt(1, 1000.4f, 600.7f, 0.5f, 120, 20, kScreen);
    CHECK_EQ(left.x, 889);   // 1000 + (-30 * 2194 / 640 = -102) * 0.5 - 60
    CHECK_EQ(left.y, 559);   // 601 + (-25 * 1234 / 480 = -64) * 0.5 - 10
    const worldwords::Point right = worldwords::nameAt(2, 1000.4f, 600.7f, 0.5f, 120, 20, kScreen);
    CHECK_EQ(right.x, 991);  // 1000 + 102 * 0.5 - 60
    CHECK_EQ(right.y, 559);
    const worldwords::Point over = worldwords::nameAt(3, 1000.4f, 600.7f, 0.5f, 120, 20, kScreen);
    CHECK_EQ(over.x, 940);
    CHECK_EQ(over.y, 473);   // 601 + (-100 * 1234 / 480 = -257) * 0.5
    const worldwords::Point odd = worldwords::nameAt(7, 1000.4f, 600.7f, 0.5f, 120, 20, kScreen);  // as 0
    CHECK_EQ(odd.y, 626);
}

TEST(world_words_the_scale_as_the_game_s_sprites) {
    // FUN_00412d60 (rhw * f0 * [0x695798]) over [0x6D5EFC], as the game's own renders keep it at the actor's + 0x48
    CHECK_EQ(worldwords::scaleOf(0.01f, 1000.0f, 0.5f, 2.5f), 2.0f);
    CHECK_EQ(worldwords::scaleOf(0.01f, 1000.0f, 0.5f, 0.0f), 0.0f);  // no norm read: none
}
