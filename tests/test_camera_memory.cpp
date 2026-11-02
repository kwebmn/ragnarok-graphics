// 3.0.11-beta.184 (R-ANMTEJ «Зум при вході в гру збивається ... зберігати останнє значення з гри»): the camera's tilt
// and distance kept between the games (core/camera_memory) - the game's two slots, outdoors and buildings
#include <string>

#include "core/camera_memory.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(camera_memory_round_trip) {
    cameramemory::Memory m;
    m.outdoors = {-60.0f, 812.6f};
    m.buildings = {-45.0f, 275.7f};
    const std::string text = cameramemory::format(m);
    CHECK_EQ(text, std::string("outdoors -60.0 812.6\nbuildings -45.0 275.7\n"));
    const auto back = cameramemory::parse(text);
    CHECK(back.has_value());
    if (!back) return;
    CHECK(!cameramemory::differs(*back, m));
}

TEST(camera_memory_refuses_nonsense) {
    CHECK(!cameramemory::parse("").has_value());
    CHECK(!cameramemory::parse("outdoors -60 812\n").has_value());                        // a slot missing
    CHECK(!cameramemory::parse("outdoors -60 abc\nbuildings -45 280\n").has_value());     // not a number
    CHECK(!cameramemory::parse("outdoors -60 812x\nbuildings -45 280\n").has_value());    // not all of it a number
    CHECK(!cameramemory::parse("outdoors 30 812\nbuildings -45 280\n").has_value());      // tilted up past level
    CHECK(!cameramemory::parse("outdoors -60 99999\nbuildings -45 280\n").has_value());   // past any zoom
    CHECK(!cameramemory::parse("outdoors -60 nan\nbuildings -45 280\n").has_value());
    const auto any = cameramemory::parse("\r\nbuildings -45 280\r\noutdoors -60.5 812.25\r\n");  // any order, CRLF
    CHECK(any.has_value());
    if (!any) return;
    CHECK(any->outdoors.tilt == -60.5f && any->outdoors.zoom == 812.25f && any->buildings.zoom == 280.0f);
}

TEST(camera_memory_now_into_its_slot) {
    const cameramemory::Memory slots;  // the game's own at a start: -45 and 300 both
    CHECK(slots.outdoors.tilt == -45.0f && slots.outdoors.zoom == 300.0f && slots.buildings.zoom == 300.0f);
    const cameramemory::Pose now{-70.0f, 1500.0f};
    const cameramemory::Memory out = cameramemory::withNow(slots, now, false);
    CHECK(out.outdoors.zoom == 1500.0f && out.outdoors.tilt == -70.0f && out.buildings.zoom == 300.0f);
    const cameramemory::Memory in = cameramemory::withNow(slots, now, true);
    CHECK(in.buildings.zoom == 1500.0f && in.buildings.tilt == -70.0f && in.outdoors.zoom == 300.0f);
    CHECK(cameramemory::differs(out, slots));
    CHECK(!cameramemory::differs(slots, cameramemory::withNow(slots, {-45.2f, 300.3f}, false)));  // under half a unit
}
