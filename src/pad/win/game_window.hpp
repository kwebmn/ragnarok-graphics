#pragma once
#include <windows.h>

#include "pad/core/config/config.hpp"

// What the pad does to the game's window and the Windows cursor around it.
namespace uapad::gamewindow {

// Shows the normal cursors over the title bar and the borders: the client hides the cursor on every
// WM_SETCURSOR, outside its client area too. Takes over the window procedure once per window.
void subclass(HWND game);
// On: no title bar and borders, the picture where `where` says (the old style and place are remembered).
// Off: the old style and place back. Windowed only; false when nothing could be done now.
bool setBorderless(HWND game, bool on, const WindowPlacement& where);
// Keeps the Windows cursor inside the game's client area while `on` (set again when the system drops it).
void lockCursor(HWND game, bool on);
// The Windows cursor is over the game's client area and no other program's window is on top of it there.
bool cursorOverGame(HWND game, POINT screen);
// 3.0.3-beta.8: the driver's panel of the game windows in the game's picture - its rectangle in the game window's
// client pixels (Windows' size of them), whose clicks are not the game's; all: none of them is (the panel dragged);
// nullptr: no panel. On the game's thread, as the gate. 3.0.9-beta.4: the answer card's too, each its own;
// 3.0.11-beta.1: the new minimap's (prototype). 3.0.11-beta.24: `game` - the window the rectangle is in
// 3.0.11-beta.7: «Довідник»; 3.0.11-beta.31: the new minimap's big map
// 3.0.11-beta.35: the buffs' column; 3.0.11-beta.36: the character block; 3.0.11-beta.38: the target's card;
// 3.0.11-beta.39: the party's panel; 3.0.11-beta.63: the buffs' further columns; 3.0.11-beta.70: the tracker
enum class Blocker { Panel, Card, Minimap, Reference, BigMap, Buffs, Character, Target, Party, Hotbar, Chat, Items,
                     Stats, BuffsLines, Tracker, HudEdit, PadTiles, PadAssign, Skills, Equip };
// 3.0.11-beta.33: their number (win/world_mouse names each)
constexpr int kBlockerCount = int(Blocker::Equip) + 1;
void setBlocked(Blocker who, HWND game, const RECT* rect, bool all);
// 3.0.11-beta.77: a part's rectangle as it set it (the client's pixels) - false while it has none
bool blockerRect(Blocker who, RECT& out);
// 3.0.11-beta.24: the part the game's own cursor is over (x, y of the game's width x height picture), -1 for none:
// the world under it is not the cursor's then (win/world_mouse). On the game's thread.
int blockedUnder(int x, int y, int width, int height);
// 3.0.11-beta.33: a press one of the driver's parts took in the game window's messages (bit i: the device's button
// i) - not the game's until it is let go, though the part be gone before the game's next mouse poll (its ✕); taken
// by that poll. On the game's thread.
void claimPress(uint8_t buttons);
uint8_t takeClaimed();
// Presses or lets go F1-F9 (1-9) as if on the keyboard: the client gets it like any key press.
void sendFKey(int key, bool down);
// Lets the cursor go if we confined it (the client is closing).
void releaseCursor();

}  // namespace uapad::gamewindow
