#pragma once
#include <string>

#include "core/settings/strings.hpp"

namespace uagfx::gamewords {

// 3.0.11-beta.218 (R-S8BERR: dismissing the mercenary asked in broken letters - the game's own Korean question,
// written into uaRO.exe and not its msgstringtable, read in the game's code page): the game's words for a message box
// in the player's language where the driver knows them (`game` in the game's bytes), else empty
std::string known(const std::string& game, settings::Lang l);
// The game's colour marks (^RRGGBB) taken out
std::string withoutColours(const std::string& game);

}  // namespace uagfx::gamewords
