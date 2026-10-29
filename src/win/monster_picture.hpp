#pragma once
#include <string>

#include "core/reference_index.hpp"
#include "core/window_panel.hpp"

namespace uagfx::win {

// 3.0.11-beta.14 (taken out of win/reference_window in 3.0.11-beta.38 for the target's card): a monster's standing
// frame (its first action's first frame) from the client's own sprites - data\sprite\몬스터 or npc by the names
// refindex::spriteNames gives it, read as the game reads its files - fitted into `box` x `box`; false: none found.
// `from` gets the sprite's name. On the game's thread.
bool monsterPicture(const refindex::Mob& mob, int box, panel::Picture& out, std::string* from = nullptr);

}  // namespace uagfx::win
