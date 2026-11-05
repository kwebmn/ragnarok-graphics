#pragma once
#include <windows.h>

#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::npctagwin {

// 3.0.11-beta.256 (the concept «Разговор с NPC с геймпада», stage 2; the owner: «1. да 2. да»): with the controller
// in hand, the NPC its Talk button takes now carries a small plate over its head - the button's glyph and the NPC's
// name (asked of the server as the game asks, 0x0094 -> 0x0095); the others within reach a faint dot each. None in a
// fight (the pad's npcTag). The right stick moves the plate (the pad's npcChoice_).
void packet(const packets::Event& e);
void frame(HWND game, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
void beforeGameDraw();  // the names wanted asked, on the game's thread

}  // namespace uagfx::win::npctagwin
