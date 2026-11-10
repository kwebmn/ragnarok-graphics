#pragma once
#include <cstdint>

namespace uagfx::uiglide {

// 3.0.18-beta.58 (docs/superpowers/plans/2026-11-10-uaui-possibilities.md, stage 2: [interface] uaui_motion): a bar's
// shown value gliding to its real one - most of the way in 140 ms, eased out; at once with the motion off or the
// first time
struct Glide {
    float shown = 0;
    uint64_t at = 0;
    bool started = false;
    float step(float target, uint64_t nowMs, bool on);
    bool moving(float target) const { return started && shown != target; }
};

}  // namespace uagfx::uiglide
