#include "core/throttle.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(throttle_writes_at_most_twice_a_second_and_the_last_state) {
    Throttle t(500);
    CHECK(t.changed(1000));   // the first change: now
    CHECK(!t.changed(1100));  // the window is being resized: held back
    CHECK(!t.changed(1200));
    CHECK(!t.due(1300));      // not yet
    CHECK(t.due(1500));       // half a second later: the state of that moment
    CHECK(!t.due(1600));      // nothing new
    CHECK(!t.due(5000));
    CHECK(t.changed(5000));   // a change after a quiet while: now
}

TEST(throttle_tells_when_a_change_is_held_back) {
    Throttle t(500);
    CHECK(!t.pending());
    CHECK(t.changed(1000));
    CHECK(!t.pending());
    CHECK(!t.changed(1100));
    CHECK(t.pending());  // the game may close now: close() writes it
    CHECK(t.due(1600));
    CHECK(!t.pending());
}
