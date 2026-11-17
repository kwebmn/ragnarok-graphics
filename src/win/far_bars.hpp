#pragma once

// 3.0.20-beta.84 (R-7WTVBF «когда отдаляю, то полоска хп выглядит огромной. чтото можно придумать чтобы смотрелось
// хорошо»): [interface] far_bars - the bars under the characters (the game's gauges, win/windows_draw) and over the
// monsters (win/mob_hp) smaller with the camera further out than the game's own views (core/gauge_scale)
namespace uagfx::win::farbars {

void setOn(bool on);
// the bars' size now (1 the game's): 1 when off, in the 3D view (its camera is not the game's) or outside the game
float scaleNow();

}  // namespace uagfx::win::farbars
