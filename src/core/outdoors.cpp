#include "core/outdoors.hpp"

namespace uagfx {

bool outdoors(const std::string& map) {
    if (map.empty()) return false;
    static const char* const kIndoors[] = {"_in", "dun", "@", "cas", "gld", "tower", "pvp", "arena", "que_", "job",
                                           "evt", "sec_", "bat_", "guild", "room", "prison", "maze", "campin"};
    for (const char* k : kIndoors)
        if (map.find(k) != std::string::npos) return false;
    if (map.find("fild") != std::string::npos) return true;
    static const char* const kTowns[] = {"prontera", "geffen", "payon", "morocc", "alberta", "izlude", "aldebaran",
                                         "comodo", "umbala", "yuno", "amatsu", "gonryun", "louyang", "ayothaya",
                                         "einbroch", "einbech", "lighthalzen", "hugel", "rachel", "veins", "xmas",
                                         "niflheim", "moscovia", "brasilis", "dewata", "malangdo", "malaya", "eclage",
                                         "splendide", "manuk", "mora", "dicastes01", "lasagna", "rockridge",
                                         "harboro1", "pay_arche", "mid_camp"};
    for (const char* t : kTowns)
        if (map == t) return true;
    return false;
}

}  // namespace uagfx
