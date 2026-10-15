#pragma once
#include <map>
#include <set>
#include <string>

#include "core/config.hpp"

namespace uagfx::effect_costs {

// 0.76: what the effects cost the graphics card. The Direct3D 12 renderer times its passes (the log's "GPU: effects a
// frame" line); the driver writes the last half minute's into uaro-graphics\effect-costs.txt, the settings app shows
// them on the Effects page, and [frames] keep_fps turns the costliest off when the game is too slow.
using Costs = std::map<std::string, double>;  // a pass's name -> ms a frame

// "name ms" a line; parse() skips what does not read.
std::string format(const Costs& c);
Costs parse(const std::string& text);

// The ms the effect behind an [effects] switch costs (its pass's), or below 0 when not measured.
double costOf(const Costs& c, const std::string& key);

// [frames] keep_fps: the switch to turn off next - on, not turned off already: the costliest measured one (over
// 0.05 ms); with nothing measured, the first of a list from the usually costliest down (with d3d11, of those Direct3D
// 11 draws). "" when none is left.
std::string pickToDrop(const EffectSettings& e, const Costs& c, const std::set<std::string>& dropped,
                       bool d3d11 = false);
void drop(EffectSettings& e, const std::string& key);  // its switch off

}  // namespace uagfx::effect_costs
