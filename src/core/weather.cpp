#include "core/weather.hpp"

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

}  // namespace uagfx::weather
