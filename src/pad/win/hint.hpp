#pragma once
#include <windows.h>

#include <string>

// 0.7: one line over the bottom of the game window - what the pad's buttons do in an NPC window. It never takes
// the focus, and the mouse goes through it.
namespace uapad::hint {

void show(HWND game, const std::wstring& text);
void hide();

}  // namespace uapad::hint
