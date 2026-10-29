#pragma once
#include <windows.h>

#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::partywin {

// 3.0.11-beta.39 (stage 7 of the new interface; the owner: «делай»): [interface] party - the party's panel at the
// top left under the character block (core/party_panel). Its members from the zone server's packets: the list
// (0x00fb: each seat's map, leader, online), one joining or changed (0x01e9), one gone (0x0105), their HP as the server
// sends it to those near (0x0106; the character's own never); a member seen dying (0x0080) at 0. On the game's thread.
void packet(const packets::Event& e);
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);

}  // namespace uagfx::win::partywin
