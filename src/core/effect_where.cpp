#include "core/effect_where.hpp"

#include "core/living.hpp"
#include "core/outdoors.hpp"
#include "core/region_look.hpp"
#include "core/seasons.hpp"
#include "core/weather.hpp"

namespace uagfx::effect_where {
namespace {

// The year shows on it in some season: snow, dry or greener ground (seasons::of), whichever season is chosen now
bool hasSeasons(const std::string& map, bool open, const living::Place& place) {
    for (const int month : {1, 4, 10}) {
        const seasons::Look l = seasons::of(map, month, open, place);
        if (l.snow > 0.0f || l.dry > 0.0f || l.lush > 0.0f) return true;
    }
    return false;
}

}  // namespace

bool worksOn(const std::string& sectionKey, const std::string& map, const EffectSettings& e) {
    if (map.empty()) return true;
    const std::string k = sectionKey.compare(0, 8, "effects.") == 0 ? sectionKey.substr(8) : sectionKey;
    const bool open = outdoors(map);
    const living::Place place = living::placeOf(map, open, false);
    const weather::Kind sky = weather::chosen(e.weatherKind, map);
    if (k == "weather") return sky != weather::Kind::None;
    if (k == "storms" || k == "wet_ground" || k == "rain_wet") return sky == weather::Kind::Rain;
    if (k == "floor_reflections") return sky == weather::Kind::Rain || (!open && floorShine(map) > 0.0f);
    if (k == "sand_drift") return place.sand;
    if (k == "footprints") return sky == weather::Kind::Snow || desert(map) || open;  // snow, sand, dust outdoors
    if (k == "wet_walls" || k == "cave_drips") return place.cave;
    if (k == "lit_dust") return !open;
    if (k == "underwater") return place.underwater;
    if (k == "swamp") return place.swamp;
    if (k == "lava") return map == "mag_dun01" || map == "mag_dun02" || map == "tak_run";  // its maps (0.31)
    if (k == "butterflies" || k == "birds" || k == "night_fireflies") return place.wildlife;
    if (k == "flowers") return place.flowers >= 0;
    if (k == "grass" || k == "dew") return open;
    if (k == "ground_seams" || k == "no_game_fog") return open;
    if (k == "morning_fog") return open && !desert(map);
    if (k == "region_looks") return region_look::of(map, 100).region[0] != 0;
    if (k == "seasons") return hasSeasons(map, open, place);
    return true;
}

}  // namespace uagfx::effect_where
