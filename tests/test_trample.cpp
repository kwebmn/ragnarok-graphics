#include <algorithm>
#include <cmath>

#include "core/trample.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

footsteps::Step stepAt(float x, float z, float born) {
    footsteps::Step s;
    s.x = x;
    s.z = z;
    s.born = born;
    return s;
}

bool near(float a, float b, float within) { return std::fabs(a - b) <= within; }

}  // namespace

TEST(trample_holds_as_grass_ps_summed_the_steps) {
    // 2.6.8: grass_ps's own sum until now - within sqrt(12) of a step the blades lean away from it, the newer the
    // harder; within 3 they are trodden down. At 2 units from a new step: away by 1 - 4/12, 4/9 of the height left.
    const std::deque<footsteps::Step> one{stepAt(0.0f, 0.0f, 10.0f)};
    trample::Hold h = trample::at(one, 10.0f, 2.0f, 2.0f, 0.0f);
    CHECK(near(h.pushX, 2.0f / 3.0f, 1e-4f) && near(h.pushZ, 0.0f, 1e-6f));
    CHECK(near(h.left, 4.0f / 9.0f, 1e-4f));
    h = trample::at(one, 10.0f, 2.0f, 0.0f, 0.0f);  // on the step: trodden flat, no way to lean
    CHECK(near(h.left, 0.0f, 1e-6f) && h.pushX == 0.0f && h.pushZ == 0.0f);
    h = trample::at(one, 10.0f, 2.0f, 0.0f, -4.0f);  // past its reach: nothing
    CHECK(h.pushX == 0.0f && h.pushZ == 0.0f && h.left == 1.0f);
    h = trample::at(one, 11.0f, 2.0f, 2.0f, 0.0f);  // half its life gone: half the push, half the treading
    CHECK(near(h.pushX, 1.0f / 3.0f, 1e-4f) && near(h.left, 1.0f - 0.5f * (5.0f / 9.0f), 1e-4f));
    CHECK(trample::at(one, 12.0f, 2.0f, 2.0f, 0.0f).left == 1.0f);  // its life over
    CHECK(trample::at(one, 10.0f, 0.0f, 2.0f, 0.0f).left == 1.0f);  // no life: no step holds
}

TEST(trample_grid_reads_back_what_the_steps_hold) {
    // The grid over a walker's steps, read between its texel centres as the shader reads it, says what the sum says
    const std::deque<footsteps::Step> steps{stepAt(10.0f, 20.0f, 4.0f), stepAt(14.5f, 21.0f, 4.3f),
                                            stepAt(19.0f, 20.0f, 4.6f), stepAt(23.5f, 21.0f, 4.9f)};
    const float now = 5.0f, life = 2.0f;
    trample::Grid g;
    trample::build(steps, now, life, g);
    CHECK_EQ(g.texels.size(), size_t(trample::Grid::kSide * trample::Grid::kSide * 4));
    CHECK(g.cell >= 0.5f && g.cell <= 0.51f);  // a small walk: the finest texels
    float worstPush = 0.0f, worstLeft = 0.0f;
    for (float z = 15.0f; z <= 26.0f; z += 0.37f)
        for (float x = 5.0f; x <= 29.0f; x += 0.41f) {
            const trample::Hold want = trample::at(steps, now, life, x, z), got = trample::sample(g, x, z);
            worstLeft = std::max(worstLeft, std::fabs(got.left - want.left));
            bool onStep = false;  // the push turns round over a step's middle: the blades there lie trodden down
            for (const footsteps::Step& s : steps) onStep |= std::hypot(x - s.x, z - s.z) < 1.0f;
            if (!onStep)
                worstPush = std::max({worstPush, std::fabs(got.pushX - want.pushX), std::fabs(got.pushZ - want.pushZ)});
        }
    CHECK(worstPush < 0.12f);  // the blades lean by about as much
    CHECK(worstLeft < 0.12f);  // and are trodden down about as far
    // at a texel's centre only the bytes' steps
    const float cx = g.originX + 40.5f * g.cell, cz = g.originZ + 61.5f * g.cell;
    const trample::Hold want = trample::at(steps, now, life, cx, cz), got = trample::sample(g, cx, cz);
    CHECK(near(got.pushX, std::clamp(want.pushX, -2.0f, 2.0f), 2.0f / 127.0f));
    CHECK(near(got.left, want.left, 1.0f / 127.0f));
}

TEST(trample_grid_edges_hold_nothing) {
    // Read past the grid, the shader takes its edge: the edge's texels hold nothing, so nothing leans out there
    const std::deque<footsteps::Step> steps{stepAt(-50.0f, 7.0f, 0.0f), stepAt(-40.0f, 9.0f, 0.0f)};
    trample::Grid g;
    trample::build(steps, 0.5f, 2.0f, g);
    const int side = trample::Grid::kSide;
    CHECK_EQ(g.texels.size(), size_t(side * side * 4));
    if (g.texels.size() != size_t(side * side * 4)) return;
    for (int i = 0; i < side; ++i)
        for (const int at : {i, (side - 1) * side + i, i * side, i * side + side - 1}) {
            CHECK_EQ(int(g.texels[size_t(at) * 4]), 0);
            CHECK_EQ(int(g.texels[size_t(at) * 4 + 1]), 0);
            CHECK_EQ(int(g.texels[size_t(at) * 4 + 2]), 127);
        }
    const trample::Hold far = trample::sample(g, 400.0f, -300.0f);
    CHECK(far.pushX == 0.0f && far.pushZ == 0.0f && far.left == 1.0f);
}

TEST(trample_grid_spreads_over_steps_far_apart) {
    // Two walkers 150 units apart: the texels grow so the grid takes both, each still leaning its blades
    const std::deque<footsteps::Step> steps{stepAt(0.0f, 0.0f, 1.0f), stepAt(150.0f, 30.0f, 1.0f)};
    trample::Grid g;
    trample::build(steps, 1.0f, 2.0f, g);
    CHECK(g.cell > 1.0f && g.cell < 1.4f);
    CHECK(trample::sample(g, 2.0f, 0.0f).pushX > 0.3f);
    CHECK(trample::sample(g, 148.0f, 30.0f).pushX < -0.3f);
    CHECK(trample::sample(g, 0.0f, 0.0f).left < 0.5f);
    CHECK(trample::sample(g, 150.0f, 30.0f).left < 0.5f);
}

TEST(trample_no_grid_without_a_live_step) {
    trample::Grid g;
    trample::build({}, 1.0f, 2.0f, g);
    CHECK(g.texels.empty());
    trample::build({stepAt(0.0f, 0.0f, 0.0f)}, 3.0f, 2.0f, g);  // its life over
    CHECK(g.texels.empty());
    trample::build({stepAt(0.0f, 0.0f, 0.0f)}, 0.5f, 0.0f, g);  // the steps kept for nothing that lasts
    CHECK(g.texels.empty());
    CHECK(trample::sample(g, 0.0f, 0.0f).left == 1.0f);
}

TEST(trample_grass_roots_on_every_second_row_at_twice_the_size) {
    // 2.7.29 (the user: «есть мысль, что шар подтормаживает графику, можешь оптимизировать?» - the grass 3-5 ms of
    // 8-13 at 4K, R-PDY4DY): at twice the game's size and more a game pixel is two rows and more - grass_ps looks for
    // the blades' roots on every second row (as Direct3D 11 always), the same blades for half the work
    CHECK_EQ(trample::rootRows(1.0f), 1);
    CHECK_EQ(trample::rootRows(1.25f), 1);
    CHECK_EQ(trample::rootRows(1.5f), 2);
    CHECK_EQ(trample::rootRows(2.0f), 2);
    CHECK_EQ(trample::rootRows(3.0f), 2);  // grass_ps's rows: one or every second
}

