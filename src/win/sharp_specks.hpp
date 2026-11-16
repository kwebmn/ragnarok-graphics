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
// 3.0.20-beta.68 (the owner's item 7, the buffs «как в игре, только чётко»; core/sharp_art): a painted picture - the
// pixel art the game keys out of magenta: «AGI UP!», Gloria's nun, the angels, their wings - gets a copy four times
// larger by xBR
void note(uint32_t texture, const uint32_t* argb, int w, int h, unsigned effect);
// A skill's draw adding light from a texture with a copy: drawn from the copy (its texture in cmd, sharpCopy; filtered
// smoothly through its levels); `look` told once, in the first draw's line. False - as the game draws it.
// A painted picture's draw, however it blends: the copy's texture beside the game's own (cmd.sharpArt) - the renderer
// draws the pack's AI picture of the game's texture when it has one, else the copy.
bool swap(DrawCmd& cmd, DrawSetup& setup, const char* look);
// 3.0.20-beta.68: a painted picture's copy is made a few rows a frame (core/sharp_art CopyJob: whole it is up to
// 23 ms of the game's thread) - called with each skill's draw, it works on the copy in hand once a frame and sends it
// to the card when done
void work();
// The 30-second line's part ("; the skills' specks: ..."); "" until a sheet or a painted picture was seen
std::string stats();

}  // namespace uagfx::win::sharpspecks
