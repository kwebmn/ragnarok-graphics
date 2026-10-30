#pragma once
#include <windows.h>

#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::castwin {

// 3.0.11-beta.48 (the concept's cast label; the owner on its version 4: «каст: над героем полосой, но не как
// старый»): [interface] cast - the character's cast over the hero in the new look (core/cast_bar) where the game's
// own gauge stands, that one hidden while it shows. What is cast from the server's 0x013e (the character's own); its
// place, its start and end from the game's player (the game mode's world + 0x3C: its gauge window + 0x220, made and
// moved by the game each frame; the cast's end + 0x22C, its start + 0x230 by timeGetTime). On the game's thread.
void packet(const packets::Event& e);
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::castwin
