#pragma once
#include "core/image.hpp"

namespace uagfx::win {

// AI textures (0.13): which cell of uaRO.exe's sprite cache the game is writing a frame into (core/sprite_exe.hpp),
// so that the 64-bit renderer can enlarge the frame on its own. installSpriteHook() sends that one call through
// spriteWrite below; false, with a log line, when the exe is another build.
bool installSpriteHook();
// While the game writes a frame: the cell (in the texture's texels); false at any other time.
bool spriteCell(Rect& cell);

}  // namespace uagfx::win
