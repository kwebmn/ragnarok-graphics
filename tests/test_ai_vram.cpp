#include <cstdint>
#include <vector>

#include "core/ai/vram.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(ai_vram_puts_away_the_copies_not_seen_for_longest) {
    // 0.44: over the budget, the AI copies of the textures not drawn for longest go first, until a tenth under it;
    // one drawn in the last frames stays, also when the budget is short of it.
    std::vector<ai::Copy> copies = {{1, 400, 100}, {2, 300, 5}, {3, 200, 50}, {4, 100, 199}, {5, 500, 200}};
    // 1500 bytes, budget 1000: down to 900 - the copies of 2 (frame 5) and 3 (frame 50) and 1 (frame 100).
    const auto away = ai::putAway(copies, 1500, 1000, 200, 30);
    CHECK_EQ(away.size(), size_t(3));
    CHECK(away.size() == 3 && away[0] == 2 && away[1] == 3 && away[2] == 1);
    // Under the budget: nothing.
    CHECK(ai::putAway(copies, 900, 1000, 200, 30).empty());
    // No budget (not known): nothing.
    CHECK(ai::putAway(copies, 1500, 0, 200, 30).empty());
    // All seen of late: nothing can go.
    CHECK(ai::putAway(copies, 1500, 1000, 110, 200).empty());
    // Just enough: the oldest alone.
    const auto one = ai::putAway(copies, 1200, 1100, 200, 30);
    CHECK(one.size() == 1 && one[0] == 2);
}

TEST(ai_vram_takes_a_share_of_what_the_system_gives) {
    // The copies keep to 40 % of the video memory the system gives the game (DXGI's budget), at least 256 MB; none
    // known: 1 GB.
    const uint64_t mb = 1024ull * 1024;
    CHECK_EQ(ai::copiesBudget(16000 * mb), 6400 * mb);
    CHECK_EQ(ai::copiesBudget(2000 * mb), 800 * mb);
    CHECK_EQ(ai::copiesBudget(500 * mb), 256 * mb);
    CHECK_EQ(ai::copiesBudget(0), 1024 * mb);
}
