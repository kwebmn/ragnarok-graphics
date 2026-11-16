// 3.0.20-beta.76: DLSS's start in the renderer's background keeps out of a map's first seconds (core/late_start.hpp).
#include "core/late_start.hpp"
#include "test.hpp"

using namespace uagfx;

// The owner's logs: in 58 games the map stood 4-6 s after the game's start and DLSS came up 2.5-8 s after the map's
// name - fine every time. In the one game where the map's name came 0.4 s before DLSS's start ended (16.11 22:40:54,
// beta.68) the 64-bit renderer went silent at the map's first frame and was gone five seconds later (exit code
// 0xC0000409).
TEST(late_start_keeps_out_of_a_maps_first_seconds) {
    using namespace latestart;
    CHECK(quiet(1000, 0, 1000));                       // no map so far: at once
    CHECK(!quiet(54600, 54190, 54600));                // that game: the map 0.4 s ago
    CHECK(!quiet(54190 + kMapQuietMs - 1, 54190, 54600));
    CHECK(quiet(54190 + kMapQuietMs, 54190, 54600));   // its pictures up, its world made: now
    CHECK(quiet(9800, 4300, 9800));                    // the usual game: the map 5.5 s ago
    // maps one after another (a walk through warps): held back not for ever
    CHECK(!quiet(60000, 59000, 31000));                // 29 s so far
    CHECK(quiet(61000, 60500, 31000));                 // half a minute: it goes
    CHECK(!quiet(54190 + 1400, 54190, 54600));         // the map's models' pictures still going up
    CHECK(kMapQuietMs >= 2500 && kMapQuietMs <= 6000 && kMostHeldMs == 30000);
}
