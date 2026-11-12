#pragma once
#include <cstdint>
#include <string>

#include "core/frame.hpp"
#include "core/states.hpp"

namespace uagfx::win::sharpspecks {

// 3.0.18-beta.181 ([effects] crisp_storm_gust, core/sharp_specks): Storm Gust's snow drawn from copies of its own
// sheets at real resolution - a sheet noted as a draw of it comes (its texels as the game last wrote them), its copy
// made and sent to the card then; the rest of Storm Gust (its cloud, whirl, lightning, ghost, ice) as the game draws
// it. Device7::record, the game's thread.
constexpr uint16_t kStormGust = 89;  // its effect's number (effect_exe::Tag::id; R-8X3RCC's record)

// Whether the texture of a Storm Gust draw is to be looked at: not seen yet, or written since (`changed`)
bool wanted(uint32_t texture, bool changed);
// Its top level's texels (ARGB, w x h): a sheet of snow gets its copy (once a picture), anything else is left as it is
void note(uint32_t texture, const uint32_t* argb, int w, int h);
// A draw of Storm Gust's adding light from a texture with a copy: drawn from the copy (its texture in cmd, sharpCopy;
// filtered smoothly through its levels). False - as the game draws it.
bool swap(DrawCmd& cmd, DrawSetup& setup, const char* look);  // look: told once, in the first draw's line
// The 30-second line's part ("; storm gust's snow: ..."); "" until a sheet was seen
std::string stats();

}  // namespace uagfx::win::sharpspecks
