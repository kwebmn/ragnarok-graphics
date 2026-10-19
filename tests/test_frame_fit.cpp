#include "core/frame_fit.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(frame_fit_keeps_generated_frames_under_the_monitor) {
    // Reflex's margin under the refresh: 144 Hz -> 138.24 shown at most
    CHECK_EQ(frame_fit::gameFps(144, 4), 34);  // 136 shown, not R-Q7RBYV's 156 on a 144 Hz monitor
    CHECK_EQ(frame_fit::gameFps(144, 3), 46);
    CHECK_EQ(frame_fit::gameFps(144, 2), 69);
    CHECK_EQ(frame_fit::gameFps(240, 4), 56);
    CHECK_EQ(frame_fit::gameFps(60, 2), 29);
}

TEST(frame_fit_leaves_alone_what_it_does_not_know) {
    CHECK_EQ(frame_fit::gameFps(144, 1), 0);  // no generated frames
    CHECK_EQ(frame_fit::gameFps(0, 4), 0);    // the refresh unknown
    CHECK_EQ(frame_fit::gameFps(1, 4), 0);    // Windows' "the display's default"
}

TEST(frame_fit_takes_the_lower_of_the_players_limit) {
    CHECK_EQ(frame_fit::pacedFps(0, 144, 4), 34);
    CHECK_EQ(frame_fit::pacedFps(100, 144, 4), 34);
    CHECK_EQ(frame_fit::pacedFps(30, 144, 2), 30);  // fps_limit lower: as the player has it
    CHECK_EQ(frame_fit::pacedFps(60, 144, 1), 60);  // no frame generation: fps_limit alone
    CHECK_EQ(frame_fit::pacedFps(0, 0, 4), 0);
}
