#pragma once

namespace uagfx::win {

// 3.0.2-beta.5 (the user: the admin's new content as a GRF of his own in a folder of its own): the GRFs in
// <client>\updates\ over the game's archives (core/updates.hpp) - uaRO.exe's archive functions hooked at the driver's
// load, the archives opened at the first lookup. The game's loose data\ files still come first. Nothing when the
// folder holds no GRF, [client] updates is off or the build is another.
void installUpdates();
// Its lines for the driver's log, which opens later (real_ddraw)
void logUpdates();

}  // namespace uagfx::win
