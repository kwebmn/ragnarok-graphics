#include "core/footsteps.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(footsteps_follow_a_walker_left_and_right) {
    // 0.80: a character walking along x leaves a step each stride, aside of its way by turns; one standing, none.
    footsteps::Tracker t;
    float time = 0.0f;
    for (int k = 0; k <= 60; ++k, time += 0.05f) t.update({map::Point{float(k) * 0.5f, 0.0f, 10.0f}}, time, 20.0f);
    CHECK(t.steps().size() >= 5 && t.steps().size() <= 7);
    CHECK(t.steps()[0].z != t.steps()[1].z);                       // left, right
    CHECK(std::abs(t.steps()[0].z - 10.0f) < 1.0f);
    CHECK(std::abs(t.steps()[0].angle) < 0.01f);                  // along +x
    const size_t before = t.steps().size();
    for (int k = 0; k < 20; ++k, time += 0.05f) t.update({map::Point{30.0f, 0.0f, 10.0f}}, time, 20.0f);
    CHECK(t.steps().size() == before);                             // standing
}

TEST(footsteps_fade_and_no_trail_across_a_warp) {
    footsteps::Tracker t;
    t.update({map::Point{0, 0, 0}}, 0.0f, 2.0f);
    t.update({map::Point{5, 0, 0}}, 0.1f, 2.0f);
    CHECK(t.steps().size() == 1);
    t.update({map::Point{5, 0, 0}}, 3.0f, 2.0f);
    CHECK(t.steps().empty());                                      // gone with its life
    t.update({map::Point{7, 0, 0}}, 3.1f, 2.0f);                   // within reach: the same walker...
    t.update({map::Point{7.2f, 0, 0}}, 3.2f, 2.0f);
    CHECK(t.steps().empty());
    footsteps::Tracker u;
    u.update({map::Point{0, 0, 0}}, 0.0f, 20.0f);
    u.update({map::Point{7, 0, 0}}, 0.05f, 20.0f);
    u.update({map::Point{14, 0, 0}}, 0.1f, 20.0f);
    CHECK(u.steps().size() == 2);
}
