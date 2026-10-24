#include <string>

#include "core/buff_timers.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.4-beta.6 (the user's idea 4): the player's buffs counted down - times from the server's own data
namespace {
constexpr uint16_t kBlessing = 34, kBlessingIcon = 10, kPotionIcon = 37;
}

TEST(buff_timers_the_table_from_the_server) {
    CHECK_EQ(buffs::duration(kBlessingIcon, kBlessing, 7), 180000u);  // 40 s + 20 s a level
    CHECK_EQ(buffs::duration(kBlessingIcon, 0, 0), 240000u);          // not known: its longest
    CHECK_EQ(buffs::duration(kPotionIcon, 0, 0), 1800000u);           // a potion's half hour
    CHECK_EQ(buffs::duration(60000, 0, 0), 0u);                       // none
}

TEST(buff_timers_by_the_level_cast) {
    buffs::Timers t;
    t.cast(kBlessing, 7, 1000);
    t.icon(kBlessingIcon, true, 1200);
    CHECK_EQ(t.left(kBlessingIcon, 1200), 180000u);
    CHECK_EQ(t.left(kBlessingIcon, 51200), 130000u);
    CHECK_EQ(t.left(kBlessingIcon, 300000), 0u);  // past its end
}

TEST(buff_timers_without_a_cast_the_longest) {
    buffs::Timers t;
    t.icon(kPotionIcon, true, 0);
    CHECK_EQ(t.left(kPotionIcon, 0), 1800000u);
    t.cast(kBlessing, 10, 100000);  // long ago by the time its icon comes
    t.icon(kBlessingIcon, true, 110000);
    CHECK_EQ(t.left(kBlessingIcon, 110000), 240000u);
}

TEST(buff_timers_a_map_change_keeps_the_time) {
    buffs::Timers t;
    t.cast(kBlessing, 5, 0);
    t.icon(kBlessingIcon, true, 0);  // 140 s
    t.icon(kBlessingIcon, true, 30000);  // sent again on the next map
    CHECK_EQ(t.left(kBlessingIcon, 30000), 110000u);
    t.cast(kBlessing, 10, 60000);  // cast again: anew
    t.icon(kBlessingIcon, true, 60100);
    CHECK_EQ(t.left(kBlessingIcon, 60100), 240000u);
    t.icon(kBlessingIcon, false, 70000);  // put out
    CHECK_EQ(t.left(kBlessingIcon, 70000), 0u);
}

TEST(buff_timers_what_an_icon_shows) {
    CHECK_EQ(buffs::label(45000), std::string("45"));
    CHECK_EQ(buffs::label(272000), std::string("4:32"));
    CHECK_EQ(buffs::label(599000), std::string("9:59"));
    CHECK_EQ(buffs::label(1740000), std::string("29m"));
    CHECK_EQ(buffs::label(300), std::string("1"));  // the last second still shows
    CHECK_EQ(buffs::label(0), std::string(""));
}
