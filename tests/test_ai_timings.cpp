#include <cmath>

#include "core/ai/timings.hpp"
#include "test.hpp"

using namespace uagfx::ai;

// 3.0.2-beta.7: a start's steps in their order, a step's parts in its brackets; one decimal under 10 ms
TEST(ai_steps_line_in_order) {
    Steps s;
    s.add("shaders", 9512.4);
    s.addPart("ai_conv", 8650.2);
    s.addPart("ai_mip", 3.24);
    s.add("the pack opened", 312.0);
    CHECK_EQ(s.line(), std::string("shaders 9512 ms (ai_conv 8650, ai_mip 3.2), the pack opened 312 ms"));
    CHECK(std::fabs(s.total() - 9824.4) < 0.01);  // the steps alone: the parts are in theirs
}

TEST(ai_steps_none) { CHECK_EQ(Steps().line(), std::string()); }

// A picture of the pack: its parts' average and most, and the whole of each picture's; 3.0.2-beta.12: how long the
// frame that took it lasted (its wait for the frame is never longer)
TEST(ai_load_times_average_and_most) {
    LoadTimes t;
    t.add({1.0, 2.0, 3.0, 0.5, 10.2}, 16.6);
    t.add({3.0, 0.4, 5.0, 0.1, 22.1}, 33.4);
    CHECK_EQ(t.count(), 2u);
    CHECK_EQ(t.line(), std::string("2: in the queue 2.0 (3.0), read 1.2 (2.0), WebP 4.0 (5.0), fitted 0.3 (0.5), "
                                   "until its frame 16 (22), in all 24 (31) - its frame lasted 25 (33)"));
}

// 3.0.2-beta.12 (the client build session: icons waited 67 ms for their frame, decoded in 3.6): an icon that came in
// a long frame of the game's waits for that frame's end - the line shows the frame's length beside the wait
TEST(ai_load_times_a_long_frame_shown) {
    LoadTimes t;
    t.add({0.1, 0.2, 3.6, 0.1, 66.0}, 70.0);
    CHECK_EQ(t.line(), std::string("1: in the queue 0.1 (0.1), read 0.2 (0.2), WebP 3.6 (3.6), fitted 0.1 (0.1), "
                                   "until its frame 66 (66), in all 70 (70) - its frame lasted 70 (70)"));
}

TEST(ai_load_times_none) {
    CHECK_EQ(LoadTimes().count(), 0u);
    CHECK_EQ(LoadTimes().line(), std::string());
}
