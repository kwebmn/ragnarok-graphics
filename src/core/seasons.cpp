#include "core/seasons.hpp"

#include <algorithm>

#include "core/trees.hpp"

namespace uagfx::seasons {

Look of(const std::string& map, int month, bool outdoors, const living::Place& place) {
    Look l;
    if (!outdoors || place.cave || place.underwater || place.sand) return l;
    if (place.snow) {  // snow all year: on the roofs and the tops too
        l.snow = 0.7f;
        return l;
    }
    if (trees::evergreen(map) || place.flowers == 3) return l;  // no seasons in the tropics
    switch (month) {
    case 12: l.snow = 0.45f, l.dry = 0.3f; break;
    case 1: l.snow = 0.7f, l.dry = 0.3f; break;
    case 2: l.snow = 0.5f, l.dry = 0.3f; break;
    case 3: l.lush = 0.3f; break;
    case 4: l.lush = 0.6f; break;
    case 5: l.lush = 0.45f; break;
    case 9: l.dry = 0.15f; break;
    case 10: l.dry = 0.35f; break;
    case 11: l.dry = 0.5f; break;
    default: break;
    }
    return l;
}

living::Place withYear(living::Place place, const Look& year) {
    if (year.snow > 0.0f) {
        place.insects = false;
        place.flowers = -1;  // 2.7.6: and no flowers in the grass under the snow (R-NV24QA)
    }
    return place;
}

namespace {
float toward(float from, float to, float step) {
    return from < to ? std::min(to, from + step) : std::max(to, from - step);
}
}  // namespace

Look ease(const Look& shown, const Look& target, float seconds, float fadeSeconds) {
    if (!(fadeSeconds > 0.0f)) return target;
    const float step = std::max(seconds, 0.0f) / fadeSeconds;
    Look l;
    l.snow = toward(shown.snow, target.snow, step);
    l.dry = toward(shown.dry, target.dry, step);
    l.lush = toward(shown.lush, target.lush, step);
    return l;
}

bool same(const Look& a, const Look& b) { return a.snow == b.snow && a.dry == b.dry && a.lush == b.lush; }

int monthFor(int choice, int localMonth) {
    static const int kMonths[kSeasons] = {0, 4, 7, 10, 1};  // April, July, October, January
    return choice > kSeasonAuto && choice < kSeasons ? kMonths[choice] : localMonth;
}

}  // namespace uagfx::seasons
