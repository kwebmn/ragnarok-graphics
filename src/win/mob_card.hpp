#pragma once
#include <windows.h>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::mobcardwin {

// 3.0.11-beta.279 (the owner's feature 5): [interface] mob_card - a small card by the monster under the mouse cursor
// (or the controller's target) after a fifth of a second: its name and level, HP, element, race and size, and how the
// fight goes by the server's rules (core/mob_battle): the character's chance to hit it, its chance to hit the
// character, its blow less the character's DEF and its damage a second, the character's blow and the blows it takes.
// The character's numbers from the status window's (HIT, FLEE, perfect dodge, DEF, ATK). It takes no clicks: the
// monster stays the world's. On the game's thread.
void frame(HWND game, bool on, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::mobcardwin
