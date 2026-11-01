// 3.0.11-beta.141 (the owner: «блок с персонажем должен работать от альт-V, там должен быть прогресбар с бейз опытом и
// джоб опытом ... точное количество опыта. и побольше подробностей»): the experience gained as the character plays
// (core/exp_rate) - each change of the level's exp made a gain, across a new level too; its pace an hour over the last
// ten minutes; the time to the next level
#include "core/exp_rate.hpp"
#include "test.hpp"

using namespace uagfx;

// More in the same level: the difference; a level more: the rest of the old level's and the new one's; less (a death's
// loss): none, counted on from there; a level less (a new job): none
TEST(exp_rate_gains_in_a_level_and_across_one) {
    exprate::Track t;
    t.feed(0, 10, 1000, 5000);  // the first reading: where it stands
    CHECK(t.session() == 0 && t.last() == 0);
    t.feed(1000, 10, 1500, 5000);
    CHECK(t.session() == 500 && t.last() == 500);
    t.feed(2000, 11, 200, 6000);  // a new level: 3 500 to its end, 200 into the next
    CHECK(t.last() == 3700 && t.session() == 4200);
    t.feed(3000, 11, 150, 6000);  // a death's loss
    CHECK(t.session() == 4200 && t.last() == 3700);
    t.feed(4000, 11, 400, 6000);
    CHECK(t.last() == 250 && t.session() == 4450);
    t.feed(5000, 1, 0, 30);  // a new job: its level 1
    t.feed(6000, 1, 20, 30);
    CHECK(t.last() == 20 && t.session() == 4470);
    t.reset();
    CHECK(t.session() == 0 && t.last() == 0);
}

// A reading of 0 in the same level after more (the game's numbers not come yet) is not taken: the next one counts from
// the one before it, not from 0; nothing known (level 0) is not taken either
TEST(exp_rate_a_passing_zero_not_taken) {
    exprate::Track t;
    t.feed(0, 10, 1000, 5000);
    t.feed(100, 10, 0, 5000);
    t.feed(200, 0, 0, 0);
    t.feed(300, 10, 1100, 5000);
    CHECK(t.last() == 100 && t.session() == 100);
}

// The pace an hour: the gains of the last ten minutes over the time measured (ten minutes, or since the first reading
// when that was later); not known before a minute; none gained in the window: 0
TEST(exp_rate_an_hour_over_its_window) {
    exprate::Track t;
    t.feed(0, 50, 0, 1000000);
    CHECK(t.perHour(30000) < 0);  // half a minute: not known yet
    long long exp = 0;
    for (uint64_t ms = 10000; ms <= 120000; ms += 10000) t.feed(ms, 50, exp += 1000, 1000000);
    CHECK(t.perHour(120000) > 359999 && t.perHour(120000) < 360001);  // 12 000 in 2 minutes
    t.feed(130000, 50, exp += 1000, 1000000);
    CHECK(t.perHour(540000) > 0 && t.perHour(540000) < 360000);  // 9 minutes on: 13 000 over them
    CHECK(t.perHour(30 * 60000) == 0);                            // half an hour later, nothing since
}

// The time to the next level at a pace: minutes, rounded up; not known with no pace or at the last level
TEST(exp_rate_time_to_the_level) {
    CHECK(exprate::minutesTo(1000, 5000, 4000.0) == 60);
    CHECK(exprate::minutesTo(0, 100, 7000.0) == 1);
    CHECK(exprate::minutesTo(1000, 5000, 0.0) < 0 && exprate::minutesTo(1000, 5000, -1.0) < 0);
    CHECK(exprate::minutesTo(1000, 0, 100.0) < 0);
}
