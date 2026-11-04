// 3.0.11-beta.213 (the owner: «сделай так чтобы вывод команды @mobsearch показывался на миникарте, ну и чтобы это
// можно было потом убрать с миникарты»): uAthena's @mobsearch answers in the chat - «Mob Search... NAME MAP» and a line
// «%2d[%3d:%3d] NAME» a monster (atcommand.c atmobsearch_sub) - read into the cells the minimap marks
#include <string>

#include "core/mob_search.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(mob_search_header) {
    auto h = mobsearch::header("Mob Search... Poring prt_fild08");
    CHECK(h && h->first == "Poring" && h->second == "prt_fild08");
    h = mobsearch::header("Mob Search... Golden Thief Bug prt_sewb4");
    CHECK(h && h->first == "Golden Thief Bug" && h->second == "prt_sewb4");
    CHECK(!mobsearch::header("Mob Search..."));
    CHECK(!mobsearch::header("Poring: hello"));
}

TEST(mob_search_points) {
    auto p = mobsearch::point(" 1[123: 45] Poring");
    CHECK(p && p->x == 123 && p->y == 45 && p->name == "Poring");
    p = mobsearch::point("12[  7:200] Golden Thief Bug");
    CHECK(p && p->x == 7 && p->y == 200 && p->name == "Golden Thief Bug");
    CHECK(!mobsearch::point("Player : 1[2:3] hi"));
    CHECK(!mobsearch::point("[12:34] no number"));
    CHECK(!mobsearch::point(" 1[12 34] Poring"));
}

// The header starts a search (the one before gone), its lines follow it while they come close after each other; any
// other line ends it; clear() forgets it
TEST(mob_search_reader) {
    mobsearch::Reader r;
    CHECK(!r.active());
    CHECK(r.line("Mob Search... Poring prt_fild08", 1000));
    CHECK(r.active() && r.result().what == "Poring" && r.result().map == "prt_fild08" && r.result().points.empty());
    CHECK(r.line(" 1[123: 45] Poring", 1050));
    CHECK(r.line(" 2[ 10: 20] Poring", 1100));
    CHECK_EQ(int(r.result().points.size()), 2);
    CHECK(!r.line("Bob : hello", 1200));  // the search's lines are over
    CHECK(!r.line(" 3[ 11: 21] Poring", 1300));  // a like line after another one is not the search's
    CHECK_EQ(int(r.result().points.size()), 2);
    CHECK(r.line("Mob Search... Lunatic prt_fild08", 2000));
    CHECK(r.result().what == "Lunatic" && r.result().points.empty());
    CHECK(r.line(" 1[ 50: 60] Lunatic", 2100));
    CHECK(!r.line(" 2[ 51: 61] Lunatic", 9100));  // too long after: not the search's
    r.clear();
    CHECK(!r.active() && r.result().points.empty());
}

// The map's name as the game keeps it (with its .rsw or .gat, any case) is the search's map
TEST(mob_search_same_map) {
    CHECK(mobsearch::sameMap("prt_fild08", "prt_fild08"));
    CHECK(mobsearch::sameMap("prt_fild08", "PRT_FILD08.rsw"));
    CHECK(mobsearch::sameMap("prt_fild08.gat", "prt_fild08"));
    CHECK(!mobsearch::sameMap("prt_fild08", "prt_fild09"));
    CHECK(!mobsearch::sameMap("", "prt_fild08"));
}
