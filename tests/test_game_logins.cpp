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

// 3.0.21-beta.5 (R-KQRYFF: a tester's second game window entered with the account the first one played; the first
// was thrown out with «another one logged in with this account» and the driver reported a lost connection): the
// window that plays a login - the login card says so under its field before «Увійти» is pressed
TEST(game_logins_window_that_plays_a_login) {
    const std::vector<gamelogins::Busy> others{{0xAAAA, 2}, {0xBBBB, 3}};
    CHECK_EQ(gamelogins::windowOf(others, 0xBBBB), 3);
    CHECK_EQ(gamelogins::windowOf(others, 0xCCCC), 0);
    CHECK_EQ(gamelogins::windowOf({}, 0xAAAA), 0);
}

// A login taken over: the game whose login the server accepts while another game of this session plays it leaves a
// line; the game thrown out finds it by its login and the time of the server's notice - the player's own other
// window, no drop to report
TEST(game_logins_taken_over_by_another_window) {
    gamelogins::TakenTable t{};
    CHECK_EQ(gamelogins::takenBy(t, 0xAAAA, 100, 50000, 15000), 0);  // nothing put
    gamelogins::putTaken(t, 0xAAAA, 2, 200, 50300);                  // window 2 (process 200), 0.3 s after the notice
    CHECK_EQ(gamelogins::takenBy(t, 0xAAAA, 100, 50000, 15000), 2);
    CHECK_EQ(gamelogins::takenBy(t, 0xAAAA, 100, 50600, 15000), 2);  // or a little before the notice came
    CHECK_EQ(gamelogins::takenBy(t, 0xBBBB, 100, 50000, 15000), 0);  // another login
    CHECK_EQ(gamelogins::takenBy(t, 0xAAAA, 200, 50000, 15000), 0);  // not by this game itself
    CHECK_EQ(gamelogins::takenBy(t, 0xAAAA, 100, 90000, 15000), 0);  // long before the notice: another matter
    CHECK_EQ(gamelogins::takenBy(t, 0xAAAA, 100, 20000, 15000), 0);  // long after it
    // several lines of a login: the nearest in time
    gamelogins::putTaken(t, 0xAAAA, 3, 300, 52000);
    CHECK_EQ(gamelogins::takenBy(t, 0xAAAA, 100, 51800, 15000), 3);
    CHECK_EQ(gamelogins::takenBy(t, 0xAAAA, 100, 50400, 15000), 2);
    // a full table: the oldest line's place taken
    for (int i = 0; i < gamelogins::kMost + 2; ++i) gamelogins::putTaken(t, uint64_t(0xD000 + i), 4, 400, 60000 + i);
    CHECK_EQ(gamelogins::takenBy(t, uint64_t(0xD000 + gamelogins::kMost + 1), 100, 60000, 15000), 4);
    CHECK_EQ(gamelogins::takenBy(t, 0xAAAA, 100, 51800, 15000), 0);  // pushed out by the newer ones
}
