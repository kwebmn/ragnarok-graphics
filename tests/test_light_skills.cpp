// 3.0.18-beta.186 (the owner: «делай 2 без остановки» - the game's own light skill effects, /mineffect's, when the
// frames run short in a crowd of skills): when the game's light effects are wanted, by two-second windows
#include "core/light_skills.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
// a two-second window: `fps` frames a second, `perFrame` skills' draws a frame
bool window(lightskills::State& s, uint64_t& now, double fps, int perFrame, int target = 60) {
    now += lightskills::kWindowMs;
    const uint32_t frames = uint32_t(fps * lightskills::kWindowMs / 1000.0);
    return lightskills::step(s, now, lightskills::kWindowMs, frames, frames * uint32_t(perFrame), target);
}
}  // namespace

TEST(light_skills_on_when_skills_crowd_a_slow_game) {
    lightskills::State s;
    uint64_t now = 1000;
    CHECK(!window(s, now, 40, 80));  // one slow window: not yet
    CHECK(window(s, now, 40, 80));   // two: the game's light effects
    CHECK(window(s, now, 70, 80));   // fast again: kept a while (a fight comes in waves)
    for (int i = 0; i < 9; ++i) window(s, now, 70, 80);
    CHECK(!window(s, now, 70, 80));  // twenty seconds and more fast: the game's own effects back
}

TEST(light_skills_not_for_a_slow_game_without_skills) {
    lightskills::State s;
    uint64_t now = 1000;
    for (int i = 0; i < 5; ++i) CHECK(!window(s, now, 35, 3));  // slow, but few skills: they are not why
    CHECK(!window(s, now, 40, 80, 0));                           // no frame rate kept: never
    CHECK(!window(s, now, 40, 80, 0));
    lightskills::State t;
    CHECK(!window(t, now, 40, 80));
    CHECK(window(t, now, 40, 80));
    for (int i = 0; i < 10; ++i) window(t, now, 40, 2);  // the fight over: the skills gone, slow or not
    CHECK(!window(t, now, 40, 2));
    CHECK(!window(t, now, 40, 80, 0));  // the frame rate no longer kept: off at once
}
