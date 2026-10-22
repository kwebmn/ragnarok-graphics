#include <cmath>

#include "pad/core/logic/targeting.hpp"
#include "pad_test_fixtures.hpp"
#include "test.hpp"

using namespace uapad;

TEST(targeting_nearest_first_monsters_only) {
    WorldState w = fx::world();
    fx::addMob(w, 110000003, Cell{106, 100});  // distance 6
    fx::addMob(w, 110000001, Cell{102, 101});  // distance 2
    fx::addMob(w, 110000002, Cell{97, 104});   // distance 4
    w.apply(EvActor{2000002, 7, 3, 150, Cell{101, 100}, Cell{101, 100}}, 0);          // player
    w.apply(EvActor{110000009, 1002, 100, 200, Cell{101, 101}, Cell{101, 101}}, 0);  // pet
    auto c = targetCandidates(w, 0, {});
    CHECK(c == (std::vector<uint32_t>{110000001, 110000002, 110000003}));
}

TEST(targeting_ties_broken_by_id) {
    WorldState w = fx::world();
    fx::addMob(w, 110000020, Cell{103, 100});
    fx::addMob(w, 110000010, Cell{100, 97});
    auto c = targetCandidates(w, 0, {});
    CHECK(c == (std::vector<uint32_t>{110000010, 110000020}));
}

TEST(targeting_nearest_is_by_straight_line) {
    WorldState w = fx::world();  // we stand at (100, 100)
    fx::addMob(w, 110000001, Cell{105, 105});  // 5 RO steps, but about 7 cells in a straight line
    fx::addMob(w, 110000002, Cell{106, 100});  // 6 either way
    CHECK(targetCandidates(w, 0, {}) == (std::vector<uint32_t>{110000002, 110000001}));
}

TEST(targeting_candidates_stay_within_reach) {
    WorldState w = fx::world();  // we stand at (100, 100)
    fx::addMob(w, 110000001, Cell{115, 100});  // 15 cells: in
    fx::addMob(w, 110000002, Cell{116, 100});  // 16: off the screen, not a candidate
    fx::addMob(w, 110000003, Cell{90, 130});   // 30
    CHECK(targetCandidates(w, 0, {}) == (std::vector<uint32_t>{110000001}));
}

TEST(targeting_blacklist_until_expiry) {
    WorldState w = fx::world();
    fx::addMob(w, 110000001, Cell{101, 100});
    fx::addMob(w, 110000002, Cell{103, 100});
    std::unordered_map<uint32_t, uint32_t> blacklist{{110000001, 5000}};
    CHECK(targetCandidates(w, 4999, blacklist) == (std::vector<uint32_t>{110000002}));
    CHECK(targetCandidates(w, 5000, blacklist) == (std::vector<uint32_t>{110000001, 110000002}));
}

TEST(targeting_uses_interpolated_positions) {
    WorldState w = fx::world();
    w.apply(EvActor{110000001, 1002, 0, 100, Cell{110, 100}, Cell{101, 100}}, 0);  // walking towards us
    fx::addMob(w, 110000002, Cell{104, 100});
    CHECK(targetCandidates(w, 0, {}).front() == 110000002u);
    CHECK(targetCandidates(w, 800, {}).front() == 110000001u);  // 8 cells walked: now at (102, 100)
}

TEST(targeting_give_up_after_silence) {
    WorldState w = fx::world();
    fx::addMob(w, 110000001, Cell{102, 100});
    CHECK(!shouldGiveUp(w, 110000001, 1000, 4999, 4000));
    CHECK(shouldGiveUp(w, 110000001, 1000, 5000, 4000));
    w.apply(EvAction{fx::kSelf, 110000001, 0}, 4500);
    CHECK(!shouldGiveUp(w, 110000001, 1000, 8000, 4000));
    CHECK(shouldGiveUp(w, 110000001, 1000, 8500, 4000));
}

TEST(targeting_pick_in_direction) {
    WorldState w = fx::world();  // we stand at (100, 100)
    fx::addMob(w, 110000001, Cell{105, 100});  // east
    fx::addMob(w, 110000002, Cell{100, 106});  // north
    fx::addMob(w, 110000003, Cell{95, 101});   // west
    CHECK_EQ(pickInDirection(w, 0, 1, 0, {}), 110000001u);
    CHECK_EQ(pickInDirection(w, 0, 0, 3, {}), 110000002u);  // any length
    CHECK_EQ(pickInDirection(w, 0, -1, 0, {}), 110000003u);
    CHECK_EQ(pickInDirection(w, 0, 1, 0.2, {}), 110000001u);
    CHECK_EQ(pickInDirection(w, 0, 0, -1, {}), 0u);  // nothing to the south
    CHECK_EQ(pickInDirection(w, 0, 0, 0, {}), 0u);   // no direction at all
}

TEST(targeting_aim_prefers_angle_over_distance) {
    WorldState w = fx::world();
    fx::addMob(w, 110000001, Cell{103, 102});  // 33.7 deg off east, 3.6 cells: score 40.9
    fx::addMob(w, 110000002, Cell{110, 101});  // 5.7 deg off east, 10 cells: score 25.8
    CHECK_EQ(pickInDirection(w, 0, 1, 0, {}), 110000002u);
    auto close = aimScore(w, 0, 110000001, 1, 0);
    auto far = aimScore(w, 0, 110000002, 1, 0);
    CHECK(close && far && *far < *close);
    CHECK(far && std::fabs(*far - (5.7106 + 2 * 10.0499)) < 0.01);
    fx::addMob(w, 110000000, Cell{110, 99});  // the mirror image of 110000002: same score, lower id
    CHECK_EQ(pickInDirection(w, 0, 1, 0, {}), 110000000u);
}

TEST(targeting_aim_skips_blacklisted_own_cell_far_and_non_monsters) {
    WorldState w = fx::world();
    fx::addMob(w, 110000001, Cell{100, 100});  // on our own cell: no direction to it
    fx::addMob(w, 110000002, Cell{116, 100});  // 16 cells: out of reach
    fx::addMob(w, 110000003, Cell{104, 100});  // blacklisted
    w.apply(EvActor{2000002, 7, 3, 150, Cell{103, 100}, Cell{103, 100}}, 0);  // a player
    const std::unordered_map<uint32_t, uint32_t> bl{{110000003, 5000}};
    CHECK_EQ(pickInDirection(w, 0, 1, 0, bl), 0u);
    CHECK(!aimScore(w, 0, 110000001, 1, 0));
    CHECK(!aimScore(w, 0, 110000002, 1, 0));
    CHECK(!aimScore(w, 0, 110000003, 0, 1));  // 90 degrees off
    CHECK(aimScore(w, 0, 110000003, 1, 0));   // the score itself knows nothing of the blacklist
    CHECK(!aimScore(w, 0, 999, 1, 0));        // unknown actor
    fx::addMob(w, 110000004, Cell{115, 114});  // 15 cells away diagonally: still in reach
    CHECK_EQ(pickInDirection(w, 0, 1, 1, bl), 110000004u);
}
