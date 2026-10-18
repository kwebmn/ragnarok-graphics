#pragma once
#include <cstdint>

namespace uagfx::texture_exe {

// 2.5.18: uaRO.exe's CTexture. Update (FUN_004176c0, its vtable's second method; thiscall: x, y, the picture's width
// and height, its ARGB texels, the black key - a byte -, its pitch in texels, 0: its width) makes a picture 16-bit into
// the texture's surface and its mip levels again from it (FUN_004175d0). Every texture of the game goes through it but
// a 4 x 4 test at the start (a direct call at 0x4052AB): the world's, the effects' (FUN_0040c590), the sprites'
// 32-bit frames (CSprRes::Load), the windows' tiles.
constexpr uint32_t kVtable = 0x6957C8;
constexpr uint32_t kUpdateSlot = kVtable + 4;
constexpr uint32_t kUpdate = 0x4176C0;
constexpr uint32_t kSurface = 0x0C;  // CTexture: its IDirectDrawSurface7 (crisp_exe::kTextureSurface)
// The windows' pictures into their 256 x 256 tiles (CDCBitmap, FUN_00403a40) - again whenever a window changes, from
// 32-bit pictures crisp text draws itself: its call of Update returns here, and those stay 16-bit.
constexpr uint32_t kWindowTilesReturn = 0x403BBD;

}  // namespace uagfx::texture_exe
