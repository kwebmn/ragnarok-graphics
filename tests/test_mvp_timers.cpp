// 3.0.11-beta.280 (the owner's feature 9): the bosses' respawns by the server's rule
#include "core/mvp_timers.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(mvp_timers_soonest_first_and_kept_across_starts) {
    mvptimers::Timers t;
    CHECK(t.died(1039, "prt_maze03", 1000, 7200000));  // Baphomet, 2 h
    CHECK(t.died(1112, "treasure02", 1100, 3600000));  // Drake, 1 h
    CHECK(!t.died(1039, "prt_maze03", 1002, 7200000));  // told twice
    CHECK(!t.died(1039, "", 1002, 7200000));
    auto s = t.shown(2000);
    CHECK_EQ(s.size(), size_t(2));
    CHECK_EQ(s[0].mob, 1112);
    CHECK_EQ(s[0].back(), 4700LL);
    const mvptimers::Timers back = mvptimers::Timers::parse(t.text() + "junk\n");
    CHECK_EQ(back.shown(2000).size(), size_t(2));
    CHECK_EQ(back.shown(2000)[1].map, std::string("prt_maze03"));
    // past 30 minutes after it came back: gone
    CHECK_EQ(t.shown(4700 + 1801).size(), size_t(1));
    t.prune(8200 + 1801);
    CHECK(t.empty());
}

TEST(mvp_timers_left_words) {
    CHECK_EQ(mvptimers::left(4700, 1000), std::string("1:01:40"));
    CHECK_EQ(mvptimers::left(1754, 1000), std::string("12:34"));
    CHECK_EQ(mvptimers::left(1000, 1190), std::string("+3:10"));
}
