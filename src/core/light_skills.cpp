#include "core/light_skills.hpp"

namespace uagfx::lightskills {

bool step(State& s, uint64_t now, uint32_t ms, uint32_t frames, uint32_t skillDraws, int target) {
    if (target <= 0 || ms == 0) {  // no frame rate kept: the game's own effects
        s = State{};
        return false;
    }
    const double fps = double(frames) * 1000.0 / double(ms);
    const double perFrame = frames ? double(skillDraws) / double(frames) : 0.0;
    if (!s.on) {
        s.slow = fps < kSlow * double(target) && perFrame >= double(kCrowd) ? s.slow + 1 : 0;
        if (s.slow >= kSlowWindows) {
            s.on = true;
            s.slow = 0;
            s.easySince = 0;
        }
        return s.on;
    }
    const bool easy = fps >= kFast * double(target) || perFrame < double(kCalm);
    if (!easy) {
        s.easySince = 0;
        return true;
    }
    if (!s.easySince) s.easySince = now;
    if (now - s.easySince >= kBackMs) s = State{};
    return s.on;
}

}  // namespace uagfx::lightskills
