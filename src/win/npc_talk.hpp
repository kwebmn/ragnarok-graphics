#pragma once
#include <windows.h>

#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::npcwin {

// 3.0.11-beta.98 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's «Разговор
// с NPC»): [interface] npc_talk - the NPC talk (core/npc_talk) in place of the game's text window (UI window 0x10 at
// 0x727F7C, its class 0x698CF0) and menu (0x11 at 0x727F80, 0x699080) while they are open: their shown flags held at
// 0, their buttons pressed as a click on them (the pad's clientnpc: Next 0x98, Close 0x87, the menu's OK 0x76 after its
// list's choice +0x64, Cancel 0x77). The words come from the server's packets (0x00b4, 0x00b7 through win/packet_tap) -
// the text window's +0x88 set (a Next pressed) begins a new page; the game's number field stays the game's. Keys
// 1-9 choose, Enter answers, Esc closes - while the chat's line is empty. On the game's thread.
void packet(const packets::Event& e);
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(UINT m, WPARAM wp, LPARAM lp);
void beforeGameDraw();
bool shown();

}  // namespace uagfx::win::npcwin
