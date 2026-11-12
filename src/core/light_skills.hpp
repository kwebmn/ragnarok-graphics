#pragma once
#include <cstdint>

namespace uagfx::lightskills {

// 3.0.18-beta.186 (the owner: «делай 2 без остановки» - «облегчённые эффекты на слабых ПК»): uaRO.exe's own light
// skill effects (its /mineffect: the effects' _min variants, chosen as each effect starts) while the frames run short
// of [frames] keep_fps in a crowd of skills - a fight, a guild war - and the game's own again once the frame rate is
// back or the skills are gone for twenty seconds. The parts that need no Windows (win/light_skills).
constexpr uint32_t kWindowMs = 2000;
constexpr int kSlowWindows = 2;        // windows in a row under kSlow of the frame rate kept, with skills crowding
constexpr double kSlow = 0.9, kFast = 1.1;
constexpr uint32_t kCrowd = 30, kCalm = 5;  // skills' draws a frame: a crowd of them, hardly any
constexpr uint64_t kBackMs = 20000;    // fast (or calm) this long: the game's own effects again

struct State {
    bool on = false;
    int slow = 0;
    uint64_t easySince = 0;  // since when the frame rate is back (or the skills gone); 0: not
};

// A window of `ms` with `frames` drawn, `skillDraws` of them the skills' draws, the frame rate kept at `target`
// (0: none): whether the game's light skill effects are wanted now
bool step(State& s, uint64_t now, uint32_t ms, uint32_t frames, uint32_t skillDraws, int target);

}  // namespace uagfx::lightskills
