#pragma once

namespace uagfx::win::consolehint {

// 3.0.17-beta.11 (the concept «простой старт»: «В игре одна подсказка на первой карте: Alt+D — швидкі
// налаштування»): once a computer, 5 s into the first map, a line in the game's chat (its own push, as the hotbar's
// battle mode line); uaro-graphics\cache\console-hint.txt then says it was shown. On the game's thread.
void frame(unsigned codePage);

}  // namespace uagfx::win::consolehint
