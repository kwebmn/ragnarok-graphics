#include "core/haze.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(haze_by_the_kind_of_map) {
    // 0.34: a light haze far off under the open sky; in dungeons, dark far off and a low fog over the ground; in
    // houses and the like nothing.
    const haze::Look out = haze::lookOf("prt_fild08");
    CHECK(out.haze > 0.0f && out.fog == 0.0f);
    CHECK(out.hazeColour[2] > out.hazeColour[0]);  // bluish
    const haze::Look dun = haze::lookOf("mag_dun01");
    CHECK(dun.haze > 0.0f && dun.fog > 0.0f);
    CHECK(dun.hazeColour[0] < 0.2f);  // dark far off
    const haze::Look in = haze::lookOf("prt_in");
    CHECK(in.haze == 0.0f && in.fog == 0.0f);
    CHECK(haze::lookOf("").haze == 0.0f);
    CHECK(haze::lookOf("gld_dun01").fog > 0.0f);
    CHECK(haze::lookOf("1@tower").haze == 0.0f);  // instances: as they are
}
