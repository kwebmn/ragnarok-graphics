#pragma once
#include <cstdint>

namespace uagfx::win {

// 2.5.18: the textures in their pictures' own colours (core/full_colour.hpp): uaRO.exe's CTexture::Update
// (core/texture_exe.hpp) through a hook of its vtable entry - after the game's own 16-bit write the picture's texels go
// to the texture's surface too. false, with a log line, when the exe is another build.
bool installTextureHook();
// [quality] full_colour: off - the 16-bit texels alone, from the next texture the game writes.
void setFullColour(bool on);
uint32_t fullColourTextures();  // the writes kept in full colour so far

}  // namespace uagfx::win
