#pragma once
#include <string>

#include "core/reference_index.hpp"
#include "core/window_panel.hpp"

namespace uagfx::win {

// 3.0.11-beta.14 (taken out of win/reference_window in 3.0.11-beta.38 for the target's card): a monster's standing
// frame (its first action's first frame) from the client's own sprites - data\sprite\몬스터 or npc by the names
// refindex::spriteNames gives it, read as the game reads its files - fitted into `box` x `box`; false: none found.
// `from` gets the sprite's name. On the game's thread.
// 3.0.19-beta.1: `id` the monster's number - none of its sprites in the client: the site's picture of it kept in the
// driver (core/mob_pictures: the Emperium, the treasure chests...); 3.0.19-beta.2: the game's own name of its sprite
// first (uaRO.exe's job name table: Chonchon's chocho, Verit's vorit, Atroce's atros)
bool monsterPicture(int id, const refindex::Mob& mob, int box, panel::Picture& out, std::string* from = nullptr);

}  // namespace uagfx::win
