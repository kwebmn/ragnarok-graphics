#pragma once
#include <windows.h>

#include <cstdint>

namespace uagfx::win::keywalk {

// 3.0.18-beta.207 (the keyboard concept https://claude.ai/artifact/SxvWxL5BM2tHtSWAvYPh9J, stage 1): the game window's
// arrows. Held while the chat line is empty they walk (core/key_walk): the pad runtime reads the walk each tick and
// gives it to the pad's controller as its stick (PadState::keyWalk) - the walk without the mouse, the arrow on the
// ground. The game does not see those arrows; any other arrow is its own as before.

bool key(UINT m, WPARAM wp, LPARAM lp);  // true: the arrow was the walk's
void lost();                             // the window lost the keys: every arrow let go
// The walk now (x right, y up); true while arrows are held, and for 0.6 s after the last let go - the controller's
// tick that sends the stop step
bool vector(float& x, float& y);
bool walking();             // arrows held now
void setModeDirs(uint8_t dirs);  // 3.0.18-beta.208: the walk mode's keys held (win/walk_mode) - walked as the arrows
void setEnabled(bool on);   // [interface] arrows_walk
bool enabled();

}  // namespace uagfx::win::keywalk
