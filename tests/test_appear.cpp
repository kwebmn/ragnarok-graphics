#include "core/appear.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(appear_a_new_sprite_fades_in_a_known_one_stays) {
    appear::Tracker t;
    CHECK(t.alpha(1, 100, 100, 60, 0) == 1.0f);  // the first frame: as it is
    t.frame(16);
    CHECK(t.alpha(1, 104, 101, 60, 16) == 1.0f);  // the same monster, a step on
    CHECK(t.alpha(1, 600, 300, 60, 16) == 0.0f);  // a new one
    t.frame(16);
    CHECK(t.alpha(1, 108, 102, 60, 141) == 1.0f);
    const float half = t.alpha(1, 600, 300, 60, 141);
    CHECK(half > 0.45f && half < 0.55f);
    t.frame(16);
    CHECK(t.alpha(1, 600, 300, 60, 400) == 1.0f);
}

TEST(appear_no_blink_for_a_frame_missed_or_a_new_layer) {
    // 1.0.2: a monster shown, then a frame without it (or a frame shown twice), then it again - still shown
    appear::Tracker t;
    t.alpha(1, 100, 100, 60, 0);
    t.frame(16);
    t.alpha(1, 100, 100, 60, 300);  // fully shown by now
    t.frame(16);
    t.frame(16);  // drawn in no sprites
    CHECK(t.alpha(1, 102, 100, 60, 340) == 1.0f);
    t.frame(16);
    // A layer more next to it (a weapon in an attack): shown at once, the monster too.
    CHECK(t.alpha(1, 104, 100, 60, 360) == 1.0f);
    CHECK(t.alpha(1, 110, 90, 60, 360) == 1.0f);
    t.frame(16);
    // Long gone, back: new.
    CHECK(t.alpha(1, 100, 100, 60, 2000) == 0.0f);
}

TEST(appear_kinds_kept_apart_and_after_a_pause_nothing_fades) {
    appear::Tracker t;
    t.alpha(1, 100, 100, 60, 0);
    t.frame(16);
    CHECK(t.alpha(4, 100, 100, 60, 300) == 0.0f);  // an item where a monster is: its own
    t.frame(5000);  // a map loaded
    CHECK(t.alpha(2, 300, 300, 60, 10000) == 1.0f);
}

TEST(appear_no_blink_when_an_attack_frame_jumps) {
    // 2.4.1: a quarter of the monsters' animations move their frame's middle 45 sprite pixels or more from one frame to
    // the next (Lunatic 60, Baphomet 51, Marina 86) - over 96 pixels of the screen with the camera near: the monster
    // was taken for a new one and faded in again, a blink. Near is now up to 1.5 of the frame's size.
    appear::Tracker t;
    t.alpha(1, 100, 100, 80, 0);
    t.frame(16);
    t.alpha(1, 100, 100, 80, 300);  // fully shown
    t.frame(16);
    CHECK(t.alpha(1, 210, 100, 80, 316) == 1.0f);  // the lunge: 110 pixels on, its frame 80 across
    t.frame(16);
    CHECK(t.alpha(1, 100, 100, 80, 332) == 1.0f);  // and back
    t.frame(16);
    CHECK(t.alpha(1, 400, 100, 80, 348) == 0.0f);  // 300 pixels off: another one, new
}
