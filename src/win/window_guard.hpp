#pragma once
#include <cstdint>

namespace uagfx::win {

// 3.0.11-beta.194 (R-ZS5B2D: uaRO.exe crashed on Enter 17 s into geffen_in, its windows' manager still naming its
// menu, a window already freed): uaRO.exe's Enter (0x4CE990, its one call 0x4D9BE5) called through the driver - a
// field of the windows' manager naming a window gone from its list (core/window_guard) cleared first, not called
// through; logged. False, with a log line, when the exe is another build.
bool installWindowGuard();
// The same check before the game draws its windows each frame (win/windows_draw): Esc's menu (MakeWindow 0x30 shows
// the one its field names) and the windows the packets talk to alike
void mendWindowFields(uint32_t manager, const char* when);

}  // namespace uagfx::win
