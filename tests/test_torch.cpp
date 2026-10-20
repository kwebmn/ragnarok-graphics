#include <cmath>
#include <cstring>
#include <vector>

#include "core/torch.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }
}  // namespace

TEST(torch_flame_of_an_upright_quad) {
    // A flame frame in the right half of a 256 x 256 texture, drawn twice its size with its top left at (100, 50):
    // its base is texel (65, 103) of the frame, its tip 58 texels higher, its half width 14 texels.
    const torch::Corner q[4] = {{100, 50, 0.5f, 0.01f, 0.5f, 0.0f},
                                {356, 50, 0.5f, 0.01f, 1.0f, 0.0f},
                                {100, 306, 0.5f, 0.01f, 0.5f, 0.5f},
                                {356, 306, 0.5f, 0.01f, 1.0f, 0.5f}};
    torch::Flame f;
    CHECK(torch::flameOf(q, f));
    CHECK(near(f.baseX, 230.0f));
    CHECK(near(f.baseY, 256.0f));
    CHECK(near(f.upX, 0.0f));
    CHECK(near(f.upY, -116.0f));
    CHECK(near(f.halfWidth, 28.0f));
    CHECK(near(f.z, 0.5f));
    CHECK(near(f.rhw, 0.01f));
}

TEST(torch_flame_of_a_turned_quad) {
    // The same frame turned a quarter (the frame's up pointing to the screen's right) at its own size.
    const torch::Corner q[4] = {{228, 0, 0.3f, 0.02f, 0.0f, 0.0f},   // texel (0, 0)
                                {228, 128, 0.3f, 0.02f, 0.5f, 0.0f},  // texel (128, 0)
                                {100, 0, 0.3f, 0.02f, 0.0f, 0.5f},    // texel (0, 128)
                                {100, 128, 0.3f, 0.02f, 0.5f, 0.5f}};
    torch::Flame f;
    CHECK(torch::flameOf(q, f));
    CHECK(near(f.baseX, 228.0f - 103.0f));
    CHECK(near(f.baseY, 65.0f));
    CHECK(near(f.upX, 58.0f));
    CHECK(near(f.upY, 0.0f));
    CHECK(near(f.halfWidth, 14.0f));
}

TEST(torch_flame_needs_a_quad_that_maps_the_texture) {
    const torch::Corner flat[4] = {{0, 0, 0.5f, 0.1f, 0.2f, 0.2f}, {10, 0, 0.5f, 0.1f, 0.2f, 0.2f},
                                   {0, 10, 0.5f, 0.1f, 0.2f, 0.2f}, {10, 10, 0.5f, 0.1f, 0.2f, 0.2f}};
    torch::Flame f;
    CHECK(!torch::flameOf(flat, f));
    const torch::Corner line[4] = {{0, 0, 0.5f, 0.1f, 0.0f, 0.0f}, {10, 0, 0.5f, 0.1f, 0.5f, 0.0f},
                                   {20, 0, 0.5f, 0.1f, 1.0f, 0.0f}, {30, 0, 0.5f, 0.1f, 0.0f, 0.0f}};
    CHECK(!torch::flameOf(line, f));  // no height in the texture
}

TEST(torch_flame_of_a_halved_frame) {
    // The game's sprite setting "half": the frame is 64 x 64 texels in a 64 x 64 texture, drawn at the same size on
    // the screen - the flame lands where it does at full size.
    const torch::Corner q[4] = {{100, 50, 0.5f, 0.01f, 0.0f, 0.0f},
                                {356, 50, 0.5f, 0.01f, 1.0f, 0.0f},
                                {100, 306, 0.5f, 0.01f, 0.0f, 1.0f},
                                {356, 306, 0.5f, 0.01f, 1.0f, 1.0f}};
    torch::Flame f;
    CHECK(torch::flameOf(q, f));
    CHECK(near(f.baseX, 230.0f));
    CHECK(near(f.baseY, 256.0f));
    CHECK(near(f.upY, -116.0f));
    CHECK(near(f.halfWidth, 28.0f));
}

TEST(torch_flicker_stays_within_its_amount) {
    float lo = 10.0f, hi = -10.0f;
    bool differs = false;
    for (int i = 0; i < 2000; ++i) {
        const double t = i * 0.01;
        const float f = torch::flicker(t, 7, 0.3f);
        lo = std::fmin(lo, f);
        hi = std::fmax(hi, f);
        differs = differs || std::fabs(f - torch::flicker(t, 8, 0.3f)) > 0.01f;
        CHECK(torch::flicker(t, 7, 0.0f) == 1.0f);
    }
    CHECK(lo >= 0.7f - 1e-5f);
    CHECK(hi <= 1.3f + 1e-5f);
    CHECK(hi - lo > 0.25f);  // it does flicker
    CHECK(differs);          // each torch its own way
    // Smooth: a hundredth of a second moves it a little.
    CHECK(std::fabs(torch::flicker(3.0, 7, 0.3f) - torch::flicker(3.01, 7, 0.3f)) < 0.12f);
}

TEST(torch_seed_holds_still_at_its_place) {
    CHECK(torch::seedAt(12.3f, 45.6f) == torch::seedAt(12.4f, 45.5f));  // the camera moved: the same torch
    CHECK(torch::seedAt(12.3f, 45.6f) != torch::seedAt(32.0f, 45.6f));
    CHECK(torch::seedAt(12.3f, 45.6f) != torch::seedAt(12.3f, 65.6f));
}

TEST(torch_flame_of_the_games_vertices) {
    // 2.6.12: the four vertices as the game writes them (x, y, z, rhw, the diffuse colour, the specular one, u, v,
    // stride bytes apart): the flame flameOf finds in their corners, its fade the diffuse alpha (the .act's 240 whole)
    const float corner[4][6] = {{100, 50, 0.5f, 0.01f, 0.5f, 0.0f},
                                {356, 50, 0.5f, 0.01f, 1.0f, 0.0f},
                                {100, 306, 0.5f, 0.01f, 0.5f, 0.5f},
                                {356, 306, 0.5f, 0.01f, 1.0f, 0.5f}};
    for (const size_t stride : {size_t(32), size_t(40)}) {
        std::vector<uint8_t> v(4 * stride, 0);
        for (int i = 0; i < 4; ++i) {
            uint8_t* p = v.data() + size_t(i) * stride;
            std::memcpy(p, corner[i], 4 * sizeof(float));
            const uint32_t diffuse = 0x78FFFFFFu;  // alpha 120: half shown
            std::memcpy(p + 16, &diffuse, sizeof diffuse);
            std::memcpy(p + 24, corner[i] + 4, 2 * sizeof(float));
        }
        torch::Flame f;
        CHECK(torch::flameOfQuad(v.data(), stride, f));
        CHECK(near(f.baseX, 230.0f) && near(f.baseY, 256.0f) && near(f.upY, -116.0f) && near(f.rhw, 0.01f));
        CHECK(near(f.alpha, 0.5f));
        const float none = 0.0f;  // no depth: no flame
        for (int i = 0; i < 4; ++i) std::memcpy(v.data() + size_t(i) * stride + 12, &none, sizeof none);
        CHECK(!torch::flameOfQuad(v.data(), stride, f));
    }
}

TEST(torch_flicker_amount_by_the_slider) {
    // 2.6.16: R-N3ZZ3F - "the flicker seems to change nothing": 0-100 % moved the light on the walls from steady to
    // only 12 %. Now 50 % is the calm flicker the torches had, 0 steady, 100 % four times its swing
    CHECK(torch::flickerAmount(0) == 0.0f);
    CHECK(near(torch::flickerAmount(50), 0.175f, 1e-6f));
    CHECK(near(torch::flickerAmount(100), 0.7f, 1e-6f));
    CHECK(torch::flickerAmount(25) < 0.5f * torch::flickerAmount(50));
    CHECK(torch::flickerAmount(150) == torch::flickerAmount(100) && torch::flickerAmount(-5) == 0.0f);
}
