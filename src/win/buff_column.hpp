#pragma once
#include <windows.h>

#include "core/config.hpp"
#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::buffcolumn {

// 3.0.11-beta.35 (stage 7 of the new interface, the HUD - the buffs first; [interface] buffs = column | row): the
// character's buffs in the driver's own column at the right edge under the minimap (core/buff_panel), the game's own
// icons hidden. uaRO.exe lays its icons out as effects of the player's icon effect (FUN_005d56a0, each one's .tga by
// CSession::IconImage 0x648BC0): its one call there (0x5D5785) answers none while the column shows them, and the game
// lays them out again at once (the icon effect's own message 0x2c, as each status icon packet sends it). The icons
// from the game's list (0x728484), their times from the packets (win/buff_overlay's), their pictures from the game's
// own files. On the game's thread.
bool installBuffIcons();  // the call redirected (it answers as the game's until the column shows); false: another build
// 3.0.11-beta.63: `overflow` - what the column does not hold: under «ще N» or in a second column
void frame(HWND game, BuffsLook look, BuffsOverflow overflow, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
// The mouse over it is its own: the name in a tip, a click stars a favourite, «назви» opens the names
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// The game's icons hidden for the column: the times on them ([buffs] timers) have none to go on
bool hidingGameIcons();

}  // namespace uagfx::win::buffcolumn
