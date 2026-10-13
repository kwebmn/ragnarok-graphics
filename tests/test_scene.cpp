#include "core/d3d7.hpp"
#include "core/scene.hpp"
#include "core/states.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::d3d7;

TEST(scene_interface_starts_at_zenable_off) {
    SceneTracker t;
    t.beginScene();
    CHECK(!t.ui());
    t.renderState(RS_ZWRITEENABLE, 0);  // world lists G and I turn off Z writes only
    CHECK(!t.ui());
    t.renderState(RS_ZENABLE, 0);  // L11851: the interface lists U1 and U2
    CHECK(t.ui());
    t.renderState(RS_ZENABLE, 1);
    CHECK(t.ui());  // until the next frame
    t.beginScene();
    CHECK(!t.ui());
}

TEST(scene_draw_setups_differ_by_the_interface_flag) {
    DrawSetup a;
    DrawSetup b;
    b.ui = true;
    CHECK(!(a == b));
}
