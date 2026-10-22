#pragma once
#include <windows.h>

#include "pad/core/logic/controller.hpp"

// 0.7: the uaRO client's NPC windows - the text with its Next or Close button, the menu, the number field - read
// in its memory and answered through their own buttons, as a click on them would.
namespace uapad::clientnpc {

// True when the running exe is the uaRO build whose windows we know (checked by their code).
bool supported();
// The NPC window on top: the number field, then the menu, then the text window - the order Enter takes them.
NpcDialog read();
// Does what the command asks of the open window; false when that window is not there (any more).
bool answer(DialogCommand c);
// Puts `value` into the number field the way the keyboard would: the old digits out, the new ones in.
void typeNumber(HWND game, int value);

}  // namespace uapad::clientnpc
