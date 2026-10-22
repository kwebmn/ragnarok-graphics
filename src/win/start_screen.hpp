#pragma once
#include <windows.h>

#include "core/start_screen.hpp"

namespace uagfx::win {

// 0.40: a panel over the game's black window while the renderer starts (core/start_screen.hpp) - what it does and a
// bar. At the monitor's real pixels (sharp under Windows scaling), owned by the game's window (above it, not above
// other programs), never taking the focus or the clicks. openStart() as the start begins, closeStart() once it ends.
// 2.12: the panel's language - the settings app's choice, else Windows' (decide() sets it)
void startLanguage(settings::Lang l);
void openStart(HWND game);
void showStart(const start::Progress& p);  // nothing without a panel; a new stage at once, the rest every 30 ms
void closeStart();

}  // namespace uagfx::win
