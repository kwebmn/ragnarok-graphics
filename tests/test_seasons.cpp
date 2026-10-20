#include "core/seasons.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(seasons_by_the_month_and_the_place) {
    living::Place field;
    CHECK(seasons::of("prt_fild08", 1, true, field).snow > 0.5f);
    CHECK(seasons::of("prt_fild08", 10, true, field).dry > 0.3f);  // 2.4: the leaves on the ground are gone
    CHECK(seasons::of("prt_fild08", 4, true, field).lush > 0.5f);
    CHECK(seasons::of("prt_fild08", 7, true, field).snow == 0.0f);
    CHECK(seasons::of("prt_fild08", 1, false, field).snow == 0.0f);  // indoors
    CHECK(seasons::of("cmd_fild01", 1, true, field).snow == 0.0f);   // Comodo: no winter
    living::Place desert;
    desert.sand = true;
    CHECK(seasons::of("moc_fild01", 1, true, desert).snow == 0.0f);
    living::Place lutie;
    lutie.snow = true;
    CHECK(seasons::of("xmas", 7, true, lutie).snow > 0.5f);  // all year
}

TEST(seasons_chosen_like_the_clock) {
    // 2.6.22, [effects] season (the user: «время года по принципу с часами»): the computer's calendar, or a season
    // of the player's choice that stays - its month in the middle of it
    CHECK_EQ(seasons::monthFor(seasons::kSeasonAuto, 10), 10);
    CHECK_EQ(seasons::monthFor(seasons::kSeasonSpring, 10), 4);
    CHECK_EQ(seasons::monthFor(seasons::kSeasonSummer, 10), 7);
    CHECK_EQ(seasons::monthFor(seasons::kSeasonAutumn, 1), 10);
    CHECK_EQ(seasons::monthFor(seasons::kSeasonWinter, 10), 1);
    CHECK_EQ(seasons::monthFor(42, 10), 10);  // none of them: the calendar's
    // winter's month snows on the fields, autumn's is dry, spring's lush, summer's as the game
    const living::Place field = living::placeOf("prt_fild08", true, false);
    CHECK(seasons::of("prt_fild08", seasons::monthFor(seasons::kSeasonWinter, 10), true, field).snow > 0.5f);
    CHECK(seasons::of("prt_fild08", seasons::monthFor(seasons::kSeasonSpring, 10), true, field).lush > 0.5f);
    const seasons::Look summer = seasons::of("prt_fild08", seasons::monthFor(seasons::kSeasonSummer, 10), true, field);
    CHECK(summer.snow == 0.0f && summer.dry == 0.0f && summer.lush == 0.0f);
}

TEST(seasons_change_the_creatures_place_in_autumn_alone) {
    // 2.7.5: another season chosen made the map's creatures anew every time (the butterflies came again); the place
    // differs only by autumn's leaves - the same place, the same creatures
    const living::Place summer = living::placeOf("prt_fild08", true, false);
    const living::Place autumn = living::placeOf("prt_fild08", true, true);
    CHECK(summer == living::placeOf("prt_fild08", true, false));
    CHECK(!(summer == autumn));
    CHECK(!(summer == living::placeOf("moc_fild12", true, false)));
}
