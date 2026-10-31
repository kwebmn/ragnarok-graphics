#pragma once
#include <windows.h>

#include <cstdint>
#include <vector>

#include "core/bar_hint.hpp"
#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::hotbarwin {

// 3.0.11-beta.40 (stage 8 of the new interface, the hotbar first; the owner: «делай дальше»): [interface] hotbar - the
// game's hotkeys bar redrawn at the bottom middle (core/hotbar_panel). The game keeps its bar (UI window 0x24,
// g_windowMgr + 0x9C, its class's vtable 0x6985D0): its shown flag (+0x38) held at 0 while this one shows, its keys
// (F1-F9, the battle mode's letters - they send the window its message 0x29 hidden or not) its own. What is in each
// slot from the game's session (+0x314 a slot's words, +0x4C4 a skill's level), an item's count from the bag; a click
// sends the window the same message 0x29 the keys do. The pencil shows the game's own bar to drag skills and items
// into it. On the game's thread.
// 3.0.11-beta.78: ownBars - [interface] hotbar_per_character, each character's own bar (the new bar on or not)
// 3.0.11-beta.86: padHints - [interface] pad_hints, the controller's buttons on its cells while played with it
void frame(HWND game, bool on, bool ownBars, bool padHints, unsigned codePage, double k, int targetWidth,
           int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// 3.0.11-beta.45: a key of its cells pressed (F1-F9; the battle mode's letters while the chat does not take them) - the
// cell shown pressed; the key stays the game's
void key(UINT m, WPARAM wp, LPARAM lp);
// 3.0.11-beta.53: just before the game draws its windows (win/windows_draw) - its own bar hidden (shown while edited)
void beforeGameDraw();
// A skill or an item dragged from the game's windows and let go at the game's (x, y) over one of its cells (`drag`: the
// game mode's drag, + 0x32C): the game's own bar takes it as dropped on that cell (its drop, vtable + 0x2C - the slot
// set, the drag ended; the battle mode's row the cell's); false: not over a cell (win/world_mouse asks)
bool drop(int x, int y, uint32_t drag);
// 3.0.11-beta.68 (the concept's «Ваша панель: Fire Bolt ×2…»): the skills in the game's bar now (its 27 slots, as the
// session keeps them - the new bar shown or not), each with its name as the game shows it; in the slots' order
std::vector<barhint::Skill> barSkills();

// 3.0.11-beta.91 (the concept «Контроллер в интерфейсе uaRO», stage 6): a slot of the game's bar (0-26) given an item
// (its number's words, "501") or a skill ("##AEGIS") at a level, the bar told to fill its own (each character's own
// kept as the other changes are); its words now («Red Potion», «Double Strafe»; "" empty). On the game's thread
bool assign(int slot, const std::string& value, int level);
std::string slotWords(int slot);

}  // namespace uagfx::win::hotbarwin
