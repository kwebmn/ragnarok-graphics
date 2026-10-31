// 3.0.11-beta.110 (the concept «Концепция окон и инвентаря», stage 2): the windows' drags as one - a press on a thing
// becomes the window's own drag once the cursor moved past 10 pixels times the scale (till let go); let go, a drag
// gives its thing (the game's drag begun and let go there: «Речі», «Навички», «Спорядження»), a click nothing; what is
// dragged told to the windows that take it (core/window_drag)
#include "core/window_drag.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(window_drag_a_press_becomes_a_drag_past_ten_pixels) {
    windrag::Press p;
    CHECK(windrag::released(p) < 0);  // nothing pressed
    windrag::pressed(p, 3, 100, 100);
    CHECK(!windrag::moved(p, 108, 100, 1.0f));  // a hand's shake
    CHECK(windrag::moved(p, 111, 100, 1.0f));
    CHECK(windrag::moved(p, 100, 100, 1.0f));  // dragging till let go
    CHECK_EQ(windrag::released(p), 3);
    CHECK(windrag::released(p) < 0 && p.index < 0);
    windrag::pressed(p, 5, 100, 100);
    CHECK(!windrag::moved(p, 115, 100, 2.0f));  // 20 pixels at the scale 2
    CHECK(windrag::released(p) < 0);            // a click
}

TEST(window_drag_tells_what_is_dragged) {
    CHECK(windrag::now().kind == windrag::Kind::None);
    windrag::set(windrag::Thing{windrag::Kind::Item, 1750, 0x8000, 10});
    CHECK(windrag::now().kind == windrag::Kind::Item && windrag::now().id == 1750 && windrag::now().location == 0x8000);
    windrag::clear();
    CHECK(windrag::now().kind == windrag::Kind::None);
}
