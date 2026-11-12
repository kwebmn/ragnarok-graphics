#pragma once
#include <string>

namespace uagfx::win::lightskillswin {

// 3.0.18-beta.186 ([frames] light_skills, core/light_skills): uaRO.exe's own light skill effects while the frames run
// short of keep_fps in a crowd of skills - its effects' «_min» flag now (0x772D2C, as on a guild war's map) set, and
// back to the player's own /mineffect (0x772D30) when it is wanted no more; the player's setting itself never touched.
// A new map: the game's own value again (it sets it on the way in), wanted or not from the next window on.

// Device7::record: a skill's draw
void drawn();
// Each present: `foreground` false - nothing decided (a window behind others draws few frames). The frame rate it
// was wanted at when the light effects were just turned on (said on the screen), else -1.
int frame(bool on, int keepFps, bool foreground, const std::string& map);

}  // namespace uagfx::win::lightskillswin
