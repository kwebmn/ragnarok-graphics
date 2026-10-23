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

TEST(map_load_the_same_map_again_quickly) {
    // 3.0.2-beta.14 (beta.13: maps load in 0.25 s, and the same map's next entry was dropped as a glance away from
    // the world): the game's loading pictures make it a load, however quick
    map_load::Watch w;
    w.frame(0, "prt_fild08", true);
    CHECK(w.frame(16, "prt_fild08", false) == Event::Began);  // the map's start: a frame of the game's
    CHECK(w.frame(30, "prt_fild08", false, true) == Event::None);  // its loading pictures
    CHECK(w.frame(250, "prt_fild08", true) == Event::Ended);
    // without the world for a moment, no loading picture, the same map: still none
    CHECK(w.frame(266, "prt_fild08", false) == Event::Began);
    CHECK(w.frame(282, "prt_fild08", true) == Event::Dropped);
}

TEST(map_load_the_maps_start_is_no_world) {
    // 3.0.2-beta.15 (beta.13 and 14: some loads slow again, unmeasured): uaRO.exe's new camera (0x4fa2e0) keeps its
    // matrix's memory as it found it - the old camera's, when it gets the same block - so a loading picture could
    // pass for the world. While the game's map start runs (0x559030, hooked) no frame is the world.
    map_load::Watch w;
    w.frame(0, "prontera", true);
    CHECK(w.frame(16, "prt_fild08", true, false, true) == Event::Began);  // the old camera's matrix: not the world
    CHECK(w.frame(30, "prt_fild08", true, true, true) == Event::None);
    CHECK(w.frame(250, "prt_fild08", true) == Event::Ended);  // the map's start over: the world
    CHECK(w.loadMs() == 234.0);
    // a map's start makes it a load however quick, the same map without a loading picture too
    CHECK(w.frame(300, "prt_fild08", false, false, true) == Event::Began);
    CHECK(w.frame(400, "prt_fild08", true) == Event::Ended);
}

TEST(map_load_why_not_measured) {
    // 3.0.2-beta.15: a dropped map start says why; a moment without the world in the same map says nothing
    map_load::Watch w;
    w.frame(0, "prontera", true);
    CHECK(w.frame(10, "geffen", false, false, true) == Event::Began);
    CHECK(w.frame(20, "", false) == Event::Dropped);
    CHECK_EQ(std::string(w.droppedWhy()), std::string("back to the character list"));
    CHECK(w.frame(30, "geffen", false, false, true) == Event::Began);
    CHECK(w.frame(30 + 120001, "geffen", false) == Event::Dropped);
    CHECK_EQ(std::string(w.droppedWhy()), std::string("no world for two minutes"));
    w.frame(30 + 120100, "geffen", true);
    CHECK(w.frame(30 + 120200, "geffen", false) == Event::Began);
    CHECK(w.frame(30 + 120216, "geffen", true) == Event::Dropped);
    CHECK_EQ(std::string(w.droppedWhy()), std::string());
}
