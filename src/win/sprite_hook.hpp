#pragma once
#include "core/image.hpp"

namespace uagfx::win {

// AI textures (0.13): which cell of uaRO.exe's sprite cache the game is writing a frame into (core/sprite_exe.hpp),
// so that the 64-bit renderer can enlarge the frame on its own, and (0.16) which sprite file the frame is of - its
// folder tells monsters, players, NPCs and items apart. installSpriteHook() sends the write's one call and the sprite
// loader's vtable entry through hooks of its own; false, with a log line, when the exe is another build.
bool installSpriteHook();
// While the game writes a frame: the cell (in the texture's texels) and what the frame is of (sprite_exe::Source);
// false at any other time.
bool spriteCell(Rect& cell, uint8_t& source);
bool spriteCellCard();  // 0.95: ... and it is a card lying on the ground

}  // namespace uagfx::win
