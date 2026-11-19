#pragma once
#include <cstdint>
#include <string>

namespace uagfx::weather {

// 0.39, the driver's own weather (win/gpu/weather.cpp) - the server sends none (its maps have no weather flags but a
// festival's fireworks): snow over Lutie, leaves falling in Payon's woods, sand in the wind about Morroc, a fine rain
// over Niflheim and Einbroch. Under the open sky only.
enum class Kind : uint8_t { None, Snow, Rain, Leaves, Sand };

Kind of(const std::string& map);

// 2.6.22, [effects] weather_kind (by the Clock and the Season): the weather all that goes by it takes - the map's own
// (auto, the table above), clear skies, or one of the four of the player's choice under every open sky; never under a
// roof (in the order of the file's words).
enum Choice { kWeatherAuto, kWeatherClear, kWeatherRain, kWeatherSnow, kWeatherLeaves, kWeatherSand, kWeathers };
Kind chosen(int choice, const std::string& map);

// 2.7.40: whether a draw of the game's effect `effectId` is not drawn - its own snow (162: uaRO.exe's EF_SNOW, which a
// map's snow sets going) where ours falls (`sky` the weather drawn, `shown` «Опади» on): two snows fell over Lutie
bool hidesGameEffect(Kind sky, bool shown, int effectId);

// 3.0.5-beta.19 (the user: «делай» - the pollen 2.37 ms of a Radeon 610M's frame for a full-screen pass): weather_vs's
// cells, a quad each over its particle - each layer's cells across and all of them over a picture of width x height
// game pixels: the kind's (shaders.cpp weatherLayer: 1 snow ... 5 pollen, 6 dust) cell side times the layer's depth,
// two more each way for the layer's shift; none for a picture of no size
// 3.0.22-beta.23 (the owner 19.11 «возьми» - another server's «Sandstorm»): [effects] sandstorm - over the sand in the
// wind (Morroc's, or the player's own choice of it) a storm now and then: one in each kStormEvery, at the same moment
// on every computer by the clock, rising over kStormRise, whole kStormHold, gone over kStormRise - the air thick with
// sand, the sight cut short. How much of a storm at `unixSeconds` (the wall clock, 0-1, smooth)
constexpr int kStormEvery = 40 * 60, kStormRise = 10, kStormHold = 30;  // seconds
float stormAt(double unixSeconds);
struct QuadCells {
    uint32_t across[3] = {0, 0, 0};
    uint32_t count[3] = {0, 0, 0};
};
QuadCells quadCells(int kind, float width, float height);

}  // namespace uagfx::weather
