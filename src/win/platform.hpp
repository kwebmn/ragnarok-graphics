#pragma once
#include "core/platform.hpp"

namespace uagfx::win {

// 3.0.4-beta.8: what runs the game (core/platform.hpp) - Wine by ntdll's wine_get_version, Steam's Proton and the
// Steam Deck by the environment Steam gives a game; found once
const platform::Facts& platformFacts();

}  // namespace uagfx::win
