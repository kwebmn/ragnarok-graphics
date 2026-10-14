#pragma once
#include "core/effect_exe.hpp"

namespace uagfx::win {

// 0.27: which of the game's effects a draw is of (core/effect_exe.hpp). installEffectHook() puts a hook of its own
// into the vtable entry of the effects' primitives' Render and (0.28) at the entry of the renderer's face sort;
// false, with a log line, when the exe is another build.
bool installEffectHook();
// The effect the draw whose vertices these are is of (Kind::None: none the driver cares about).
effect_exe::Tag drawnEffect(const void* vertices);
// A frame was shown: the next one's primitives are noted anew.
void effectFrame();

}  // namespace uagfx::win
