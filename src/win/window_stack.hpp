#pragma once
#include "core/window_sizes.hpp"
#include "core/json.hpp"
#include <windows.h>

#include <optional>

#include "core/frame.hpp"
#include "core/window_stack.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::windowstack {

// 3.0.11-beta.108 (the concept «Концепция окон и инвентаря», stage 1; the owner 31.10: «окна одно поверх другого не
// появляются», answers 4-8 «ок»): «Речі», «Характеристики», «Навички», «Спорядження», the shop, the NPC talk and
// «Довідник» in one stack (core/window_stack) - drawn bottom up; the mouse top down, a press one takes raising it;
// their keys the top's first, Esc closing the top one that has none of its own (the chat's line empty); the
// controller the top's (pad runtime); the parts under the cursor the top first (pad game_window); the top of two or
// more in a gold edge. On the game's thread.
// Each frame, after the windows' own: who is shown; one opening where another stands goes beside it (question 4: the
// target's width x height, `k` its pixels a game's)
void frame(int targetWidth, int targetHeight, double k);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(UINT m, WPARAM wp, LPARAM lp);
std::optional<winstack::Win> top();
bool marked(winstack::Win w);  // its gold edge now
// 3.0.11-beta.115 (question 7 «ок»: the D-pad goes into the other window too): the controller's frame past `from`'s
// edge toward the window docked beside it (`right` - its right edge) - that one raised, the controller's now; false:
// none docked there
bool padCross(winstack::Win from, bool right);
// 3.0.18-beta.151 (the owner: the equipment window not sized by pulling): a window's own size - its
// bottom right corner pulled with the mouse (a gold corner under the pointer there), kept in
// uaro-graphics\window-sizes.txt; 1 for one never pulled or not sized so (its own scale times this)
float sizeOf(winstack::Win w);
// 3.0.24-beta.48 (the audit 28.11, the owner's «сделать»): «Речі», «Склад», «Візок» - their own columns and rows (the
// corner adds whole cells); each tells its cell's step (a cell with its gap, the target's pixels) each frame
windowsizes::Cells cellsOf(winstack::Win w);
// 3.0.24-beta.52 (the audit 28.11, item 10): a list window's («Навички», the NPC's shop) rows by its corner; the step
// set by setCellStep - a row's height
int rowsOf(winstack::Win w);
void setCellStep(winstack::Win w, float px);
// 3.0.22-beta.39 (the MCP's `windows`): the stack's windows, the top first, with their boxes (the target's
// pixels); the window whose corner grip is shown and the one pulled; the game's cursor there
json::Value describe();
// 3.0.22-beta.40: a shown window of the stack over (x, y), the target's pixels (hud_edit's grip not there)
bool covers(int x, int y);

}  // namespace uagfx::win::windowstack
