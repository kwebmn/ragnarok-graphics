#include "core/light_mode.hpp"

#include <algorithm>

#include "core/effect_groups.hpp"

namespace uagfx::light {

bool Overrides::operator==(const Overrides& o) const {
    return superLight == o.superLight && battery == o.battery && other == o.other && effectsLevel == o.effectsLevel &&
           aiHidden == o.aiHidden && worldScaleMax == o.worldScaleMax && renderScaleMax == o.renderScaleMax &&
           fpsMax == o.fpsMax && minimisedFps == o.minimisedFps && noSharpen == o.noSharpen &&
           noAnisotropy == o.noAnisotropy && noSupersampling == o.noSupersampling && pacerSleeps == o.pacerSleeps &&
           noUpscaler == o.noUpscaler && flags == o.flags;
}

Overrides overridesOf(const Config& c, bool onBattery, bool otherWindow) {
    Overrides o;
    if (c.batterySaver && onBattery) {
        o.battery = true;
        o.effectsLevel = 1;
        o.worldScaleMax = 75;
        o.fpsMax = std::clamp(c.batteryFps, 15, 60);
        o.minimisedFps = 5;
        o.pacerSleeps = true;
        o.flags = kPlainUpscale;
    }
    if (c.superLight) {
        o.superLight = true;
        o.effectsLevel = 0;
        o.aiHidden = true;
        o.worldScaleMax = 50;
        o.renderScaleMax = 100;
        o.noSharpen = o.noAnisotropy = o.noSupersampling = true;
        o.flags = kFlatWorld | kWorld16 | kPointMips | kPlainUpscale;
    }
    if (c.windowOthersLight && otherWindow) {  // the stricter of each where another mode is on too
        o.other = true;
        o.effectsLevel = 0;
        o.aiHidden = true;
        o.renderScaleMax = std::min(o.renderScaleMax, 50);
        o.noSharpen = o.noSupersampling = o.noUpscaler = true;
        o.fpsMax = o.fpsMax > 0 ? std::min(o.fpsMax, 10) : 10;
    }
    return o;
}

EffectSettings effectsOf(const EffectSettings& effective, const Overrides& o) {
    return o.effectsLevel >= 3 ? effective : effect_groups::atMost(effective, o.effectsLevel);
}

int flatLevel(int width, int height, int levels, bool transparent) {
    if (levels <= 1 || width <= 0 || height <= 0) return 0;
    if (!transparent) return levels - 1;
    int level = 0;
    while (level + 1 < levels && std::min(width >> (level + 1), height >> (level + 1)) >= 16) ++level;
    return level;
}

int fpsLimit(int file, const Overrides& o) {
    if (o.fpsMax <= 0) return file;
    return file > 0 ? std::min(file, o.fpsMax) : o.fpsMax;
}

}  // namespace uagfx::light
