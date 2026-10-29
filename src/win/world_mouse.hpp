#pragma once

namespace uagfx::win {

// 3.0.11-beta.24 (the owner: «курсор мишки поверх интерфейса реагирует
// еще и на мир»): the driver's own parts in the game's picture (the windows' panel, the answer card, the new
// minimap, «Довідник») are windows of the game's for its cursor - the world under them shows no names,
// no sword or talk cursor, and takes no click. uaRO.exe's game mode asks its windows' manager first each frame
// (0x4CCFB0: 0 the world under the cursor, 1 a window, 2 a window the world shows through); over a part of ours
// its answer is 1, and the cursor the arrow a plain window of the game's sets (0x4C9D10). False, with a log line,
// when the exe is another build.
bool installWorldMouse();

}  // namespace uagfx::win
