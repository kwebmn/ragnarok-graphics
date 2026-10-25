#include "core/weather.hpp"

#include <cmath>

#include "core/outdoors.hpp"

namespace uagfx::weather {
namespace {

bool starts(const std::string& s, const char* prefix) {
    return s.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

}  // namespace

Kind of(const std::string& map) {
    if (map == "xmas" || map == "xmas_fild01") return Kind::Snow;  // Lutie: its town and its field
    if (desert(map)) return Kind::Sand;
    if (!outdoors(map)) return Kind::None;
    if (map == "payon" || starts(map, "pay_fild")) return Kind::Leaves;
    if (map == "niflheim" || starts(map, "nif_fild") || map == "einbroch" || starts(map, "ein_fild")) return Kind::Rain;
    return Kind::None;
}

Kind chosen(int choice, const std::string& map) {
    static const Kind kKinds[kWeathers] = {Kind::None, Kind::None, Kind::Rain, Kind::Snow, Kind::Leaves, Kind::Sand};
    if (choice <= kWeatherAuto || choice >= kWeathers) return of(map);
    return outdoors(map) ? kKinds[choice] : Kind::None;
}

bool hidesGameEffect(Kind sky, bool shown, int effectId) { return shown && sky == Kind::Snow && effectId == 162; }

QuadCells quadCells(int kind, float width, float height) {
    QuadCells q;
    if (!(width > 0.0f && height > 0.0f)) return q;
    const float side = kind == 1 ? 36.0f : kind == 2 ? 22.0f : kind == 3 ? 110.0f : kind == 4 ? 60.0f : 80.0f;
    for (int layer = 0; layer < 3; ++layer) {
        const float cell = side * (0.6f + 0.5f * float(layer));
        const uint32_t across = uint32_t(std::ceil(width / cell)) + 2, down = uint32_t(std::ceil(height / cell)) + 2;
        q.across[layer] = across;
        q.count[layer] = across * down;
    }
    return q;
}

}  // namespace uagfx::weather
