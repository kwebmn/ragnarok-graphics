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

}  // namespace uagfx::weather
