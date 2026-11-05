#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::groundwin {

// 3.0.11-beta.283 (the owner's features 11 and 12): the items on the ground named on small plates over their cells
// (core/ground_items from the server's packets): [interface] drop_names - all of them while Alt is held;
// [interface] drop_highlight - the valuable ones always (a card purple, a weapon or an armour blue, a dear one gold).
// Their cells projected by the game's camera at the character's height; the names the game's own (its item table,
// the unidentified the pack's). Under the game's windows, as the world is. On the game's thread.
void packet(uint16_t id, const uint8_t* packet);
void frame(HWND game, bool names, bool highlight, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::groundwin
