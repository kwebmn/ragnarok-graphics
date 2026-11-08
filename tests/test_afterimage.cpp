// 3.0.16-beta.13 (the owner's pick 15 of «20 идей»): the afterimages behind a fast character, the body's tints
#include "core/afterimage.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(afterimage_only_when_faster_than_a_walk) {
    afterimage::Trail walk;
    for (uint64_t t = 0; t <= 1000; t += 16) {  // 30 units a second: a plain walk
        const float at[3] = {float(t) * 0.03f, 0, 0};
        walk.note(at, t);
    }
    CHECK(walk.speed(1000) > 25 && walk.speed(1000) < 35);
    CHECK(walk.ghosts(1000).empty());
    afterimage::Trail run;
    for (uint64_t t = 0; t <= 1000; t += 16) {  // 60 a second
        const float at[3] = {float(t) * 0.06f, 0, 0};
        run.note(at, t);
    }
    const std::vector<afterimage::Ghost> g = run.ghosts(992);
    CHECK(g.size() == 3);
    CHECK(g[0].at[0] < 992 * 0.06f && g[1].at[0] < g[0].at[0]);  // behind, further each
    CHECK(g[0].alpha > g[2].alpha);
    CHECK(run.ghosts(992 + 400).empty());  // stopped: gone after the tail
}

TEST(afterimage_no_trail_across_a_warp) {
    afterimage::Trail t;
    const float a[3] = {0, 0, 0}, b[3] = {500, 0, 0};
    t.note(a, 0);
    t.note(b, 16);
    CHECK(t.ghosts(16).empty());
    CHECK(afterimage::tintOf(true, false).strength > 0 && afterimage::tintOf(false, false).strength == 0);
}
