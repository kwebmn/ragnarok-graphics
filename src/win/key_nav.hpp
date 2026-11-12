#pragma once
#include <windows.h>

#include <cstdint>

namespace uagfx::win::keynav {

// 3.0.18-beta.210 (the keyboard concept https://claude.ai/artifact/SxvWxL5BM2tHtSWAvYPh9J, stage 4): the keys in the
// driver's windows. The window of the stack opened or clicked last takes the keys once an arrow is pressed with the
// chat line empty: the arrows, Enter, Esc, Space, PgUp / PgDn and Shift+F10 (or the menu key) are then its pad's
// D-pad, A, B, X, LB / RB and Y - the pad runtime hands them to the window's own pad handler, its gold ring shows and
// its hints name the keys (padglyph key caps). A click on the ground gives the keys back to the world; words typed in
// the chat line take them back too.

bool key(UINT m, WPARAM wp, LPARAM lp);  // true: the window's
void click(bool onWindow);  // a press in one of the stack's windows (true) or on the ground (false)
void frame(int top);        // the stack's top window (its kind), -1 none - a newly opened one takes the keys' focus
bool engaged();             // the keys drive the top window now
uint16_t buttons();         // the pad bits the keys hold (kPadUp ... kPadY)
void lost();                // the window lost the keys

}  // namespace uagfx::win::keynav
