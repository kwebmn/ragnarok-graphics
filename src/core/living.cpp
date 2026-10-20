#include "core/living.hpp"

#include <algorithm>
#include <ctime>

namespace uagfx::living {
namespace {

bool startsWith(const std::string& s, const char* p) { return s.compare(0, std::char_traits<char>::length(p), p) == 0; }
bool has(const std::string& s, const char* p) { return s.find(p) != std::string::npos; }

template <size_t N>
bool startsWithAny(const std::string& s, const char* const (&list)[N]) {
    for (const char* p : list)
        if (startsWith(s, p)) return true;
    return false;
}

// 0 before a, 1 from b, smooth between.
float ramp(float x, float a, float b) {
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

Place placeOf(const std::string& map, bool outdoors, bool autumn) {
    Place p;
    if (map.empty()) return p;
    static const char* const kSand[] = {"moc_fild", "morocc", "moc_ruins", "ve_fild", "veins", "cmd_fild07"};
    static const char* const kSnow[] = {"xmas", "ice_dun", "ra_fild12"};
    static const char* const kUnder[] = {"iz_dun", "iz_ac2", "dew_dun"};
    static const char* const kDry[] = {"moc_pryd", "alde_dun", "c_tower", "lhz_dun", "thor_v", "mag_dun", "ice_dun",
                                       "moc_", "tha_", "kh_", "jupe"};
    static const char* const kSwamp[] = {"um_fild", "bra_fild", "bra_dun", "dew_fild", "ma_fild"};
    p.sand = startsWithAny(map, kSand);
    p.snow = startsWithAny(map, kSnow);
    p.underwater = startsWithAny(map, kUnder);
    p.cave = !p.underwater && !outdoors && (has(map, "dun") || has(map, "sew") || startsWith(map, "anthell") ||
                                            startsWith(map, "orcsdun") || startsWith(map, "abyss")) &&
             !startsWithAny(map, kDry);
    p.swamp = startsWithAny(map, kSwamp);
    if (outdoors && !p.sand && !p.snow) {
        static const char* const kPayon[] = {"pay", "hu_fild", "gon"};
        static const char* const kAmatsu[] = {"ama", "lou"};
        static const char* const kTropics[] = {"um_", "umbala", "bra", "dew", "mal", "ayo", "cmd", "comodo"};
        p.flowers = startsWithAny(map, kPayon)     ? 1
                    : startsWithAny(map, kAmatsu)  ? 2
                    : startsWithAny(map, kTropics) ? 3
                                                   : 0;
        static const char* const kBare[] = {"nif", "niflheim", "gl_", "glast", "ein", "lhz", "lighthalzen"};
        if (startsWithAny(map, kBare)) p.flowers = -1;
        p.wildlife = p.flowers >= 0;
        p.insects = p.wildlife;  // 2.7.5: seasons::withYear keeps them away in winter
    }
    p.leaves = startsWith(map, "ama") ? 2 : autumn ? 1 : 0;
    return p;
}

int localMinute() {
    const std::time_t now = std::time(nullptr);
    const std::tm* t = std::localtime(&now);
    return t ? t->tm_hour * 60 + t->tm_min : 12 * 60;
}

float morningFog(int minute) {
    const float h = float(minute) / 60.0f;
    return ramp(h, 4.5f, 6.0f) * (1.0f - ramp(h, 8.0f, 10.5f));
}

float dew(int minute) {
    const float h = float(minute) / 60.0f;
    return ramp(h, 5.0f, 6.5f) * (1.0f - ramp(h, 9.0f, 12.0f));
}

float daytime(int minute) {
    const float h = float(minute) / 60.0f;
    return ramp(h, 6.5f, 7.5f) * (1.0f - ramp(h, 18.5f, 19.5f));
}

}  // namespace uagfx::living
