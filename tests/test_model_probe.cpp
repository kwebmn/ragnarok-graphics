// 3.0.20-beta.61 (R-HBPQ54 «зникли частини мосту», R-3W4TYM «теж моста немає»): the models' dump reads the depth
// where a face of ours should be - there, something nearer, or what is behind it (the face not drawn)
#include <cmath>

#include "core/game_camera.hpp"
#include "core/model_probe.hpp"
#include "test.hpp"

using namespace uagfx;
using modelprobe::Seen;

namespace {

camera::Projection lens() {
    camera::Projection p;
    p.f0 = 1200.0f, p.f1 = 1200.0f;
    p.cx = 640.0f, p.cy = 480.0f;
    p.nearZ = 10.0f, p.farZ = 1500.0f;
    return p;
}

}  // namespace

TEST(model_probe_a_faces_middle_and_its_depth_there) {
    const camera::Projection p = lens();
    // a deck's triangle 300 deep, facing the eye squarely: its middle at (10, -5, 300)
    const float a[3] = {-20.0f, -35.0f, 300.0f}, b[3] = {40.0f, -35.0f, 300.0f}, c[3] = {10.0f, 55.0f, 300.0f};
    const float none[2] = {0.0f, 0.0f};
    modelprobe::Point at;
    CHECK(modelprobe::pointOf(a, b, c, p, 1.0f, none, 1280, 960, at));
    CHECK_EQ(at.x, int(std::floor(640.0f + 1200.0f * 10.0f / 300.0f + 0.5f)));
    CHECK_EQ(at.y, int(std::floor(480.0f + 1200.0f * -5.0f / 300.0f + 0.5f)));
    const float depth = modelprobe::depthOf(p, 300.0f);
    CHECK(at.lo <= depth && depth <= at.hi);
    CHECK(at.hi - at.lo < 1e-6f);  // square to the eye: one depth
    CHECK(at.area > 1000.0f);
    CHECK_EQ(int(modelprobe::judge(at, depth)), int(Seen::There));
    // the riverbed 80 behind shows there: the face was not drawn; a wall 50 in front hides it
    CHECK_EQ(int(modelprobe::judge(at, modelprobe::depthOf(p, 380.0f))), int(Seen::Farther));
    CHECK_EQ(int(modelprobe::judge(at, modelprobe::depthOf(p, 250.0f))), int(Seen::Nearer));
    CHECK_EQ(int(modelprobe::judge(at, 1.0f)), int(Seen::Farther));  // nothing drawn there at all
}

TEST(model_probe_a_slanted_face_takes_its_depths_across_the_pixel) {
    const camera::Projection p = lens();
    // a deck seen from above at a slant: deeper up the screen
    const float a[3] = {-40.0f, 60.0f, 260.0f}, b[3] = {40.0f, 60.0f, 260.0f}, c[3] = {0.0f, -40.0f, 340.0f};
    const float none[2] = {0.0f, 0.0f};
    modelprobe::Point at;
    CHECK(modelprobe::pointOf(a, b, c, p, 1.0f, none, 1280, 960, at));
    CHECK(at.hi > at.lo);
    CHECK(at.hi - at.lo < modelprobe::kMostSpan);
    // any sample of that pixel is the face's: the depths at its edges both judged there
    CHECK_EQ(int(modelprobe::judge(at, at.lo)), int(Seen::There));
    CHECK_EQ(int(modelprobe::judge(at, at.hi)), int(Seen::There));
    CHECK_EQ(int(modelprobe::judge(at, at.hi + 10.0f * modelprobe::kSlack)), int(Seen::Farther));
}

TEST(model_probe_the_targets_scale_and_jitter_move_the_pixel) {
    const camera::Projection p = lens();
    const float a[3] = {-20.0f, -35.0f, 300.0f}, b[3] = {40.0f, -35.0f, 300.0f}, c[3] = {10.0f, 55.0f, 300.0f};
    const float jitter[2] = {0.25f, -0.25f};
    modelprobe::Point at;
    // the world drawn at twice the game's size (4K under a 1920 game)
    CHECK(modelprobe::pointOf(a, b, c, p, 2.0f, jitter, 2560, 1920, at));
    CHECK_EQ(at.x, int(std::floor(2.0f * (640.0f + 40.0f) + 0.5f + 0.25f)));
    CHECK_EQ(at.y, int(std::floor(2.0f * (480.0f - 20.0f) + 0.5f - 0.25f)));
}

TEST(model_probe_what_is_not_to_be_judged) {
    const camera::Projection p = lens();
    const float none[2] = {0.0f, 0.0f};
    modelprobe::Point at;
    // a corner behind the near plane
    const float n0[3] = {-20.0f, -35.0f, 300.0f}, n1[3] = {40.0f, -35.0f, 5.0f}, n2[3] = {10.0f, 55.0f, 300.0f};
    CHECK(!modelprobe::pointOf(n0, n1, n2, p, 1.0f, none, 1280, 960, at));
    // off the target
    const float o0[3] = {900.0f, 0.0f, 300.0f}, o1[3] = {960.0f, 0.0f, 300.0f}, o2[3] = {930.0f, 60.0f, 300.0f};
    CHECK(!modelprobe::pointOf(o0, o1, o2, p, 1.0f, none, 1280, 960, at));
    // edge on to the eye: its plane goes through it
    const float e0[3] = {0.0f, 0.0f, 100.0f}, e1[3] = {0.0f, 0.0f, 300.0f}, e2[3] = {30.0f, 0.0f, 200.0f};
    CHECK(!modelprobe::pointOf(e0, e1, e2, p, 1.0f, none, 1280, 960, at));
    // past where the depth is squeezed (the far plane less ten)
    const float f0[3] = {-20.0f, -35.0f, 1495.0f}, f1[3] = {40.0f, -35.0f, 1495.0f}, f2[3] = {10.0f, 55.0f, 1495.0f};
    CHECK(!modelprobe::pointOf(f0, f1, f2, p, 1.0f, none, 1280, 960, at));
    // long and thin: 400 pixels of area, but no pixel surely inside it
    const float t0[3] = {-100.0f, 0.0f, 300.0f}, t1[3] = {100.0f, 0.0f, 300.0f}, t2[3] = {100.0f, 0.25f, 300.0f};
    CHECK(!modelprobe::pointOf(t0, t1, t2, p, 1.0f, none, 1280, 960, at));
    // a sliver: no pixel of its own
    const float s0[3] = {0.0f, 0.0f, 300.0f}, s1[3] = {0.1f, 0.0f, 300.0f}, s2[3] = {0.0f, 0.1f, 300.0f};
    CHECK(!modelprobe::pointOf(s0, s1, s2, p, 1.0f, none, 1280, 960, at));
}
