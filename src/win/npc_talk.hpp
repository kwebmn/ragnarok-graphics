#pragma once
#include <windows.h>

#include "core/frame.hpp"
#include "core/game_packets.hpp"
#include "pad/core/logic/controller.hpp"

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
// 3.0.11-beta.255: `remember` - [interface] npc_remember, the option chosen last time at the same menu chosen again
void frame(HWND game, bool on, bool remember, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(UINT m, WPARAM wp, LPARAM lp);
void beforeGameDraw();
bool shown();
bool numberOpen();  // 3.0.11-beta.258: the game's number field open (its hint line shows the digits)
// 3.0.11-beta.255 (the concept «Разговор с NPC с геймпада», stage 1): a controller's command for the talk while this
// window shows - the menu's moves by its two columns (D-pad, LB / RB), A's (Next, OK) held kGuardMs after a new page
// or menu; true when taken here (done or held), false: the pad's own way (client_npc)
bool padCommand(uapad::DialogCommand c);

// 3.0.11-beta.109 (the concept «Концепция окон и инвентаря», question 4): where it is on the target (win/window_stack:
// a window opening where another stands goes beside it); false not shown
bool box(int& x, int& y, int& w, int& h);

// 3.0.13-beta.10 (the owner's pick 29 of «50 идей»: «Esc закрывает картинку NPC (cut-in), которая иногда зависает»):
// [interface] esc_cutin - Esc with the NPC's picture shown closes it as the server does (the game mode's message 0x71,
// no name, type -1); the key goes on to the rest
void cutinEsc(UINT m, WPARAM wp);
void setCutinEsc(bool on);

}  // namespace uagfx::win::npcwin
