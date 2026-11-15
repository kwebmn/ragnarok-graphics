#pragma once
#include <string>

#include "core/frame_pace.hpp"

namespace uagfx::win {

// 3.0.2-beta.5 (the user: the admin's new content as a GRF of his own in a folder of its own): the GRFs in
// <client>\updates\ over the game's archives (core/updates.hpp) - uaRO.exe's archive functions hooked at the driver's
// load, the archives opened at the first lookup. The game's loose data\ files still come first. Nothing when the
// folder holds no GRF, [client] updates is off or the build is another.
void installUpdates();
// Its lines for the driver's log, which opens later (real_ddraw)
void logUpdates();
// 3.0.11-beta.15: a file of the game's as the game itself finds it (its CFileMgr::GetData: updates\ first when
// mounted, then DATA.INI's archives in its order with its own names - zips, GRFs) - the driver's own pictures (the
// reference's sprites and icons). The game's thread only; false: another uaRO.exe build, or no such file.
bool readGameFile(const std::string& name, std::string& out);
// 3.0.19-beta.51 (core/frame_pace): the game's archive reads on the calling thread since its last call - how many,
// their time, the slowest; from now on that thread's reads are the ones counted (the game's, at its present)
void takeFrameReads(framepace::GameFrame& frame);

}  // namespace uagfx::win
