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

// 2.2.2: the passes the Direct3D 12 renderer times on its own, in its order (win/gpu/impl.hpp EffectTimer): the key
// Costs and the switches know them by, and the log's words. One table: four lists of them were kept by hand.
struct TimedPass {
    const char* key;
    const char* words;
};
inline constexpr TimedPass kTimedPasses[] = {
    {"ao", "soft shadows"}, {"clouds", "clouds"}, {"actor_shadows", "characters' shadows"},
    {"lights", "lights and torches"}, {"haze", "haze"}, {"heat", "heat haze"}, {"grade", "colour"},
    {"dof", "depth of field"}, {"weather", "weather"}, {"glow", "glow"}, {"water", "water's copies"},
    {"sky", "sky"}, {"shafts", "light in the air"}, {"floor", "floor reflections"}, {"bloom", "bloom"},
    {"grass", "grass"}, {"new_trees", "new trees"}};

// "name ms" a line; parse() skips what does not read.
std::string format(const Costs& c);
Costs parse(const std::string& text);

// The ms the effect behind an [effects] switch costs (its pass's), or below 0 when not measured.
double costOf(const Costs& c, const std::string& key);
// 2.2.1: its timed pass's name, "" when it has none of its own (a group's sum takes each pass once)
std::string passOf(const std::string& key);

// [frames] keep_fps: the switch to turn off next - on, not turned off already: the costliest measured one (over
// 0.05 ms); with nothing measured, the first of a list from the usually costliest down (with d3d11, of those Direct3D
// 11 draws). "" when none is left.
std::string pickToDrop(const EffectSettings& e, const Costs& c, const std::set<std::string>& dropped,
                       bool d3d11 = false);
void drop(EffectSettings& e, const std::string& key);  // its switch off
// 2.2.2, keep_fps: whether a five-second window was too slow because of the graphics card - under 90 % of the frames
// a second wanted (`target`), and the card's own work a frame (`gpuMs`, below 0 when not measured: the frames alone
// decide) over 85 % of a frame's time at that rate. A game slow for the processor's part (a crowd, a town) keeps its
// effects: turning them off would not help.
bool slowForCard(double fps, int target, double gpuMs);

}  // namespace uagfx::effect_costs
