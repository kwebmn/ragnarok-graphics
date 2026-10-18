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

float floorShine(const std::string& map) {
    if (map.empty() || outdoors(map)) return 0.0f;
    static const char* const kSmooth[] = {
        "prt_castle", "prt_church", "prtg_cas", "payg_cas", "gefg_cas", "aldeg_cas", "schg_cas", "arug_cas",
        "nguild_",    "gl_cas",     "gl_church", "gl_knt",  "gl_in",    "c_tower",   "alde_dun", "tha_t",
        "lhz_dun",    "lhz_in",     "kh_dun",   "kh_kiehl", "kh_rossi", "kh_vila",   "ra_temple", "ra_temin",
        "ra_temsky",  "ra_san",     "ice_dun",  "xmas_dun", "jupe_core", "juperos_"};
    for (const char* k : kSmooth)
        if (map.compare(0, std::char_traits<char>::length(k), k) == 0) return 1.0f;
    if (map.find("dun") != std::string::npos) return 0.0f;  // the caves, the mines, the sewers, the tombs
    if (map.find("_in") != std::string::npos || map.find("room") != std::string::npos ||
        map.compare(0, 4, "job_") == 0)
        return 0.5f;  // houses: wooden and tiled floors, a faint gloss
    return 0.0f;
}

bool desert(const std::string& map) {
    return map == "morocc" || map == "moc_ruins" || map.compare(0, 8, "moc_fild") == 0;
}

}  // namespace uagfx
