// 3.0.13-beta.17 (the owner's pick 1 of «50 идей»): a model's draw over the character's body nearer than it covers it;
// the ground at its feet, a wall behind it or one beside it does not; the see-through glides down and back
#include <cstring>
#include <vector>

#include "core/model_fade.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
// a quad of TL1 vertices (32 bytes each) at one depth
std::vector<uint8_t> quad(float x0, float y0, float x1, float y1, float z) {
    const float xy[4][2] = {{x0, y0}, {x1, y0}, {x0, y1}, {x1, y1}};
    std::vector<uint8_t> v(4 * 32, 0);
    for (int i = 0; i < 4; ++i) {
        const float f[4] = {xy[i][0], xy[i][1], 0.5f, 1.0f / z};
        std::memcpy(v.data() + i * 32, f, sizeof f);
    }
    return v;
}
}  // namespace

TEST(model_fade_covers_the_body_only_when_nearer) {
    modelfade::Focus f;
    f.known = true;
    f.x = 400;
    f.y = 300;
    f.z = 200;
    f.h = 60;
    const auto front = quad(350, 200, 450, 320, 150), behind = quad(350, 200, 450, 320, 260);
    const auto beside = quad(500, 200, 600, 320, 150), feet = quad(350, 295, 450, 305, 199);
    CHECK(modelfade::covers(front.data(), 32, 4, nullptr, 0, true, f));
    CHECK(!modelfade::covers(behind.data(), 32, 4, nullptr, 0, true, f));
    CHECK(!modelfade::covers(beside.data(), 32, 4, nullptr, 0, true, f));
    CHECK(!modelfade::covers(feet.data(), 32, 4, nullptr, 0, true, f));
    modelfade::Focus none;
    CHECK(!modelfade::covers(front.data(), 32, 4, nullptr, 0, true, none));
}

TEST(model_fade_glides_down_and_back) {
    modelfade::Fader fd;
    CHECK(fd.step(7, false, 1) == 1.0f && fd.fading() == 0);
    float a = 1.0f;
    for (uint32_t frame = 1; frame <= 20; ++frame) a = fd.step(7, true, frame);
    CHECK(a == modelfade::kSeen);
    CHECK(fd.step(7, true, 20) == a);  // once a frame
    a = fd.step(7, false, 21);
    CHECK(a > modelfade::kSeen && a < 1.0f);
    for (uint32_t frame = 22; frame <= 40; ++frame) a = fd.step(7, false, frame);
    CHECK(a == 1.0f);
    fd.frameDone(41);
    CHECK(fd.fading() == 0);
}
