#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "core/game_camera.hpp"
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

namespace {

torch::Flame flameAt(float x, float alpha) {
    torch::Flame f;
    f.baseX = x;
    f.baseY = 300.0f;
    f.upY = -60.0f;
    f.rhw = 0.01f;
    f.alpha = alpha;
    return f;
}

}  // namespace

TEST(torch_keeper_holds_a_flame_missing_a_moment) {
    // 2.6.17: "every couple of seconds the torch blinks" - a frame of the game's animation without its flame (or one
    // flameOf does not read) left the new flame out for that frame. Kept as it last was a moment, then let go.
    torch::Keeper k;
    std::vector<torch::Flame> flames{flameAt(100.0f, 1.0f)};
    std::vector<uint32_t> seeds{7};
    k.update(flames, seeds, 10.0);
    flames.clear();
    seeds.clear();
    k.update(flames, seeds, 10.05);  // missing this frame
    CHECK_EQ(flames.size(), size_t(1));
    CHECK(flames.size() == 1 && flames[0].baseX == 100.0f && flames[0].alpha > 0.9f && seeds[0] == 7u);
    flames.clear();
    seeds.clear();
    k.update(flames, seeds, 10.05 + torch::Keeper::kKeepSeconds + 0.01);  // gone long enough: gone
    CHECK(flames.empty());
}

TEST(torch_keeper_smooths_the_games_fade_dips_but_not_a_first_fade_in) {
    torch::Keeper k;
    std::vector<torch::Flame> flames{flameAt(100.0f, 0.2f)};
    std::vector<uint32_t> seeds{7};
    k.update(flames, seeds, 1.0);
    CHECK(near(flames[0].alpha, 0.2f));  // first seen: the game's own fade-in, as the player liked it
    for (int i = 1; i <= 10; ++i) {
        flames = {flameAt(100.0f, std::min(1.0f, 0.2f + 0.1f * float(i)))};
        seeds = {7};
        k.update(flames, seeds, 1.0 + 0.05 * i);
    }
    CHECK(near(flames[0].alpha, 1.0f));
    flames = {flameAt(100.0f, 0.1f)};  // a dip of a frame: hardly seen
    seeds = {7};
    k.update(flames, seeds, 1.55);
    CHECK(flames[0].alpha > 0.9f);
}

TEST(torch_keeper_keeps_a_torchs_seed_as_it_is_followed) {
    // 2.6.17: a torch by a seed cell's edge read its place a cell over now and then (its sprite's frames move the
    // flame a little) - a new seed, and the new flame jumped to another shape every couple of seconds. Followed from
    // frame to frame by its place on the screen it keeps the seed it was first given, and is not kept twice.
    torch::Keeper k;
    std::vector<torch::Flame> flames{flameAt(100.0f, 1.0f), flameAt(400.0f, 1.0f)};
    std::vector<uint32_t> seeds{7, 9};
    k.update(flames, seeds, 1.0);
    flames = {flameAt(101.0f, 1.0f), flameAt(402.0f, 1.0f)};
    seeds = {8, 9};  // the first one's place read a cell over
    k.update(flames, seeds, 1.05);
    CHECK_EQ(flames.size(), size_t(2));
    CHECK(seeds.size() == 2 && seeds[0] == 7u && seeds[1] == 9u);
}

TEST(torch_place_seed_holds_whichever_way_the_camera_looks) {
    // 2.6.17: a torch's seed by its place in the world - the keeper's and its flicker's - not by the screen
    const camera::Projection p = camera::projectionFor(1920, 1080);
    const float worldMid[3] = {12.0f, -5.0f, 102.0f};  // within a seed's cell (an edge's place may read either)
    auto flameSeenFrom = [&](float camX) {
        torch::Flame f;
        f.upY = -60.0f;
        f.rhw = 1.0f / (worldMid[2]);
        const float mx = (worldMid[0] - camX) / worldMid[2] * p.f0 + p.cx, my = worldMid[1] / worldMid[2] * p.f1 + p.cy;
        f.baseX = mx;
        f.baseY = my - f.upY * 0.38f;  // its middle 0.38 of the way up
        return f;
    };
    const float camA[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, camB[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 2.0f, 0, 0};
    const uint32_t a = torch::placeSeed(flameSeenFrom(0.0f), p, camA, true);
    const uint32_t b = torch::placeSeed(flameSeenFrom(2.0f), p, camB, true);
    CHECK_EQ(a, b);
    CHECK_EQ(a, torch::seedAt(12.0f, 102.0f));
}
