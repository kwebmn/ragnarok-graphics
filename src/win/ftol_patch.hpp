#pragma once

namespace uagfx::win {

// 2.5.20: uaRO.exe's __ftol by fisttp where the processor has SSE3 (core/ftol_exe.hpp). installFastFtol(false)
// ([debug] fast_ftol = 0) leaves the game's own; false, with a log line, when the exe is another build.
bool installFastFtol(bool on);

}  // namespace uagfx::win
