// 3.0.16-beta.21 (the owner's pick 7 of «20 идей»): time rings for the character's own walls and traps
#include "core/ground_timers.hpp"
#include "core/hotbar_panel.hpp"
#include <cmath>

#include "test.hpp"

using namespace uagfx;

TEST(ground_timers_the_servers_durations) {
    CHECK_EQ(groundtimers::durationMs(12, 10), 50000);  // Safety Wall lv 10
    CHECK_EQ(groundtimers::durationMs(25, 1), 10000);   // Pneuma
    CHECK_EQ(groundtimers::durationMs(70, 1), 4000);    // Sanctuary
    CHECK_EQ(groundtimers::durationMs(70, 10), 31000);
    CHECK_EQ(groundtimers::durationMs(117, 5), 50000);  // Ankle Snare lv 5
    CHECK_EQ(groundtimers::durationMs(123, 3), 60000);  // Claymore lv 3
    CHECK_EQ(groundtimers::durationMs(5, 10), 0);
    CHECK_EQ(groundtimers::skillOfUnit(0x85), 25);
    CHECK_EQ(groundtimers::skillOfUnit(0x7f), 0);  // Fire Wall: none here
}

TEST(ground_timers_a_cast_rings_until_its_time_or_its_units_go) {
    groundtimers::Timers t;
    const uint32_t me = 2000001;
    t.asked(25, 1, 1000);  // Pneuma asked: its nine cells come
    for (int k = 0; k < 9; ++k) {
        groundunits::Seen s;
        s.unit = 500 + uint32_t(k);
        s.caster = me;
        s.x = 100 + k % 3;
        s.y = 50 + k / 3;
        s.type = 0x85;
        t.seen(s, me, 1080 + uint64_t(k));
    }
    std::vector<groundtimers::Ring> r = t.rings(6080);
    CHECK(r.size() == 1);
    CHECK(std::fabs(r[0].x - 101.5f) < 1e-4f && std::fabs(r[0].y - 51.5f) < 1e-4f);
    CHECK(r[0].radius > 1.8f && r[0].radius < 2.0f);
    CHECK(std::fabs(r[0].left(6080) - 0.5f) < 1e-3f);
    CHECK(t.rings(11081).empty());  // its 10 s over
    for (int k = 0; k < 9; ++k) t.gone(500 + uint32_t(k));
    CHECK(t.rings(2000).empty());
    // a trap of ours seen with no cast asked (back in sight): no ring; another's: none
    groundunits::Seen trap;
    trap.unit = 900;
    trap.caster = me;
    trap.type = 0x91;
    t.seen(trap, me, 20000);
    CHECK(t.rings(20001).empty());
    t.asked(117, 5, 30000);
    trap.unit = 901;
    trap.caster = 7;
    t.seen(trap, me, 30050);
    CHECK(t.rings(30051).empty());
    trap.caster = me;
    t.seen(trap, me, 30060);
    CHECK(t.rings(30061).size() == 1);
}

// 3.0.16-beta.22: the time's plate in the slot's middle, as wide as its words
TEST(ground_timers_the_slots_plate) {
    const panel::Box cell{100, 50, 40, 40};
    const panel::Box b = hotbar::timerBox(cell, "32", 1.0f);
    CHECK(std::fabs((b.x + b.w / 2) - 120.0f) < 0.01f && std::fabs((b.y + b.h / 2) - 70.0f) < 0.01f);
    CHECK(hotbar::timerBox(cell, "32 \xC3\x97" "2", 1.0f).w > b.w);
}
