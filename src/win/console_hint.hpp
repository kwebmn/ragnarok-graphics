#pragma once

namespace uagfx::win::consolehint {

// 3.0.17-beta.11 (the concept «простой старт»: «В игре одна подсказка на первой карте: Alt+D — швидкі
// налаштування»): once a computer, 5 s into the first map, a line in the game's chat (its own push, as the hotbar's
// battle mode line); a file of the cache then says it was shown. On the game's thread.
// 3.0.21-beta.7: the settings are «Налаштування»'s (Alt+O), the console the laboratory alone - the line names Alt+O,
// and its file is uaro-graphics\cache\settings-hint.txt: who was told «Alt+D» is told once more
void frame(unsigned codePage);

}  // namespace uagfx::win::consolehint
