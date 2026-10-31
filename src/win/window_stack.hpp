#pragma once
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
void frame();  // each frame, after the windows' own: who is shown
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(UINT m, WPARAM wp, LPARAM lp);
std::optional<winstack::Win> top();
bool marked(winstack::Win w);  // its gold edge now

}  // namespace uagfx::win::windowstack
