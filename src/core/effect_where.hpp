#pragma once
#include <string>

#include "core/config.hpp"

namespace uagfx::effect_where {

// 2.6.22, the console's «Только на этой карте»: whether an effect can show anything on a map - by what the map's name
// tells (under the open sky or not, desert, snow, a damp cave, the water's kind, the weather chosen, a region of its
// own); false only when it surely cannot. The map's file is not read, so what stands on it - torches, smoke,
// fireflies, lamps, trees, water - is always listed. A map not known yet: everything.
bool worksOn(const std::string& sectionKey, const std::string& map, const EffectSettings& e);

}  // namespace uagfx::effect_where
