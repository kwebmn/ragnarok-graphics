#include "core/map_load.hpp"
#include "test.hpp"

using namespace uagfx;
using Event = map_load::Watch::Event;

TEST(map_load_the_first_map) {
    // 3.0.2-beta.12: the game's first map - no world before it; its loading picture under the map's name (uaRO.exe
    // copies the name first), then its world
    map_load::Watch w;
    CHECK(w.frame(0, "", false) == Event::None);  // the login, the character list
    CHECK(w.frame(10, "prontera", false) == Event::Began);
    CHECK(w.loading());
    CHECK(w.frame(400, "prontera", false) == Event::None);
    CHECK(w.frame(3700, "prontera", true) == Event::Ended);
    CHECK_EQ(w.map(), std::string("prontera"));
    CHECK(w.loadMs() == 3690.0);
    CHECK(w.beforeMs() < 0);  // no old map
    CHECK(!w.loading());
    CHECK(w.frame(3716, "prontera", true) == Event::None);
}

TEST(map_load_after_an_old_map) {
    // the old map's last frame to the new one's loading picture: the old map let go (and the server's word)
    map_load::Watch w;
    w.frame(0, "prontera", true);
    w.frame(16, "prontera", true);
    CHECK(w.frame(250, "prt_fild08", false) == Event::Began);
    CHECK(w.beforeMs() == 234.0);
    CHECK(w.frame(3940, "prt_fild08", true) == Event::Ended);
    CHECK(w.loadMs() == 3690.0 && w.beforeMs() == 234.0);
    CHECK_EQ(w.map(), std::string("prt_fild08"));
}

TEST(map_load_a_frame_without_the_world_no_load) {
    // the world unseen for 300 ms or less in the same map: no load; the same map loaded anew (a fly wing) is longer
    map_load::Watch w;
    w.frame(0, "prontera", true);
    CHECK(w.frame(16, "prontera", false) == Event::Began);
    CHECK(w.frame(33, "prontera", true) == Event::Dropped);
    CHECK(w.frame(50, "prontera", false) == Event::Began);
    CHECK(w.frame(900, "prontera", true) == Event::Ended);
    // another map, however quick: a load
    CHECK(w.frame(950, "izlude", false) == Event::Began);
    CHECK(w.frame(1100, "izlude", true) == Event::Ended);
}

TEST(map_load_given_up) {
    // back to the character list: dropped; two minutes without a world: dropped, and none begins until a world
    map_load::Watch w;
    w.frame(0, "prontera", true);
    CHECK(w.frame(10, "geffen", false) == Event::Began);
    CHECK(w.frame(20, "", false) == Event::Dropped);
    CHECK(w.frame(30, "", false) == Event::None);
    CHECK(w.frame(40, "geffen", false) == Event::Began);
    CHECK(w.frame(40 + 120001, "geffen", false) == Event::Dropped);
    CHECK(w.frame(40 + 120100, "geffen", false) == Event::None);
    CHECK(w.frame(40 + 120200, "geffen", true) == Event::None);
    CHECK(w.frame(40 + 120300, "payon", false) == Event::Began);
}
