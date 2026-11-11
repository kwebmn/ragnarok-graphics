#pragma once
#include <string>

#include "core/settings/strings.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win {

// 3.0.18-beta.167 (step 3, the shared layer): a text of core/settings/strings in the player's language - 55 modules
// had their own copy of this line
inline std::string say(settings::Text t) { return settings::tr(language(), t); }

}  // namespace uagfx::win
