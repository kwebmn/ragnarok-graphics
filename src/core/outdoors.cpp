#include "core/outdoors.hpp"

#include <string>

namespace uagfx {

bool outdoors(const std::string& map) {
    if (map.empty()) return false;
    // 3.0.11-beta.299 (the owner: «карты где травы нету, например нубзона, мжолниры» - «плоской карта быть не должна»;
    // the census of 921 maps, artifact XH2FtVBnnhGnRDEheuuxV1): green open maps the name rule took for a roof - no
    // grass, flowers, sun or sky there. Before the indoor words (izlu2dun, mosk_dun01 are under the sky)
    static const char* const kOpenPrefixes[] = {"mjolnir_", "new_zone0"};
    for (const char* p : kOpenPrefixes)
        if (map.compare(0, std::char_traits<char>::length(p), p) == 0) return true;
    if (map.size() == 7 && map.compare(0, 4, "new_") == 0 && map[5] == '-' && (map[6] == '2' || map[6] == '3'))
        return true;  // the novice grounds new_N-2, new_N-3 (N-1 and N-4 are the school's rooms)
    static const char* const kOpen[] = {
        "alb2trea", "glast_01",  "jawaii",    "izlu2dun",  "arug_que01", "mosk_dun01", "mosk_dun02", "mosk_dun03",
        "pay_gld",  "alde_gld",  "prt_gld",   "sch_gld",   "te_alde_gld", "te_prt_gld", "odin_tem01",
        "odin_tem02", "odin_tem03", "prt_monk", "job_monk", "tur_dun03", "yggdrasil01", "pvp_y_1-3", "pvp_y_2-3",
        "pvp_y_3-3", "pvp_y_4-3", "pvp_y_5-3", "pvp_y_6-3", "pvp_y_7-3", "pvp_y_8-3", "pvp_flo", "poring_w01",
        "poring_w02", "p_track01", "p_track02"};
    for (const char* o : kOpen)
        if (map == o) return true;
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

bool castleGrounds(const std::string& map) {
    static const char* const kCastles[] = {"prtg_cas", "payg_cas", "gefg_cas", "aldeg_cas", "schg_cas", "arug_cas",
                                           "nguild_"};
    for (const char* c : kCastles)
        if (map.compare(0, std::char_traits<char>::length(c), c) == 0) return true;
    return false;
}

bool grassGrows(const std::string& map) { return outdoors(map) || castleGrounds(map); }

bool desert(const std::string& map) {
    return map == "morocc" || map == "moc_ruins" || map.compare(0, 8, "moc_fild") == 0;
}

}  // namespace uagfx
