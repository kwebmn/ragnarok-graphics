// 3.0.13-beta.17 (the owner's pick 1 of «50 идей»): the models before the character see-through; 3.0.13-beta.26 (the
// owner: «делай дыру, мигает»): a hole round it the shaders leave - only in what is nearer the camera along the ground
// (3.0.13-beta.20, R-UQ7AGY: the fountain behind, nearer along the view only, not)
#include <cmath>
#include <cstring>
#include <vector>

#include "core/model_fade.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
// a camera looking down at 45 degrees from the south, `d` away along its view: c_x = w_x, c_y = w_y cos + w_z (-sin)
// ... as the game keeps it (c_j = sum_i w_i * view[3i+j] + t_j); the world's y up is negative
struct Cam {
    float view[12] = {};
    camera::Projection p = camera::projectionFor(800, 600);
    explicit Cam(float d) {
        const float s = std::sqrt(0.5f), c = s;
        view[0] = 1;
        view[4] = c;
        view[5] = s;
        view[7] = -s;
        view[8] = c;
        view[11] = d;
    }
    void put(const float w[3], uint8_t* out) const {  // a TL1 vertex of a world point
        float cv[3];
        for (int j = 0; j < 3; ++j) cv[j] = w[0] * view[j] + w[1] * view[3 + j] + w[2] * view[6 + j] + view[9 + j];
        const float f[4] = {cv[0] / cv[2] * p.f0 + p.cx, cv[1] / cv[2] * p.f1 + p.cy, 0.5f, 1.0f / cv[2]};
        std::memcpy(out, f, sizeof f);
    }
    // a wall across x from -40 to 40 at z (north +), from the ground (y 0) up to `top` (negative), as a strip
    std::vector<uint8_t> wall(float z, float top) const {
        std::vector<uint8_t> v(4 * 32, 0);
        const float corners[4][3] = {{-40, 0, z}, {40, 0, z}, {-40, top, z}, {40, top, z}};
        for (int i = 0; i < 4; ++i) put(corners[i], v.data() + i * 32);
        return v;
    }
};
}  // namespace

TEST(model_fade_hole_only_before_the_character) {
    const Cam cam(300);
    const float at[3] = {0, 0, 0};
    const modelfade::Focus f = modelfade::focusOf(cam.p, cam.view, at);
    CHECK(f.known && f.h > 0);
    float hole[8];
    modelfade::holeOf(f, hole);
    CHECK(hole[3] > 0 && std::abs(hole[5] * hole[5] + hole[6] * hole[6] - 1.0f) < 1e-4f && hole[7] > 0);
    // a world point's game pixel and depth, asked of the hole
    const auto at3 = [&](float x, float y, float z) {
        uint8_t v[32];
        const float w[3] = {x, y, z};
        cam.put(w, v);
        float q[4];
        std::memcpy(q, v, sizeof q);
        return modelfade::holeAt(hole, q[0], q[1], 1.0f / q[3], 800, 600);
    };
    CHECK(at3(0, -20, -20) > 0.5f);   // a wall before the body, at its middle
    CHECK(at3(0, -20, -6) > 0.0f);    // .18: right before it
    CHECK(at3(0, -60, 25) == 0.0f);   // .20: tall behind - nearer along the view, not along the ground
    CHECK(at3(0, 0, 0) == 0.0f);      // the feet themselves
    CHECK(at3(200, -20, -20) == 0.0f);  // far to the side: outside the hole
    // smooth: a little to the side, fewer of the pixels left out than in the middle, none at the edge
    const float mid = at3(0, -20, -20), side = at3(14, -20, -20);
    CHECK(side < mid && side > 0.0f);
    modelfade::Focus none;
    modelfade::holeOf(none, hole);
    CHECK(hole[3] == 0.0f && modelfade::holeAt(hole, 400, 300, 10, 800, 600) == 0.0f);
}
