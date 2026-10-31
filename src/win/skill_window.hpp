#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::skillswin {

// 3.0.11-beta.96 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's «Навыки»):
// [interface] skills - «Навички» (core/skill_window) in place of the game's skill tree (UI window 0x25, g_windowMgr +
// 0x118, its class's vtable 0x69B9D0) while that is open: its shown flag held at 0, its close through its own
// message (6, 0x87). The skills are the game's session's list (+0x2D8: a node's 32-byte record at +8 - id, type,
// learned level, SP, the server's leave to raise, range, AEGIS name), the levels they are used at its array (+0x534),
// the free points 0x771FA4, the job +0xE44; the tabs from the server's tree (core/skill_tree). A «+» asks as the
// game's own does (the game mode's message 0x4A: packet 0x0112), a double click uses it (0x7E with its record, as
// the game's window), a press drags it (0x51: id, name, level - the game's drag, dropped on the skill bar). On the
// game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
void beforeGameDraw();  // the game's window hidden before it draws its windows (win/windows_draw)
bool shown();
// The controller's (pad runtime skillsPad): the D-pad or the left stick moves the frame up and down, left and right
// the level it is used at, LB / RB the tabs, A uses, Y raises, X «Призначити», B or Start close
enum class PadCommand : uint8_t { Up, Down, Less, More, TabBack, TabOn, Use, Raise, Assign, Close };
void padCommand(PadCommand c);

// 3.0.11-beta.109 (the concept «Концепция окон и инвентаря», question 4): where it is on the target (win/window_stack:
// a window opening where another stands goes beside it); false not shown
bool box(int& x, int& y, int& w, int& h);
void moveTo(int x, int y);  // 3.0.11-beta.109: a new place on the target, kept

}  // namespace uagfx::win::skillswin
