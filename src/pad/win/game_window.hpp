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
// 3.0.11-beta.1: the new minimap's (prototype)
enum class Blocker { Panel, Card, Minimap };
void setBlocked(Blocker who, const RECT* rect, bool all);
// Presses or lets go F1-F9 (1-9) as if on the keyboard: the client gets it like any key press.
void sendFKey(int key, bool down);
// Lets the cursor go if we confined it (the client is closing).
void releaseCursor();

}  // namespace uapad::gamewindow
