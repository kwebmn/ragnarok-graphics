#pragma once
#include <windows.h>

#include <cstdint>

namespace uagfx::win {

// 3.0.11-beta.94 (R-3S33MK «чтото лагает игра»): the game's message loop's beats - uaRO.exe's import of PeekMessageA
// counted on the game's thread (it calls it each pass of its loop, in front of other windows and behind them), for
// the profiler's thread to see the game's thread stand still (core/stall_watch, win/profiler). In DllMain, after
// ROExt's hook (it chains to it).
void installStallWatch();
void setStallThread(DWORD gameThread);  // the game's thread: only its calls beat
bool stallBeats(uint32_t& beats);  // false: not hooked (the game has no such import) - nothing to watch

}  // namespace uagfx::win
