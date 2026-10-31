#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::equipwin {

// 3.0.11-beta.97 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's
// «Снаряжение»): [interface] equip - «Спорядження» (core/equip_window) in place of the game's equipment window (UI
// window 10, g_windowMgr + 0x94, its class's vtable 0x698668) while that is open: its shown flag held at 0, its close
// through its own message (6, 0x87), a drag from «Речі» let go over ours given to its drop (FUN_0046f550: the thing
// worn where it goes). The worn things are the bag's (the session's list, ITEM_INFO + 0xC worn there; cards + 0x1C ..
// + 0x28), their card slots the reference pack's. A double click takes one off (the game mode's 0x15 with the slot's
// number), a press drags it (0x16, as the game's own window), the controller's list puts one on (0x14, the ammo 100).
// On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool drop(int x, int y, uint32_t drag);  // a drag let go over it (win/world_mouse)
void beforeGameDraw();
bool shown();
// The controller's (pad runtime equipPad): the D-pad or the left stick goes round the slots (the list of what fits
// while it shows), A takes off (or lists what fits an empty slot, or puts on from the list), X lists what fits a full
// one, Y its card, B goes back from the list or closes, Start closes
enum class PadCommand : uint8_t { Up, Down, Left, Right, Put, Change, Card, Back, Close };
void padCommand(PadCommand c);

// 3.0.11-beta.109 (the concept «Концепция окон и инвентаря», question 4): where it is on the target (win/window_stack:
// a window opening where another stands goes beside it); false not shown
bool box(int& x, int& y, int& w, int& h);
// 3.0.11-beta.109: a new place on the target, kept (3.0.11-beta.111: `keep` false - not yet: it follows a window
// being dragged)
void moveTo(int x, int y, bool keep = true);
bool moving();  // 3.0.11-beta.111: its title dragged now (docked with the other, it leads)

}  // namespace uagfx::win::equipwin
