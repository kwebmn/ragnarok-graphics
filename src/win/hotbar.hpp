#pragma once
#include <windows.h>

#include <cstdint>
#include <vector>

#include "core/bar_hint.hpp"
#include "core/frame.hpp"
#include "core/pad_carry.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::hotbarwin {

// 3.0.11-beta.40 (stage 8 of the new interface, the hotbar first; the owner: «делай дальше»): [interface] hotbar - the
// game's hotkeys bar redrawn at the bottom middle (core/hotbar_panel). The game keeps its bar (UI window 0x24,
// g_windowMgr + 0x9C, its class's vtable 0x6985D0): its shown flag (+0x38) held at 0 while this one shows, its keys
// (F1-F9, the battle mode's letters - they send the window its message 0x29 hidden or not) its own. What is in each
// slot from the game's session (+0x314 a slot's words, +0x4C4 a skill's level), an item's count from the bag; a click
// sends the window the same message 0x29 the keys do. 3.0.11-beta.147 (the owner: «да, сделай так и предусмотри
// возможность чтобы можно было батлмод кнопкой на этой панели включать»): no pencil (the game's own bar shown to edit
// it) - a cell's icon held and dragged to another cell swaps the two slots, well off the bar empties its slot, a right
// click shows its card (the game's bar's own); the battle mode's button turns the mode as /battlemode. On the game's
// thread.
// 3.0.11-beta.78: ownBars - [interface] hotbar_per_character, each character's own bar (the new bar on or not)
// 3.0.11-beta.86: padHints - [interface] pad_hints, the controller's buttons on its cells while played with it
// 3.0.11-beta.125: ammo - [interface] hotbar_ammo, the ammo worn and how many left on a pill over it (win/ammo_watch)
// 3.0.11-beta.143: counts - [interface] ground_counts, a ground skill's walls standing or a trap's Traps left on its
// slot (win/ground_units), its edge flashing when a wall burnt out
// 3.0.11-beta.276 (the owner's feature 7): [interface] delay_ring - the delay after a skill swept over the skills'
// cells
void setDelayRing(bool on);
// 3.0.16-beta.8 (the owner: «для магов и пристов ... гемов, а для монков ... сферы»): [interface] hotbar_supplies -
// what the bar's skills spend (gems, Holy Water, stones, bottles) and the spirit spheres on pills after the ammo's
void setSupplies(bool on);
// 3.0.16-beta.13 (the owner's picks 1, 5, 6 of «20 идей»): [interface] skill_needs - a skill short of its gem or its
// spheres a red plate («×0», «3/5»); [interface] combo_window - the skill that chains now gold with its time shrinking,
// Asura ready (5 spheres, Fury) blue
void setNeeds(bool needs, bool comboWindow);
// 3.0.16-beta.22: [interface] ground_timers - the own walls' and traps' time left on their slots too
void setTimers(bool on);
// 3.0.11-beta.281 (the owner's feature 16): [interface] potion_warn - the bar's healing items: a chime at 10 left and
// at none, their counts red from 10 down (core/potion_warn)
void setPotionWarn(bool on);
// 3.0.11-beta.288 (the owner's feature 20): Ctrl+Shift+1..3 - the character's own bar's three profiles (with
// [interface] hotbar_per_character); true: the key was theirs
bool profileKeyPress(UINT m, WPARAM wp, LPARAM lp);
int profileNow();
void frame(HWND game, bool on, bool ownBars, bool padHints, bool ammo, bool counts, unsigned codePage, double k,
           int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// 3.0.11-beta.147: a left press at the game window's client point on a cell with a skill or an item - the hotbar's own
// (its icon lifted), not «Розміщення інтерфейсу»'s drag of the whole bar (win/hud_edit asks)
bool ownsPress(HWND game, LPARAM lp);
// While a press on a cell's icon is held: all of the mouse's buttons its own (its let go the drop, wherever the pointer
// is) - asked before the other parts
bool held(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// 3.0.11-beta.45: a key of its cells pressed (F1-F9; the battle mode's letters while the chat does not take them) - the
// cell shown pressed; the key stays the game's
void key(UINT m, WPARAM wp, LPARAM lp);
// 3.0.11-beta.53: just before the game draws its windows (win/windows_draw) - its own bar hidden
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

// 3.0.11-beta.121 (the concept's stage 5): its cells shown as places on the target, each with the game's slot it
// keeps (assign's: the row shown, the battle mode's three rows); none while not shown
std::vector<uagfx::padcarry::Place> cellPlaces();

// 3.0.11-beta.254: a controller's button used the game's slot (0-26) - its cell shown pressed as for its key
void padUsed(int slot);

}  // namespace uagfx::win::hotbarwin
