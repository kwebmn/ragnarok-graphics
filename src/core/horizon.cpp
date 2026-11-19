#include "core/horizon.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::horizon {
namespace {

constexpr float kTwoPi = 6.28318530718f;

// The default: Prontera's - green hills with forests, rocky mountains far off with snow on the highest
Climate prontera() {
    Climate c;
    c.land = Land::Hills;
    c.seed = 11;
    c.grass = {0.30f, 0.46f, 0.14f};
    c.forest = {0.08f, 0.19f, 0.06f};
    c.rock = {0.45f, 0.41f, 0.36f};
    c.snow = {0.92f, 0.94f, 0.98f};
    c.sand = {0.76f, 0.66f, 0.46f};
    return c;
}

Climate with(Climate c, Land land, uint32_t seed) {
    c.land = land;
    c.seed = seed;
    return c;
}

Climate of(const std::string& region) {
    const Climate base = prontera();
    if (region == "Payon") {  // forested mountains, little snow
        Climate c = with(base, Land::Hills, 13);
        c.mountAmp = 3800.0f;
        c.snowLine = 3400.0f;
        c.treeLine = 2600.0f;
        c.grass = {0.28f, 0.42f, 0.13f};
        c.forest = {0.06f, 0.16f, 0.05f};
        return c;
    }
    if (region == "Geffen") {  // hills and cliffs, cooler rock
        Climate c = with(base, Land::Hills, 17);
        c.mountAmp = 3200.0f;
        c.snowLine = 2600.0f;
        c.rock = {0.42f, 0.42f, 0.44f};
        return c;
    }
    if (region == "Lutie") {  // alps under snow, dark pines on the foothills
        Climate c = with(base, Land::Alps, 23);
        c.mountAmp = 4600.0f;
        c.mountStart = 4200.0f;
        c.mountFull = 11000.0f;
        c.snowLine = 1300.0f;
        c.treeLine = 1100.0f;
        c.grass = {0.78f, 0.82f, 0.86f};
        c.forest = {0.06f, 0.14f, 0.07f};
        return c;
    }
    if (region == "Louyang") {  // karst pillars in green
        Climate c = with(base, Land::Karst, 29);
        c.mountAmp = 2600.0f;
        c.mountFreq = 1.0f / 1800.0f;
        c.snowLine = 99999.0f;
        c.grass = {0.26f, 0.44f, 0.16f};
        c.haze = 3.4e-5f;
        return c;
    }
    if (region == "Amatsu") {  // steep green hills, a snowy cone or two
        Climate c = with(base, Land::Hills, 31);
        c.mountAmp = 3600.0f;
        c.mountFreq = 1.0f / 6000.0f;
        c.snowLine = 2800.0f;
        return c;
    }
    if (region == "Morroc") {  // dunes near, mesas far
        Climate c = with(base, Land::Desert, 37);
        c.hillAmp = 140.0f;
        c.mountAmp = 1500.0f;
        c.mountFreq = 1.0f / 5200.0f;
        c.mountFull = 7000.0f;
        c.snowLine = 99999.0f;
        c.haze = 4.0e-5f;
        c.sand = {0.80f, 0.62f, 0.38f};
        c.rock = {0.62f, 0.36f, 0.22f};
        return c;
    }
    if (region == "Kunlun") {  // misty peaks
        Climate c = with(base, Land::Karst, 41);
        c.mountAmp = 3400.0f;
        c.mountFreq = 1.0f / 2400.0f;
        c.snowLine = 99999.0f;
        c.haze = 4.4e-5f;
        return c;
    }
    if (region == "Ayothaya" || region == "Umbala") {  // jungle hills in a humid haze
        Climate c = with(base, Land::Hills, region == "Ayothaya" ? 43u : 47u);
        c.mountAmp = 2200.0f;
        c.snowLine = 99999.0f;
        c.treeLine = 9999.0f;
        c.grass = {0.20f, 0.40f, 0.10f};
        c.forest = {0.05f, 0.15f, 0.04f};
        c.haze = 4.2e-5f;
        return c;
    }
    if (region == "Alberta" || region == "Comodo") {  // the sea with rocky islands
        Climate c = with(base, Land::Sea, region == "Alberta" ? 51u : 53u);
        c.mountAmp = region == "Alberta" ? 1400.0f : 1100.0f;
        c.mountFreq = 1.0f / 6500.0f;
        c.snowLine = 99999.0f;
        c.haze = 2.8e-5f;
        if (region == "Comodo") c.grass = {0.24f, 0.48f, 0.16f};
        return c;
    }
    if (region == "Hugel") {  // gentle rolling hills
        Climate c = with(base, Land::Hills, 59);
        c.mountAmp = 1800.0f;
        c.snowLine = 99999.0f;
        c.grass = {0.34f, 0.50f, 0.16f};
        return c;
    }
    if (region == "Einbroch") {  // grey hills in smog
        Climate c = with(base, Land::Waste, 61);
        c.mountAmp = 2000.0f;
        c.haze = 6.0e-5f;
        c.grass = {0.36f, 0.36f, 0.30f};
        c.rock = {0.38f, 0.35f, 0.32f};
        return c;
    }
    if (region == "Lighthalzen") {  // a rocky plateau
        Climate c = with(base, Land::Hills, 67);
        c.mountAmp = 2600.0f;
        c.snowLine = 3000.0f;
        c.grass = {0.32f, 0.40f, 0.20f};
        c.haze = 3.6e-5f;
        return c;
    }
    if (region == "Juno") {  // high rocky mountains round the plateau
        Climate c = with(base, Land::Alps, 71);
        c.mountAmp = 4200.0f;
        c.snowLine = 2200.0f;
        return c;
    }
    if (region == "Rachel" || region == "Veins") {  // canyons in red rock, Rachel's peaks with snow
        Climate c = with(base, Land::Canyon, region == "Rachel" ? 73u : 79u);
        c.mountAmp = region == "Rachel" ? 2800.0f : 2000.0f;
        c.mountFreq = 1.0f / 4800.0f;
        c.snowLine = region == "Rachel" ? 2200.0f : 99999.0f;
        c.sand = {0.74f, 0.52f, 0.34f};
        c.rock = {0.58f, 0.32f, 0.20f};
        c.haze = 3.6e-5f;
        return c;
    }
    if (region == "Niflheim" || region == "Glast Heim") {  // dead grey ridges in fog
        Climate c = with(base, Land::Waste, region == "Niflheim" ? 83u : 89u);
        c.mountAmp = region == "Niflheim" ? 2400.0f : 2800.0f;
        c.snowLine = 99999.0f;
        c.grass = {0.30f, 0.30f, 0.27f};
        c.forest = {0.12f, 0.12f, 0.11f};
        c.rock = {0.34f, 0.33f, 0.33f};
        c.haze = region == "Niflheim" ? 7.0e-5f : 5.0e-5f;
        return c;
    }
    return base;  // Prontera and any region not named
}

}  // namespace

Climate climateOf(const std::string& region) { return of(region); }

std::string climateRegion(const std::string& map, const std::string& lookRegion) {
    if (!lookRegion.empty()) return lookRegion;
    if (map == "jawaii") return "Comodo";  // a tropical island in the sea
    return lookRegion;
}

Level levelForCard(int card) {
    if (card <= 1) return Level{2048, 192, 64, 0, 24};
    return card == 2 ? Level{4096, 384, 128, 24, 64} : Level{8192, 768, 128, 24, 96};
}

float uOf(float x, float z) {
    float u = std::atan2(z, x) / kTwoPi;
    if (u < 0.0f) u += 1.0f;
    return u >= 1.0f ? 0.0f : u;
}

float riseOfRow(float v) { return kRiseHigh - v * (kRiseHigh - kRiseLow); }

float rowOfRise(float rise) { return (kRiseHigh - rise) / (kRiseHigh - kRiseLow); }

bool wanted(bool on, bool wideView, bool outdoors, bool hung) { return on && wideView && outdoors && !hung; }

bool Key::operator==(const Key& o) const {
    if (region != o.region || weak != o.weak || std::fabs(mapRadius - o.mapRadius) >= 1.0f) return false;
    for (int k = 0; k < 3; ++k)
        if (std::fabs(sun[k] - o.sun[k]) >= 1e-3f) return false;
    return true;
}

void pack(const Climate& c, float mapRadius, const float sun[3], const Level& level, int row0, int row1,
          float out[kPackedFloats]) {
    for (int k = 0; k < kPackedFloats; ++k) out[k] = 0.0f;
    const float head[12] = {float(int(c.land)), float(c.seed), mapRadius, c.hillAmp, c.hillFreq, c.mountAmp,
                            c.mountFreq, c.mountStart, c.mountFull, c.snowLine, c.treeLine, c.haze};
    for (int k = 0; k < 12; ++k) out[k] = head[k];
    const auto rgb = [&](int at, const Rgb& v) {
        out[at] = v.r;
        out[at + 1] = v.g;
        out[at + 2] = v.b;
    };
    rgb(12, c.grass);
    rgb(15, c.forest);
    out[18] = float(level.steps);
    out[19] = float(level.shadowSteps);
    rgb(20, c.rock);
    out[23] = kEyeHeight;
    rgb(24, c.snow);
    for (int k = 0; k < 3; ++k) out[28 + k] = sun[k];
    out[32] = float(level.width);
    out[33] = float(level.height);
    out[36] = float(row0);
    out[37] = float(row1);
    rgb(38, c.sand);
}

Slice step(Bake& b, const Key& key, const Level& level, uint64_t now) {
    Slice s;
    const bool sameLevel = b.level.width == level.width && b.level.height == level.height &&
                           b.level.steps == level.steps && b.level.shadowSteps == level.shadowSteps &&
                           b.level.slices == level.slices;
    const bool sameLand = b.key.region == key.region && b.key.weak == key.weak &&
                          std::fabs(b.key.mapRadius - key.mapRadius) < 1.0f;
    float moved = 0.0f;
    for (int k = 0; k < 3; ++k) moved += (b.key.sun[k] - key.sun[k]) * (b.key.sun[k] - key.sun[k]);
    if (!b.started || !sameLand || !sameLevel) {
        b = Bake{};
        b.key = key;
        b.level = level;
        b.started = true;
        b.startedAt = now;
        s.restart = true;
    } else if (b.ready && !b.relighting && level.shadowSteps > 0 && moved > kRelightAngle * kRelightAngle) {
        // 3.0.22-beta.15: the sun moved on - its shadows drawn again over the strip as it stands, shown meanwhile
        for (int k = 0; k < 3; ++k) b.key.sun[k] = key.sun[k];
        b.rowsDone = 0;
        b.relighting = true;
    }
    if (b.rowsDone >= level.height || level.height <= 0 || level.slices <= 0) return s;
    const int per = (level.height + level.slices - 1) / level.slices;
    s.row0 = b.rowsDone;
    s.row1 = std::min(level.height, b.rowsDone + per);
    b.rowsDone = s.row1;
    s.relight = b.relighting;
    if (!b.relighting) ++b.frames;
    if (b.rowsDone >= level.height) {
        s.last = true;
        if (b.relighting) {
            b.relighting = false;
        } else {
            b.ready = true;
            b.readyAt = now;
        }
    }
    return s;
}

float fadeOf(const Bake& b, uint64_t now) {
    if (!b.ready) return 0.0f;
    return now <= b.readyAt ? 0.0f : std::min(1.0f, float(now - b.readyAt) / float(kFadeMs));
}

}  // namespace uagfx::horizon
