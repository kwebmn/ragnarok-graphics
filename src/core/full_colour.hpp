#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/frame.hpp"
#include "core/image.hpp"
#include "core/pixel.hpp"

namespace uagfx::full_colour {

// 2.5.18: the game makes every texture 16-bit as it writes it (uaRO.exe's CTexture::Update, FUN_004176c0): A1R5G5B5 -
// a glow's gradient in 32 steps -, A4R4G4B4 - a skill's particle and the effects' 32-bit sprite frames in 16 shades of
// alpha - or R5G6B5. The driver takes the picture's own 8-bit texels as the game hands them over (win/texture_hook.hpp)
// and draws those wherever the 16-bit texels are still made of them.

// Whether f is one of the game's 16-bit texture formats.
bool shortFormat(PixelFormat f);

// Where CTexture::Update puts a picture of w x h texels at (x, y) into a surface of sw x sh: at a lower texture detail
// it takes every step-th texel of the picture - the smallest steps it fits by - and writes them at (x / stepX,
// y / stepY).
struct Write {
    Rect rect;
    int stepX = 1, stepY = 1;
};
Write writeOf(int x, int y, int w, int h, int sw, int sh);
// The picture's texels it writes there (pitch in texels), row by row.
void texelsOf(const uint32_t* picture, int pitch, const Write& at, std::vector<uint32_t>& out);

// The texel to upload for a 16-bit one (raw in f; argb what the driver makes of it, its colour key applied) whose
// picture's texel was full: full's colour - and A4R4G4B4's alpha - when the 16 bits are still full's, else argb (a
// texel keyed away, or written again since).
uint32_t merge(PixelFormat f, uint32_t raw, uint32_t argb, uint32_t full);
// A mip level's texel: the game's (argb, of its own mip made from the 16-bit level 0) tells whether it shows; one that
// does takes the colour of the level made from the full colours (full) - and its alpha in A4R4G4B4.
uint32_t mergeMip(PixelFormat f, uint32_t argb, uint32_t full);
// The next level of a full-colour picture: four texels' average weighted by their alpha (a transparent one lends no
// colour), the alpha their mean.
void halve(const std::vector<uint32_t>& in, int w, int h, std::vector<uint32_t>& out, int& ow, int& oh);
// 3.0.9-beta.3: the game's see-through magenta (red and blue 0xFE or more, green 3 or less) in a decoded picture made
// clear, the colour under it near black (0x00080808), as the game keys its textures (0x0421 in its 16 bits: the AI
// textures know it); true when the picture had any (R-6JBVTG: Izlude's bridge and wall ground textures, 6-8 % of it)
bool keyMagenta(std::vector<uint32_t>& argb);

// What a texel of an upload in full colour was in the 16-bit texture (f): the AI textures know a texture by those.
uint32_t gameTexel(PixelFormat f, uint32_t texel);
void gameView(PixelFormat f, const uint32_t* texels, size_t count, std::vector<uint32_t>& out);
// An upload's texels as the AI textures take them: pixels themselves, or their 16-bit view kept in keep.
const uint32_t* aiTexels(const UploadCmd& u, const uint32_t* pixels, std::vector<uint32_t>& keep);

}  // namespace uagfx::full_colour
