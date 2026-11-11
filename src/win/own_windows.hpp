#pragma once
#include <windows.h>

#include <string>
#include <vector>

#include "core/closed_windows.hpp"

namespace uagfx::win::ownwin {

// 3.0.18-beta.120 (the concept «Ігровий інтерфейс», stage 2): our windows closed by ✕ - read once
// from uaro-graphics\closed-windows.txt, written at each change; their switches untouched. On the game's thread.
bool closed(closedwin::Window w);
void setClosed(closedwin::Window w, bool closed, const char* why);
inline void toggle(closedwin::Window w, const char* why) { setClosed(w, !closed(w), why); }

// 3.0.18-beta.120 (stage 3): the character block's second row - the windows whose switches are on (each frame from
// the config), whether each is open now, a press opening or closing it; Ctrl+H/B/J/G/K the same (their WM_CHAR - a
// control character the chat's field would take, Ctrl+H a backspace - swallowed)
void setSwitches(const closedwin::Switches& on);
// 3.0.18-beta.125 (item 2: «Клавіші»): the letters [interface] key_* give (each frame); a window's key now (""
// none)
void setKeys(const closedwin::Keys& k);
std::string keyOf(closedwin::Own o);
std::vector<closedwin::Own> row();
bool lit(closedwin::Own o);
void press(closedwin::Own o, const char* why);
bool key(UINT m, WPARAM wp, LPARAM lp);

}  // namespace uagfx::win::ownwin
