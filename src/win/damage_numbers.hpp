#pragma once
#include <windows.h>

#include "core/frame.hpp"
#include "win/gpu/renderer.hpp"

namespace uagfx::win::damagewin {

// 3.0.16-beta.2: [interface] damage_numbers (core/damage_numbers.hpp) - FUN_00557eb0 (an actor's «show this
// number») sent to ours: with the switch on the number kept for us and the game's sprites not made; off, the game's
void install();
void frame(HWND game, bool on, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::damagewin
