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

}  // namespace uagfx::weather
