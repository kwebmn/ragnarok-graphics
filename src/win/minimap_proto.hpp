#pragma once
#include <windows.h>

#include <string>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::minimapproto {

// 3.0.11-beta.1: [interface] minimap = 1 - the new interface's minimap prototype (stage 1, the owner's «давай»
// 2026-10-28; core/minimap_proto). Each frame on the game's thread: the game's own minimap hidden as Ctrl+Tab hides it
// (each time the game makes it again: a new map, Ctrl+Tab; back when switched off), the driver's panel in its corner
// with the map's picture round the player and the player's cell under it (to set against /where); the mouse over it
// kept from the game. The log tells what each step did. 3.0.11-beta.6: its hints from the game's reference
// (win/reference_load) - the NPCs and the warps on it (the pointer over one names it), the map's monsters under it.
struct Show {
    bool on = false, npcs = true, warps = true, mobs = true;
};
void frame(HWND game, const Show& show, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
// 3.0.11-beta.31: and the wheel over the map zooms it; the big map's clicks first
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// 3.0.11-beta.4 (the owner: «на Ctrl+Tab ничего не происходит»): Ctrl+Tab while the prototype is on - its look goes
// round as the game's own minimap's (opaque, see-through, hidden); true: the key is the prototype's, not the game's
bool key(unsigned vk);
// 3.0.11-beta.9: «вести ›» in a map's card (core/route_follow) - that way on the new minimap: a line to its next move,
// two lines under the map (where to, how many moves left, where to take the move); made again wherever the player
// goes; «Ви на місці» on the map, then it stops. canFollow: the new minimap on
bool canFollow();
void follow(const std::string& target, bool walk);
void unfollow();
bool following(std::string& target, bool& walk);  // false: none

}  // namespace uagfx::win::minimapproto
