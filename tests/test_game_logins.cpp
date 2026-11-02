// 3.0.11-beta.178 (the login concept: several game windows): the logins the other game windows play now
// (core/game_logins) - this game's own entry and those of games no longer running left out; the first shelf card free
#include <cstdint>
#include <vector>

#include "core/game_logins.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(game_logins_busy_skips_self_and_dead) {
    gamelogins::Table t{};
    t.e[0] = gamelogins::Entry{1, 100, 0xAAAA};  // this game
    t.e[1] = gamelogins::Entry{2, 200, 0xBBBB};  // another, running
    t.e[2] = gamelogins::Entry{3, 300, 0xCCCC};  // another, gone (its pid not alive)
    t.e[3] = gamelogins::Entry{4, 0, 0xDDDD};    // a free entry
    const auto busy = gamelogins::busy(t, 100, {100, 200});
    CHECK(busy.size() == 1 && busy[0].login == 0xBBBB && busy[0].window == 2);
    CHECK(gamelogins::busy(gamelogins::Table{}, 100, {100}).empty());
}

TEST(game_logins_first_free) {
    const std::vector<uint64_t> shelf{0xA, 0xB, 0xC};
    CHECK(gamelogins::firstFree(shelf, {0xA}) == 1);
    CHECK(gamelogins::firstFree(shelf, {}) == 0);
    CHECK(gamelogins::firstFree(shelf, {0xA, 0xB, 0xC}) == -1);
    CHECK(gamelogins::firstFree({}, {}) == -1);
}

// An entry put for this game (its window's number, the login's number), the same one again in its place; taken away
TEST(game_logins_put_and_take) {
    gamelogins::Table t{};
    CHECK(gamelogins::put(t, 100, 1, 0xAAAA) == 0);
    CHECK(gamelogins::put(t, 200, 2, 0xBBBB) == 1);
    CHECK(gamelogins::put(t, 100, 1, 0xCCCC) == 0 && t.e[0].login == 0xCCCC);  // this game's own entry renewed
    gamelogins::take(t, 100);
    CHECK(t.e[0].pid == 0 && t.e[1].pid == 200);
    gamelogins::Table full{};
    for (int i = 0; i < gamelogins::kMost; ++i) CHECK(gamelogins::put(full, uint32_t(10 + i), i + 1, 1) == i);
    CHECK(gamelogins::put(full, 99, 9, 1) == -1);
}
