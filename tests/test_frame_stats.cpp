// 3.0.24-beta.34 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 1): the game's presents of the last
// minute for the MCP's perf - how many a second, how long (the middle, the slowest 5 % and 1 %, the longest), the
// stalls with their moments
#include <cmath>

#include "core/frame_stats.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(frame_stats_percentiles_and_stalls) {
    framestats::Window w;
    uint64_t us = 1000000;  // the clock in microseconds
    w.present(us);
    for (int i = 1; i <= 600; ++i) {  // 10 s at 60 a second; at 5 s one frame of 120 ms, two of 40 ms
        us += i == 300 ? 120000 : (i == 100 || i == 500) ? 40000 : 16667;
        w.present(us);
    }
    const framestats::Stats s = w.stats(us, 10);
    CHECK(s.frames >= 590 && s.frames <= 600);
    CHECK(std::abs(s.fps - 60.0) < 3.0);
    CHECK(std::abs(s.p50 - 16.667) < 0.1);
    CHECK(std::abs(s.worst - 120.0) < 0.1);
    CHECK_EQ(s.over25, 3);
    CHECK_EQ(s.over50, 1);
    CHECK_EQ(s.over100, 1);
    CHECK(!s.longest.empty() && std::abs(s.longest[0].ms - 120.0) < 0.1);
    CHECK(std::abs(s.longest[0].ago - 5.0) < 0.2);  // seconds before now
    CHECK(s.p99 >= 16.6 && s.p99 <= 120.0);
    // the last second alone (the 40 ms frame at 500 is 1.7 s back): no stall in it
    const framestats::Stats last = w.stats(us, 1);
    CHECK(last.frames >= 55 && last.frames <= 61);
    CHECK_EQ(last.over25, 0);
    // a pause (the game behind, no presents for 3 s) is no stall; the frames after it count again
    us += 3000000;
    w.present(us);
    us += 16667;
    w.present(us);
    const framestats::Stats after = w.stats(us, 1);
    CHECK_EQ(after.over100, 0);
    CHECK(after.worst < 20.0);
    // older than a minute: gone
    us += 61000000;
    w.present(us);
    us += 16667;
    w.present(us);
    CHECK_EQ(w.stats(us, 60).frames, 1);
    // nothing yet
    framestats::Window none;
    CHECK_EQ(none.stats(us, 10).frames, 0);
}
