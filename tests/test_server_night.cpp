#include <cstdint>

#include "core/config.hpp"
#include "core/effect_groups.hpp"
#include "core/server_night.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(server_night_left_out_by_default_a_fix_of_every_set) {
    // 2.6.13: the server's night does not darken the picture - the time of day is «Часы»'s and «Ночь»'s; every
    // switch off (the game's own picture) shows the server's night as the game does
    CHECK(parseConfig("").effects.noServerNight);
    CHECK(!parseConfig("[effects]\nno_server_night = 0\n").effects.noServerNight);
    CHECK(!effectsOff().noServerNight);
    const effect_groups::Member* m = effect_groups::memberOf("effects.no_server_night");
    CHECK(m && m->group == effect_groups::Fixes && m->level == 1);
}

TEST(server_night_darkness_taken_away_for_the_night_alone) {
    bool held = false;
    // the night alone darkens: left out, the byte cleared and the driver holds it so
    CHECK_EQ(int(server_night::darkNow(1, true, false, true, held)), 0);
    CHECK(held);
    CHECK_EQ(int(server_night::darkNow(0, true, false, true, held)), 0);  // and keeps it
    CHECK(held);
    // a Soul Linker's spirit darkens too: the game's own, left as it is
    bool spiritHeld = false;
    CHECK_EQ(int(server_night::darkNow(1, true, true, true, spiritHeld)), 1);
    CHECK(!spiritHeld);
    CHECK_EQ(int(server_night::darkNow(1, false, true, true, spiritHeld)), 1);
    // the switch turned off with the night still on: the game's darkness back
    CHECK_EQ(int(server_night::darkNow(0, true, false, false, held)), 1);
    CHECK(!held);
    // turned off after the night ended: nothing to give back
    held = true;
    CHECK_EQ(int(server_night::darkNow(0, false, false, false, held)), 0);
    CHECK(!held);
    // never touched: the game's byte as it is, whatever it says
    bool never = false;
    CHECK_EQ(int(server_night::darkNow(1, false, false, false, never)), 1);
    CHECK_EQ(int(server_night::darkNow(0, true, false, false, never)), 0);
    CHECK(!never);
}
