#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

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
    {"ao", "soft shadows"}, {"actor_shadows", "characters' shadows"},
    {"lights", "lights and torches"}, {"mist", "mists"}, {"heat", "heat haze"}, {"grade", "colour"},
    {"weather", "weather"}, {"glow", "glow"}, {"water", "water's copies"},
    {"shafts", "light in the air"}, {"floor", "floor reflections"}, {"bloom", "bloom"},
    {"grass", "grass"}};

// "name ms" a line; parse() skips what does not read.
std::string format(const Costs& c);
Costs parse(const std::string& text);

// The ms the effect behind an [effects] switch costs (its pass's), or below 0 when not measured.
double costOf(const Costs& c, const std::string& key);
// 2.2.1: its timed pass's name, "" when it has none of its own (a group's sum takes each pass once)
std::string passOf(const std::string& key);

// [frames] keep_fps: the switch to turn off next - on, not turned off already: the costliest measured one of a
// millisecond or more; then (2.5.16) what the world's own draws do (the materials, the relief, the characters' light,
// ... - no pass to time); then the costliest measured over 0.05 ms; with nothing measured, the first of a list from
// the usually costliest down. "" when none is left. (2.5.9: Direct3D 11 times its passes too.) 3.0.4-beta.35: the
// grass apart - "grass_thin" first while it is worth a millisecond (or nothing is measured), turned off ("grass") last
std::string pickToDrop(const EffectSettings& e, const Costs& c, const std::set<std::string>& dropped);
void drop(EffectSettings& e, const std::string& key);  // its switch off; "grass_thin": EffectSettings::grassThin
// 3.0.4-beta.11 (R-EQDYNE): the switches keep_fps may turn off that are on in `now` and were off in `was` - turned on
// by hand while the game runs (the console, the settings app): keep_fps leaves them for the rest of the game
std::vector<std::string> turnedOn(const EffectSettings& was, const EffectSettings& now);
// 2.2.2, keep_fps: whether a five-second window was too slow because of the graphics card - under 90 % of the frames
// a second wanted (`target`), and the card's own work a frame (`gpuMs`, below 0 when not measured: the frames alone
// decide) over 85 % of a frame's time at that rate. A game slow for the processor's part (a crowd, a town) keeps its
// effects: turning them off would not help.
bool slowForCard(double fps, int target, double gpuMs);

}  // namespace uagfx::effect_costs
