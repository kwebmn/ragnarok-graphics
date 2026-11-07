#pragma once
#include <cstdint>

#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::worldsigns {

// 3.0.16-beta.7 (the owner: «делай все» - the shops' and the chat rooms' signs): [interface] signs - the game's signs
// over the characters (core/world_signs) drawn as the new look's plates: the title from the server's packets (0x0131
// a shop's, 0x00d7 a room's with its places), the place the game's window's. The game's windows are not drawn
// (their shown flags held at 0 through its windows' draw, win/windows_draw) and keep taking the clicks. On the game's
// thread.
void packet(const packets::Event& e);
// In the game's windows' draw: whether this window is a sign drawn anew (noted then: its place, whose)
bool take(uint32_t window);
void frame(bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::worldsigns
