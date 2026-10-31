// 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO», stage 2): what the player plays with now - the
// controller's buttons shown on the interface after an intentional pad input, the keys' names after the mouse or the
// keyboard; at most one change per 1.5 s (core/input_mode)
#include "core/input_mode.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(input_mode_intentional_pad_input) {
    CHECK(inputmode::intentional(0x1000, 0, 0, 0, 0));        // a press
    CHECK(!inputmode::intentional(0, 0.3f, 0.2f, 0.1f, 0));    // a drifting stick
    CHECK(inputmode::intentional(0, 0.4f, 0.4f, 0, 0));        // the left stick past half its way
    CHECK(inputmode::intentional(0, 0, 0, 0, -0.6f));          // the right one
    CHECK(!inputmode::mouseMoved(5, -6));                      // a nudge
    CHECK(inputmode::mouseMoved(9, 0) && inputmode::mouseMoved(-6, -7));
}

TEST(input_mode_switches_at_most_once_in_a_second_and_a_half) {
    inputmode::Mode m;
    CHECK(!m.padShown(0));
    m.pad(1000);
    CHECK(m.padShown(1000));  // the first change at once
    m.mouse(1500);
    CHECK(m.padShown(1500));  // too soon: the pad's still shown
    CHECK(!m.padShown(2500));  // 1.5 s after the last change: the mouse's
    m.pad(2600);
    m.mouse(2700);
    CHECK(!m.padShown(4100));  // the mouse came last: nothing blinks
    m.pad(4200);
    CHECK(m.padShown(4200));
}
