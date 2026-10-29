#pragma once
#include <windows.h>

#include <string>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::referencewin {

// 3.0.11-beta.7 ([interface] reference = 1; the owner: «сделай по максимум»): the «Довідник» window over the game -
// Ctrl+F opens and closes it (no key of the client's own: its key handler FUN_004d96e0 has no case for F), Esc closes
// it. While it is open the keys are its: letters, digits and the space by the keys' own places, so a Ukrainian layout
// types the English names too; Backspace, Delete (all of it), Enter (the first found), Alt+← and the mouse's back
// button (back), the wheel, PgUp/PgDn. A click outside it is the game's. Pages: core/reference_pages; shapes:
// core/reference_window; data: win/reference_load.
void frame(HWND game, bool enabled, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool key(UINT m, WPARAM wp, LPARAM lp);  // true: the message is the window's, not the game's
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// The minimap's line with the map's name: that map's card (when the window is on); 3.0.11-beta.9: a monster's line
// under it - the monster's card
void openMap(const std::string& code);
void openMob(int id);

}  // namespace uagfx::win::referencewin
