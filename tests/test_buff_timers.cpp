#include <string>
#include <vector>

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

TEST(buff_timers_a_cast_told_after_its_icon) {
    // 3.0.4-beta.36 (R-C8NYXS «когда каст обновляю отсчет не сбрасывается»): uAthena puts the icon on before it tells
    // the cast - clif_skill_nodamage(src, bl, skill, lv, sc_start(...)) runs sc_start, its argument, first. A recast's
    // icon came while the old time ran («sent again: its time goes on»), its cast after it: never anew; and a first
    // cast's level never counted
    constexpr uint16_t kIncreaseAgi = 29;
    buffs::Timers t;
    t.icon(kBlessingIcon, true, 1000);  // the icon, then its cast at level 7
    t.cast(kBlessing, 7, 1004);
    CHECK_EQ(t.left(kBlessingIcon, 1004), 180000u - 4u);  // the level's, from the icon's moment
    t.icon(kBlessingIcon, true, 61000);  // cast again a minute on: the icon...
    t.cast(kBlessing, 10, 61003);        // ...and its cast
    CHECK_EQ(t.left(kBlessingIcon, 61003), 240000u - 3u);  // anew
    t.icon(kBlessingIcon, true, 121000);  // a map change: the icon again, no cast - its time goes on
    CHECK_EQ(t.left(kBlessingIcon, 121000), 180000u);
    t.cast(kIncreaseAgi, 10, 121500);  // another skill's cast: not this icon's
    CHECK_EQ(t.left(kBlessingIcon, 121500), 179500u);
    t.cast(kBlessing, 10, 200000);  // a cast that put no icon on (a lower level refused): the time goes on
    CHECK_EQ(t.left(kBlessingIcon, 200000), 101000u);
    t.icon(kBlessingIcon, true, 300000);  // another character now: the last one's icons and casts gone
    t.clear();
    t.cast(kBlessing, 10, 300001);
    CHECK_EQ(t.left(kBlessingIcon, 300001), 0u);
}

// 3.0.7-beta.6 (the user's notification 5): a buff about to end told once, by its name from the server's data
TEST(buff_timers_their_names) {
    CHECK_EQ(std::string(buffs::name(kBlessingIcon)), std::string("Blessing"));
    CHECK(std::string(buffs::name(kPotionIcon)).size() > 0);  // an item's: the item's name
    CHECK_EQ(std::string(buffs::name(60000)), std::string(""));
}

TEST(buff_timers_one_about_to_end_told_once) {
    buffs::Timers t;
    buffs::Ending e;
    t.cast(kBlessing, 1, 1000);  // 60 s
    t.icon(kBlessingIcon, true, 1000);
    CHECK(e.due(t, 40000, 10000).empty());  // 21 s left
    const std::vector<uint16_t> now = e.due(t, 51500, 10000);  // 9.5 s left
    CHECK(now.size() == 1 && now[0] == kBlessingIcon);
    CHECK(e.due(t, 52000, 10000).empty());  // told
    t.cast(kBlessing, 1, 55000);  // cast again: a new end, told again in its time
    t.icon(kBlessingIcon, true, 55000);
    CHECK(e.due(t, 60000, 10000).empty());
    CHECK(e.due(t, 106000, 10000).size() == 1);
    CHECK(e.due(t, 200000, 10000).empty());  // ended
}

TEST(buff_timers_a_short_one_not_told) {
    buffs::Timers t;
    buffs::Ending e;
    t.cast(kBlessing, 1, 0);
    t.icon(kBlessingIcon, true, 0);
    CHECK(e.due(t, 55000, 40000).empty());  // 60 s long, asked 40 s before: no more than half of it - not told
    t.icon(5, true, 0);  // Cloaking: 9 s at most - never a whole half minute
    CHECK(e.due(t, 1000, 10000).empty());
}

// 3.0.11-beta.81 (the party's «пора перекастовать»): which skill gives which icon - a buff one's own cast
TEST(buff_timers_skill_gives_icon) {
    CHECK(buffs::gives(kBlessing, kBlessingIcon));
    CHECK(!buffs::gives(kBlessing, kPotionIcon));
    CHECK(!buffs::gives(0, kBlessingIcon));
}
