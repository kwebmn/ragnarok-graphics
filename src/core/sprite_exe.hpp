#pragma once
#include <cstdint>

namespace uagfx::sprite_exe {

// AI textures (0.13): uaRO.exe's sprite cache. Every sprite - monsters, players, NPCs, items on the ground - is drawn
// from 256x256 textures cut into cells of one size (32, 64, 128 or 256 a side, by the frame's size); FUN_0040dcb0
// puts a frame into a free or the oldest cell, and its one call of FUN_00417ae0 (CTexture::UpdateSprite, thiscall:
// x, y, the cell's width and height, the image, its palette) writes it: the frame in the cell's top left corner, the
// rest of the cell cleared, through Lock of the whole surface and Unlock.
constexpr uint32_t kWrite = 0x417AE0;
constexpr uint32_t kWriteCall = 0x40E0D8;  // E8 rel32: that call

// Whether these 5 bytes, at kWriteCall, are the expected call of kWrite: else the exe is another build.
bool isWriteCall(const uint8_t bytes[5]);

}  // namespace uagfx::sprite_exe
