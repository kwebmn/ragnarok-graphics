#pragma once
#include "core/homun_ai.hpp"

namespace uagfx::win::homunaifiles {

// 3.0.11-beta.290 (the owner's feature 22): [interface] homun_ai - our AI written into the game's AI\USER_AI (where no
// AI is or ours is, never over another one) and its choices into uaro_config.lua; the game takes it after /hoai and
// /merai. Called each frame, writes only when the choices changed (and once at the start)
void apply(bool on, const homunai::Choices& c);

}  // namespace uagfx::win::homunaifiles
