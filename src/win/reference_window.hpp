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
// 3.0.18-beta.239: a live window of the new interface (uikit/reference_live) - moved, sized by its corner, a real
// search field; the caret outside the field, the hotbar's F-keys and the Alt windows stay the game's.
void frame(HWND game, bool enabled, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool key(UINT m, WPARAM wp, LPARAM lp);  // true: the message is the window's, not the game's
// 3.0.18-beta.239: before uaUI's field takes the keys - its own while its search field has the caret (↑ ↓ the rows,
// Enter opens, Esc clears then closes, PgUp/PgDn, Alt+← and Backspace on an empty field back)
bool fieldKey(UINT m, WPARAM wp, LPARAM lp);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// The minimap's line with the map's name: that map's card (when the window is on); 3.0.11-beta.9: a monster's line
// under it - the monster's card
void openMap(const std::string& code);
void openMob(int id);
// 3.0.11-beta.70: the tracker's entries - a pinned quest's card (by its slug), an item's
void openQuest(const std::string& slug);
void openItem(int id);
// 3.0.11-beta.21: uaRO Pad (src/pad) while the window is open - its D-pad picks a row, A opens it, B goes back (the
// first page: closes), Start closes, the bumpers scroll; the character does nothing meanwhile
bool isOpen();
bool on();  // 3.0.18-beta.242: [interface] reference on - its links elsewhere shown (the item card's «У довіднику ›»)
// 3.0.18-beta.241 (the concept's pad, «as in other windows»): Up Down Left Right walk the gold frame over the body's
// controls (on a map the pad took: its cross), Enter (A) clicks the framed one (a map: the cross takes it), Back (B)
// back (the cross: off the map), Lead (X) «Вести сюди» (the cross: the next mark), Search (Y) the on-screen keyboard
// into the search field, Prev / NextSection (LB / RB) the rail's sections (the cross: the map's zoom)
enum class PadCommand {
    Up, Down, Enter, Back, Close, PageUp, PageDown, Left, Right, Lead, Search, PrevSection, NextSection
};
void padCommand(PadCommand c);
// Each tick: the sticks (-1..1, +y up) for `seconds` - the left one moves the cross on a map, the right one scrolls
// the body (on a map: moves the map under the cross)
void padSticks(float lx, float ly, float rx, float ry, float seconds);
bool padCrossOn();  // the cross on a map: the left stick is its, not the frame's steps
// 3.0.11-beta.22: a pad button bound to «Довідник» (uaRO Pad's action «reference») - open it, or close it
void toggleFromPad();

// 3.0.11-beta.109 (the concept «Концепция окон и инвентаря», question 4): where it is on the target (win/window_stack:
// a window opening where another stands goes beside it); false not shown
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::referencewin
