#pragma once

namespace uagfx::win {

// 3.0.1-beta.8: ROExt's work (core/roext.hpp) at the driver's load, through uaRO.exe's import slots - when ROExt
// itself is gone: its dinput8.dll moved aside by the installer (uaro-graphics\original), [client] roext on, no ROExt
// the game loads from its folder. The game's window as ROExt made it (CreateWindowExA), its messages (PeekMessageA:
// the mouse buttons, Alt+F4, Ctrl+W, Scroll Lock, the wait behind other windows), its text's code page.
void installRoext();
// Its lines for the driver's log, which opens later (at the driver's start: real_ddraw)
void logRoext();

}  // namespace uagfx::win
