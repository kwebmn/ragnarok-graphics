// 3.0.21-beta.8 (R-SPPSF3 «когда открываю Alt-O начинает сильно гудеть на видеокарте вентилятор, значит там чтото с
// нагрузкой»): what an open window costs, told in the log when it closes - the frames a second and the card's work a
// frame while it was open against the seconds before it opened. The report's own log could not say it: its
// half-minute lines mix the two, and the game ran 6 s before the window opened
#include <string>

#include "core/window_load.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// `seconds` of frames at `fps` from `now` on; the window open or not; the card's work `gpuMs` and the driver's own
// `ownMs` a frame
bool run(windowload::Watch& w, double& now, double seconds, double fps, bool open, double gpuMs,
         double ownMs = 1.0) {
    bool closed = false;
    const int frames = int(seconds * fps + 0.5);
    for (int i = 0; i < frames; ++i) {
        now += 1.0 / fps;
        closed = w.frame(now, open, gpuMs, ownMs) || closed;
    }
    return closed;
}

}  // namespace

TEST(window_load_tells_the_window_against_the_seconds_before) {
    windowload::Watch w;
    double now = 100.0;
    CHECK(!run(w, now, 20.0, 200.0, false, 4.0, 1.25));  // the game alone
    CHECK(!run(w, now, 10.0, 100.0, true, 6.0, 3.75));   // the window open
    CHECK(run(w, now, 0.02, 200.0, false, 4.0, 1.25));   // closed: told once
    const std::string told = w.told();
    CHECK_EQ(told, std::string("open 10.0 s - 100 frames a second, the card 6.0 ms a frame (60 % of its time), the "
                               "driver's own work 3.75 ms a frame; in the 5 s before it 200 frames a second, the card "
                               "4.0 ms a frame (80 %), the driver's own work 1.25 ms a frame"));
    CHECK(!run(w, now, 3.0, 200.0, false, 4.0));  // nothing more to tell
}

TEST(window_load_a_glance_tells_nothing) {
    windowload::Watch w;
    double now = 0.0;
    run(w, now, 10.0, 100.0, false, 5.0);
    run(w, now, 1.5, 100.0, true, 5.0);  // under two seconds: no measure worth the name
    CHECK(!run(w, now, 0.05, 100.0, false, 5.0));
    // and one opened before the game ran five seconds alone has no «before» to compare with
    windowload::Watch early;
    now = 0.0;
    run(early, now, 1.0, 100.0, false, 5.0);
    run(early, now, 8.0, 50.0, true, 7.0);
    CHECK(run(early, now, 0.05, 100.0, false, 5.0));
    CHECK_EQ(early.told(),
             std::string("open 8.0 s - 50 frames a second, the card 7.0 ms a frame (35 % of its time), the driver's "
                         "own work 1.00 ms a frame; opened too soon to compare with the game alone"));
}

TEST(window_load_without_the_cards_time) {
    windowload::Watch w;
    double now = 0.0;
    run(w, now, 6.0, 60.0, false, -1.0);  // Direct3D 11: the card's work not measured
    run(w, now, 4.0, 30.0, true, -1.0);
    CHECK(run(w, now, 0.1, 60.0, false, -1.0));
    CHECK_EQ(w.told(), std::string("open 4.0 s - 30 frames a second, the driver's own work 1.00 ms a frame; in the "
                                   "5 s before it 60 frames a second, the driver's own work 1.00 ms a frame"));
}

// a pause of the game (another window in front, a map's loading) is not a part of either measure
TEST(window_load_skips_the_pauses) {
    windowload::Watch w;
    double now = 0.0;
    run(w, now, 6.0, 100.0, false, 5.0);
    now += 30.0;  // the game stood half a minute
    run(w, now, 5.0, 100.0, false, 5.0);
    run(w, now, 3.0, 100.0, true, 5.0);
    now += 20.0;  // and again with the window open
    run(w, now, 3.0, 100.0, true, 5.0);
    CHECK(run(w, now, 0.05, 100.0, false, 5.0));
    CHECK_EQ(w.told(),
             std::string("open 6.0 s - 100 frames a second, the card 5.0 ms a frame (50 % of its time), the driver's "
                         "own work 1.00 ms a frame; in the 5 s before it 100 frames a second, the card 5.0 ms a frame "
                         "(50 %), the driver's own work 1.00 ms a frame"));
}
