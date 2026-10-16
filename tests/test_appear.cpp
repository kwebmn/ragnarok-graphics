#include "core/appear.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(appear_a_new_sprite_fades_in_a_known_one_stays) {
    appear::Tracker t;
    CHECK(t.alpha(1, 100, 100, 0) == 1.0f);  // the first frame: as it is
    t.frame(16);
    CHECK(t.alpha(1, 104, 101, 16) == 1.0f);  // the same monster, a step on
    CHECK(t.alpha(1, 600, 300, 16) == 0.0f);  // a new one
    t.frame(16);
    CHECK(t.alpha(1, 108, 102, 141) == 1.0f);
    const float half = t.alpha(1, 600, 300, 141);
    CHECK(half > 0.45f && half < 0.55f);
    t.frame(16);
    CHECK(t.alpha(1, 600, 300, 400) == 1.0f);
}

TEST(appear_kinds_and_places_kept_apart) {
    appear::Tracker t;
    t.alpha(1, 100, 100, 0);
    t.frame(2000);
    t.frame(16);  // nothing in that frame: a sprite gone for a frame comes back new
    CHECK(t.alpha(1, 100, 100, 50) == 0.0f);
    t.frame(16);
    CHECK(t.alpha(4, 100, 100, 60) == 0.0f);  // an item where the monster was: its own
    CHECK(t.alpha(1, 100, 100, 60) > 0.0f);   // the monster goes on fading in
    t.frame(16);
    // Two layers of one character at one place: each its own.
    CHECK(t.alpha(1, 101, 100, 70) > 0.0f);
    CHECK(t.alpha(1, 101, 100, 70) == 0.0f);
}

TEST(appear_after_a_pause_nothing_fades) {
    appear::Tracker t;
    t.frame(5000);  // a map loaded
    CHECK(t.alpha(2, 300, 300, 10000) == 1.0f);
}
