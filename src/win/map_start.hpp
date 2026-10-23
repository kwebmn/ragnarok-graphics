#pragma once

namespace uagfx::win {

// 3.0.2-beta.15 (beta.13 and 14: some map loads slow again and unmeasured - the game's new camera keeps the old one's
// matrix in memory it gets back, and the driver took the loading picture for the world): the game mode's map start
// (CGameMode's vtable 0x69F388, slot 1: 0x559030 - the map's files, its models, the loading picture) hooked, so the
// driver knows when a map is being loaded whatever its camera holds. False, with a log line, in another build.
bool installMapStartHook();
bool mapStarting();     // the game's map start runs now
unsigned mapStarts();  // how many times it began so far

}  // namespace uagfx::win
