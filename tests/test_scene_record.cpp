// 3.0.17-beta.7 (the concept «простой старт»: the graphics check without the game - step 1, measure a scene's
// size): the driver's messages to the renderer recorded from the start to 15 s after the first map
#include <cstring>

#include "core/scene_record.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(scene_record_stops_after_the_first_map) {
    scenerecord::Recorder r;
    CHECK(r.take(1, 100, false, 0));       // the start: recorded
    CHECK(r.take(8, 5000, false, 50));     // a frame before any map
    CHECK(r.take(14, 9, true, 1000));      // the first map: the window opens
    CHECK(r.take(8, 5000, false, 15999));
    CHECK(!r.take(8, 5000, false, 16001));  // 15 s after it: done
    CHECK(r.done());
    CHECK_EQ(r.messages(), 4);
    CHECK_EQ(r.frames(), 2);  // Execute = 8
    CHECK_EQ(r.bytes(), uint64_t(100 + 5000 + 9 + 5000 + 4 * scenerecord::kEntryHead));
}

TEST(scene_record_has_a_ceiling) {
    scenerecord::Recorder r;
    CHECK(r.take(1, size_t(scenerecord::kMostBytes - 100), false, 0));
    CHECK(!r.take(8, 1000, false, 1));  // past the ceiling: stops, nothing half-written
    CHECK(r.done());
}

TEST(scene_record_entry_head) {
    uint8_t head[scenerecord::kEntryHead];
    scenerecord::entryHead(8, 0x01020304, head);
    uint32_t type = 0, size = 0;
    std::memcpy(&type, head, 4);
    std::memcpy(&size, head + 4, 4);
    CHECK_EQ(type, 8u);
    CHECK_EQ(size, 0x01020304u);
    CHECK(std::memcmp(scenerecord::kMagic, "UAROSCN1", 8) == 0);
}
