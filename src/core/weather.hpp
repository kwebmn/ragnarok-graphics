#pragma once
#include <cstdint>
#include <string>

namespace uagfx::weather {

// 0.39, the driver's own weather (win/gpu/weather.cpp) - the server sends none (its maps have no weather flags but a
// festival's fireworks): snow over Lutie, leaves falling in Payon's woods, sand in the wind about Morroc, a fine rain
// over Niflheim and Einbroch. Under the open sky only.
enum class Kind : uint8_t { None, Snow, Rain, Leaves, Sand };

Kind of(const std::string& map);

}  // namespace uagfx::weather
