#pragma once
#include <cstdint>
#include <string>

#include "core/frame.hpp"
#include "core/states.hpp"

namespace uagfx::win::sharpspecks {

// 3.0.18-beta.181 (Storm Gust), 3.0.18-beta.184 (every skill: the owner's «сделай все без остановки»; EffectSettings::
// sharpSpecks, core/sharp_specks): the skills' sheets of specks - the snow, the sparkles - drawn from copies made at
// real resolution: a texture noted as a skill's draw comes (its texels as the game last wrote them), a sheet's copy
// made and sent to the card then; anything else as the game draws it. Device7::record, the game's thread.

// Whether the texture of a skill's draw is to be looked at: not seen yet, or written since (`changed`); one new texture
// a frame at most (the others the next frames)
bool wanted(uint32_t texture, bool changed);
// Its top level's texels (ARGB, w x h) of the skill effect `effect`: a sheet of specks gets its copy (once a picture)
void note(uint32_t texture, const uint32_t* argb, int w, int h, unsigned effect);
// A skill's draw adding light from a texture with a copy: drawn from the copy (its texture in cmd, sharpCopy; filtered
// smoothly through its levels); `look` told once, in the first draw's line. False - as the game draws it.
bool swap(DrawCmd& cmd, DrawSetup& setup, const char* look);
// The 30-second line's part ("; the skills' specks: ..."); "" until a sheet was seen
std::string stats();

}  // namespace uagfx::win::sharpspecks
