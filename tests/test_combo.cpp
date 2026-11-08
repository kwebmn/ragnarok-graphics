// 3.0.16-beta.13 (the owner's pick 5 of «20 идей»): the Monk's combo window - 0x01d2 comes before its skill's damage
#include "core/combo.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(combo_window_opens_on_the_skill_after_it) {
    combo::Window w;
    w.skillDone(5, 1000);          // something before
    w.opened(800, 2000);           // 0x01d2 ...
    CHECK_EQ(w.after(2000), 0);    // not known yet
    w.skillDone(263, 2010);        // ... then Triple Attack's damage
    CHECK_EQ(w.after(2100), 263);
    CHECK(w.left(2400) > 0.4f && w.left(2400) < 0.6f);
    CHECK_EQ(w.after(2800), 0);    // over
    CHECK(w.left(2800) == 0.0f);
}

TEST(combo_window_closes_when_chained_and_falls_back_to_the_last_skill) {
    combo::Window w;
    w.opened(1000, 5000);
    w.skillDone(272, 5005);
    CHECK_EQ(w.after(5100), 272);
    w.skillDone(273, 5300);        // Combo Finish used, no further window told
    CHECK_EQ(w.after(5301), 0);
    combo::Window v;               // no own damage after it: the last skill just before
    v.skillDone(46, 9900);
    v.opened(2000, 10000);
    CHECK_EQ(v.after(10100), 0);
    CHECK_EQ(v.after(10400), 46);
    CHECK(combo::nextOf(273).size() == 3);
}
