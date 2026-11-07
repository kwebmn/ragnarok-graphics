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
    fd.covered(modelfade::Box{350, 200, 450, 320, 150});
    fd.frameDone(1);  // the next frame asks about it
    CHECK(fd.alongside(modelfade::Box{300, 300, 500, 450, 120}, 200));   // its front, before the character
    CHECK(!fd.alongside(modelfade::Box{300, 100, 500, 250, 240}, 200));  // its roof behind the character
    CHECK(!fd.alongside(modelfade::Box{600, 100, 700, 250, 120}, 200));  // another house
    std::vector<uint8_t> q(4 * 32, 0);
    const float xy[4][4] = {{10, 20, 0, 0.01f}, {30, 20, 0, 0.01f}, {10, 50, 0, 0.01f}, {30, 50, 0, 0.01f}};
    for (int i = 0; i < 4; ++i) std::memcpy(q.data() + i * 32, xy[i], 16);
    modelfade::Box b;
    CHECK(modelfade::boxOf(q.data(), 32, 4, b) && b.x0 == 10 && b.y1 == 50 && std::abs(b.nearest - 100) < 0.01f);
}
