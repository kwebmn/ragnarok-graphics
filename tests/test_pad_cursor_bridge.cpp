#include "pad/core/logic/cursor_bridge.hpp"
#include "test.hpp"

using namespace uapad;

TEST(cursor_bridge_first_step_parks_in_the_corner) {
    CursorBridge b;
    CursorBridge::Step s = b.step(500, 300, 1920, 1080);
    CHECK(s.settling);           // the client's cursor position is unknown: push it into the corner
    CHECK(s.dx <= -1920 && s.dy <= -1080);
    s = b.step(500, 300, 1920, 1080);
    CHECK(!s.settling);
    CHECK_EQ(s.dx, 500L);        // from the corner straight to the target
    CHECK_EQ(s.dy, 300L);
}

TEST(cursor_bridge_follows_the_target_exactly) {
    CursorBridge b;
    b.step(0, 0, 800, 600);
    b.step(100, 100, 800, 600);
    CursorBridge::Step s = b.step(90, 130, 800, 600);
    CHECK_EQ(s.dx, -10L);
    CHECK_EQ(s.dy, 30L);
    s = b.step(90, 130, 800, 600);  // nothing moved: report nothing
    CHECK_EQ(s.dx, 0L);
    CHECK_EQ(s.dy, 0L);
}

TEST(cursor_bridge_clamps_targets_to_the_client_area) {
    CursorBridge b;
    b.step(0, 0, 800, 600);
    b.step(10, 10, 800, 600);
    CursorBridge::Step s = b.step(5000, -40, 800, 600);  // the Windows cursor left the game window
    CHECK_EQ(s.dx, 789L);   // to x = 799
    CHECK_EQ(s.dy, -10L);   // to y = 0
}

TEST(cursor_bridge_resyncs_after_reset_or_resize) {
    CursorBridge b;
    b.step(0, 0, 800, 600);
    b.step(50, 50, 800, 600);
    b.reset();  // e.g. the window lost focus and the user moved the real mouse meanwhile
    CHECK(b.step(60, 60, 800, 600).settling);
    CHECK_EQ(b.step(60, 60, 800, 600).dx, 60L);
    CHECK(b.step(60, 60, 1024, 768).settling);  // the client area changed size
}

TEST(cursor_bridge_ignores_an_empty_client_area) {
    CursorBridge b;
    CursorBridge::Step s = b.step(10, 10, 0, 0);
    CHECK(!s.settling);
    CHECK_EQ(s.dx, 0L);
    CHECK_EQ(s.dy, 0L);
    CHECK(!b.synced());
}

TEST(cursor_bridge_exact_moves_the_clients_cursor_onto_the_target) {
    CursorBridge b;
    CursorBridge::Step s = b.stepExact(500, 300, 1920, 1080, 1920, 1080, 100, 100);
    CHECK(!s.settling);  // the client's cursor is known: no trip to the corner
    CHECK_EQ(s.dx, 400L);
    CHECK_EQ(s.dy, 200L);
    s = b.stepExact(500, 300, 1920, 1080, 1920, 1080, 500, 300);
    CHECK_EQ(s.dx, 0L);
    CHECK_EQ(s.dy, 0L);
}

TEST(cursor_bridge_exact_scales_to_the_game_resolution) {
    CursorBridge b;  // a 1600x900 game stretched over a 1920x1080 window
    CursorBridge::Step s = b.stepExact(960, 540, 1920, 1080, 1600, 900, 0, 0);
    CHECK_EQ(s.dx, 800L);
    CHECK_EQ(s.dy, 450L);
    s = b.stepExact(5000, -10, 1920, 1080, 1600, 900, 800, 450);  // outside the window: the edge
    CHECK_EQ(s.dx, 799L);
    CHECK_EQ(s.dy, -450L);
    s = b.stepExact(1919, 1079, 1920, 1080, 1600, 900, 0, 0);
    CHECK_EQ(s.dx, 1599L);
    CHECK_EQ(s.dy, 899L);
}

TEST(cursor_bridge_estimate_carries_on_after_exact_steps) {
    CursorBridge b;
    b.stepExact(200, 100, 800, 600, 800, 600, 0, 0);
    CursorBridge::Step s = b.step(210, 90, 800, 600);  // one poll without the client's memory
    CHECK(!s.settling);
    CHECK_EQ(s.dx, 10L);
    CHECK_EQ(s.dy, -10L);
}

TEST(cursor_bridge_exact_ignores_empty_sizes) {
    CursorBridge b;
    CursorBridge::Step s = b.stepExact(10, 10, 0, 0, 800, 600, 5, 5);
    CHECK(!s.settling && s.dx == 0 && s.dy == 0);
    s = b.stepExact(10, 10, 800, 600, 0, 600, 5, 5);
    CHECK(!s.settling && s.dx == 0 && s.dy == 0);
}
