// 3.0.13-beta.17 (the owner's pick 1 of «50 идей»): a model's draw over the character's body covers it when it is
// nearer the camera along the ground (3.0.13-beta.20, R-UQ7AGY: the fountain behind, nearer along the view only, not);
// the see-through glides down and back; a house's other parts go with it (3.0.13-beta.19)
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

TEST(model_fade_covers_only_what_stands_before_the_character) {
    const Cam cam(300);
    const float at[3] = {0, 0, 0};
    const modelfade::Focus f = modelfade::focusOf(cam.p, cam.view, at);
    CHECK(f.known && f.h > 0);
    const auto front = cam.wall(-20, -120), close = cam.wall(-6, -120), behind = cam.wall(25, -150);
    CHECK(modelfade::covers(front.data(), 32, 4, nullptr, 0, true, f));
    CHECK(modelfade::covers(close.data(), 32, 4, nullptr, 0, true, f));   // .18: right before it
    CHECK(!modelfade::covers(behind.data(), 32, 4, nullptr, 0, true, f));  // .20: tall behind - nearer along the view
    modelfade::Focus none;
    CHECK(!modelfade::covers(front.data(), 32, 4, nullptr, 0, true, none));
}

TEST(model_fade_glides_down_and_back) {
    modelfade::Fader fd;
    modelfade::Model m;
    m.id = 7;
    CHECK(fd.step(m, false, 1) == 1.0f && fd.fading() == 0);
    float a = 1.0f;
    for (uint32_t frame = 1; frame <= 20; ++frame) a = fd.step(m, true, frame);
    CHECK(a == modelfade::kSeen);
    CHECK(fd.step(m, true, 20) == a);  // once a frame
    // .21: held a while after it last covered - the body slipping off its edges does not make it pulse
    for (uint32_t frame = 21; frame <= 20 + modelfade::kHoldFrames; ++frame) CHECK(fd.step(m, false, frame) == a);
    a = fd.step(m, false, 21 + modelfade::kHoldFrames);
    CHECK(a > modelfade::kSeen && a < 1.0f);
    for (uint32_t frame = 22 + modelfade::kHoldFrames; frame <= 60; ++frame) a = fd.step(m, false, frame);
    CHECK(a == 1.0f);
    fd.frameDone(61);
    CHECK(fd.fading() == 0);
}

TEST(model_fade_the_same_model_moving) {
    // .21: the fountain's water moves - still the same model, its see-through kept; another one far off is not it
    modelfade::Fader fd;
    modelfade::Model m;
    m.id = 9;
    float a = 1.0f;
    for (uint32_t frame = 1; frame <= 10; ++frame) {
        m.w[0] = float(frame) * 3.0f;
        a = fd.step(m, true, frame);
    }
    CHECK(a == modelfade::kSeen);  // glided all the way, not started over each frame
    CHECK(fd.fading() == 1);
    modelfade::Model far = m;
    far.w[2] = 200.0f;
    CHECK(fd.step(far, false, 11) == 1.0f);
    modelfade::Model other = m;
    other.id = 10;
    CHECK(fd.step(other, false, 11) == 1.0f);
}

TEST(model_fade_the_whole_house) {
    modelfade::Fader fd;
    modelfade::Model wall;
    wall.w[0] = 100, wall.w[2] = 100;
    fd.covered(wall);
    fd.frameDone(1);  // the next frame asks about it
    modelfade::Model roof = wall, fence = wall;
    roof.w[0] += 30, roof.w[1] -= 40;
    fence.w[0] += 150;  // .23 (R-QH3H48): a long fence over its box on the screen, far along the ground
    CHECK(fd.alongside(roof));
    CHECK(!fd.alongside(fence));
}

TEST(model_fade_by_the_draws_pieces) {
    // .24 (R-2KHTSU): one draw holding the wall before the character and another far off - only the wall fades
    const Cam cam(300);
    const float at[3] = {0, 0, 0};
    const modelfade::Focus f = modelfade::focusOf(cam.p, cam.view, at);
    const auto front = cam.wall(-20, -120);
    std::vector<uint8_t> far(4 * 32, 0);
    const float corners[4][3] = {{200, 0, -20}, {280, 0, -20}, {200, -120, -20}, {280, -120, -20}};
    for (int i = 0; i < 4; ++i) cam.put(corners[i], far.data() + i * 32);
    std::vector<uint8_t> both(front);
    both.insert(both.end(), far.begin(), far.end());
    const uint16_t list[12] = {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7};
    modelfade::Fader fd;
    modelfade::Fader::Split split;
    CHECK(fd.fade(both.data(), 8, list, 12, false, 1, f, 1, split));
    CHECK(split.faded.size() == 6 && split.solid.size() == 6);
    for (uint16_t i : split.faded) CHECK(i < 4);
    CHECK(split.alpha[0] < 1.0f && split.alpha[4] == 1.0f);
    // the far one alone, nothing fading: drawn as the game made it
    modelfade::Fader none;
    CHECK(!none.fade(far.data(), 4, nullptr, 0, true, 1, f, 1, split));
}
