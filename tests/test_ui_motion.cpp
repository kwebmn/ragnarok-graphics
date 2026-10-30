// 3.0.11-beta.46: the new interface's motion (core/ui_motion) - the concept's times, one switch
#include <cmath>

#include "core/ui_motion.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(ui_motion_fades_and_the_flare) {
    motion::setOn(true);
    CHECK(motion::easeOut(0) == 0 && motion::easeOut(1) == 1 && motion::easeOut(0.5f) > 0.5f);
    CHECK(motion::fadeIn(1000, 1000, 120) == 0 && motion::fadeIn(1120, 1000, 120) == 1);
    const float mid = motion::fadeIn(1060, 1000, 120);
    CHECK(mid > 0.5f && mid < 1);
    CHECK(motion::fadeIn(5000, 0, 120) == 1);  // nothing appearing
    CHECK(motion::fadeOut(1000, 1000, 240) == 1 && motion::fadeOut(1240, 1000, 240) == 0);
    CHECK(motion::fadeOut(1300, 1000, 240) == 0 && motion::fadeOut(1000, 0, 240) == 0);
    CHECK(motion::flare(1000, 1000, 600) == 0 && std::fabs(motion::flare(1120, 1000, 600) - 1) < 1e-4f);
    CHECK(motion::flare(1400, 1000, 600) > 0 && motion::flare(1400, 1000, 600) < 1);
    CHECK(motion::flare(1600, 1000, 600) == 0 && motion::flare(900, 1000, 600) == 0);  // once, then none
    motion::setOn(false);  // [interface] animations = 0: at once
    CHECK(motion::fadeIn(1000, 1000, 120) == 1 && motion::fadeOut(1000, 1000, 240) == 0);
    CHECK(motion::flare(1120, 1000, 600) == 0);
    motion::setOn(true);
}
