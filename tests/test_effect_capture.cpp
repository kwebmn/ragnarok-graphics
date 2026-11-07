// 3.0.15-beta.1: the skills' effects recorded before any redraw - a few frames of each one's first life
#include <string>
#include <vector>

#include "core/effect_capture.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(effect_capture_keeps_the_first_life_of_each_effect) {
    effect_capture::Recorder r;
    CHECK(r.take(113, 1000));   // Magnus's first draw: frame 0 of its record
    CHECK(r.offset(113, 1006) == 6);
    CHECK(r.take(113, 1006));   // a frame kept
    CHECK(!r.take(113, 1005));  // between them: not
    CHECK(r.take(12, 1005));    // another effect starts its own record
    CHECK(!r.take(113, 1300));  // past the last frame kept
    int taken = 0;
    for (int i = 0; i < 400; ++i) taken += r.take(12, 1011) ? 1 : 0;  // frame 6 of the casting
    CHECK(taken == effect_capture::kMostDrawsAFrame);
    CHECK(r.textureTold(7) && !r.textureTold(7));
    r.wrote(effect_capture::kMostBytes);
    CHECK(r.full() && !r.take(12, 1015));
}

TEST(effect_capture_writes_draws_and_small_textures) {
    DrawSetup s{};
    const effect_capture::Vertex v[2] = {{1, 2, 0.5f, 0.01f, 0xFFFFFFFF, 0, 0, 0}, {3, 4, 0.5f, 0.01f, 0, 0, 1, 1}};
    const uint16_t ix[3] = {0, 1, 0};
    const std::string t = effect_capture::drawText(113, 10, s, 5, 128, 128, v, 2, ix, 3);
    CHECK(t.find("effect 113 +10 world") == 0);
    CHECK(t.find(" i 0 1 0") != std::string::npos);
    std::vector<uint32_t> argb(256 * 128, 0xFF808080u);
    const std::string tex = effect_capture::textureText(5, 256, 128, argb.data());
    CHECK(tex.find("texture 5 256x128 as 32x32") == 0);
    CHECK(effect_capture::firstText(89, 89) == "first 89 after skill 89\n");
}
