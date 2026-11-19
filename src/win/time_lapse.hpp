#pragma once
#include <windows.h>

namespace uagfx::win::timelapsewin {

// 3.0.22-beta.24 (the owner 19.11 «делай 1-4», 2 - the day going into the night in one take for the channel's
// videos): Ctrl+Shift+T in the game window - the time of day runs fast (core daylight::Lapse), again it stops there,
// again the clock's own; the game sees neither the key nor its letter
bool key(UINT m, WPARAM wp, LPARAM lp);
// Each frame, after the clock: the minute to show (-1: the clock's own). clock: [effects] clock now (a new choice turns
// it off); shown: the clock's minute this frame; perSecond: [effects] timelapse_speed
int frame(int clock, int shown, int perSecond);
// 0 off, 1 running, 2 held (the key hints)
int state();

}  // namespace uagfx::win::timelapsewin
