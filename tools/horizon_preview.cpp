// 3.0.22-beta.11: the horizon's climates as the 3D camera sees them - core/horizon_terrain, the twin of the card's
// bake and of the sky's light, so a climate is tuned here before a beta. Writes for each region:
//   <region>.bmp        by day, the camera toward its tallest land, the sun from the side
//   <region>-dusk.bmp   the same at dusk (the night 0.5), the sun low ahead
//   <region>-strip.bmp  the whole circle as the strip holds it (a quarter of its texels), by day
// Usage: horizon_preview <outdir> [width height]
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "core/horizon.hpp"
#include "core/horizon_terrain.hpp"
#include "core/texel_check.hpp"
#include "core/view3d.hpp"

using namespace uagfx;
using namespace uagfx::horizon;

namespace {

const char* const kRegions[] = {"Alberta", "Amatsu",  "Ayothaya", "Comodo",      "Einbroch", "Geffen", "Glast Heim",
                                "Hugel",   "Juno",    "Kunlun",   "Lighthalzen", "Louyang",  "Lutie",  "Morroc",
                                "Niflheim", "Payon",  "Prontera", "Rachel",      "Umbala",   "Veins"};
constexpr float kPi = 3.14159265f, kMapRadius = 1500.0f;

struct View {
    float night = 0.0f;
    Vec3 toSun;
};

Vec3 unit(float x, float y, float z) {
    const float l = std::sqrt(x * x + y * y + z * z);
    return {x / l, y / l, z / l};
}
Vec3 wayOf(float azimuth, float elevation) {
    return {std::cos(azimuth) * std::cos(elevation), std::sin(elevation), std::sin(azimuth) * std::cos(elevation)};
}
Rgb mixed(const Rgb& a, const Rgb& b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

// The game's sky, roughly (skyAlong: the horizon's colour by the night up to the zenith's, the sun's glow)
Rgb skyOf(Vec3 dir, const View& v) {
    const view3d::Sky s = view3d::skyOf(v.night, false);
    const Rgb horizon{s.horizon[0], s.horizon[1], s.horizon[2]};
    const Rgb zenith = mixed({0.30f, 0.52f, 0.86f}, {0.015f, 0.025f, 0.07f}, v.night);
    const float up = std::max(dir.y, 0.0f);
    Rgb sky = mixed(horizon, zenith, std::min(1.0f, std::pow(up / 0.55f, 0.8f)));
    const float sun = std::max(0.0f, dir.x * v.toSun.x + dir.y * v.toSun.y + dir.z * v.toSun.z);
    const float glow = 0.35f * std::pow(sun, 16.0f) + 2.0f * std::pow(sun, 900.0f);
    const Rgb tint = v.night > 0.3f ? Rgb{1.0f, 0.62f, 0.35f} : Rgb{1.0f, 0.95f, 0.85f};
    return {std::min(1.0f, sky.r + tint.r * glow), std::min(1.0f, sky.g + tint.g * glow),
            std::min(1.0f, sky.b + tint.b * glow)};
}

// One ray: the land as the strip would hold it, lit as the sky pass lights it, over the sky
Rgb seen(const Climate& c, Vec3 dir, const View& v, const Level& level, float pixelAngle) {
    if (dir.y < kRiseLow) {  // under the strip: the map itself in the game - a plain stand-in here
        const bool sandy = c.land == Land::Desert || c.land == Land::Canyon;
        const Rgb ground = sandy ? c.sand : c.grass;
        return toDisplay({ground.r * 0.9f, ground.g * 0.9f, ground.b * 0.9f});
    }
    const Rgb sky = skyOf(dir, v);
    const Hit h = march(c, kMapRadius, kEyeHeight, dir.x, dir.y, dir.z, level.steps, pixelAngle);
    if (h.coverage <= 0.0f) return sky;
    const Vec3 n = normalAt(c, kMapRadius, h.x, h.z, 0.8f + 0.0018f * h.distance);
    const Rgb albedo = albedoAt(c, h.x, h.z, h.height, n.y);
    const float shadow = shadowAt(c, kMapRadius, {h.x, h.height, h.z}, n, v.toSun, level.shadowSteps);
    const LightSet l = lightAt(v.night);
    const float ndl = std::max(0.0f, n.x * v.toSun.x + n.y * v.toSun.y + n.z * v.toSun.z) * shadow;
    const float amb = 0.55f + 0.45f * n.y;
    const Rgb lit{albedo.r * (l.ambient.r * amb + l.sun.r * ndl), albedo.g * (l.ambient.g * amb + l.sun.g * ndl),
                  albedo.b * (l.ambient.b * amb + l.sun.b * ndl)};
    const view3d::Sky s = view3d::skyOf(v.night, false);
    float haze = hazeOf(c, h.distance, h.height);
    if (onSea(c, h.height)) haze = std::max(haze, seaShine(dir.y));  // 3.0.22-beta.13
    const Rgb land = mixed(toDisplay(lit), {s.horizon[0], s.horizon[1], s.horizon[2]}, haze);
    return mixed(sky, land, h.coverage);
}

// The way to look: where the land stands tallest in a 70° window
float bestYaw(const Climate& c) {
    float lift[72] = {};
    for (int k = 0; k < 72; ++k) {
        const float a = 2.0f * kPi * float(k) / 72.0f;
        for (float d = c.mountStart; d < c.mountFull * 1.6f; d += 400.0f)
            lift[k] = std::max(lift[k], heightAt(c, kMapRadius, d * std::cos(a), d * std::sin(a)) / d);
    }
    int best = 0;
    float bestSum = -1.0f;
    for (int k = 0; k < 72; ++k) {
        float sum = 0.0f;
        for (int j = -7; j <= 7; ++j) sum += lift[(k + j + 72) % 72];
        if (sum > bestSum) bestSum = sum, best = k;
    }
    return 2.0f * kPi * float(best) / 72.0f;
}

void write(const std::string& path, const std::vector<uint32_t>& argb, int w, int h) {
    const std::string bmp = texelcheck::bmpOf(argb.data(), w, h);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fwrite(bmp.data(), 1, bmp.size(), f);
    std::fclose(f);
}

uint32_t argbOf(const Rgb& c) {
    const auto b = [](float v) { return uint32_t(std::lround(std::min(1.0f, std::max(0.0f, v)) * 255.0f)); };
    return 0xFF000000u | b(c.r) << 16 | b(c.g) << 8 | b(c.b);
}

template <typename F>
void rows(int h, F f) {  // four threads over the rows
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&, t] {
            for (int y = t; y < h; y += 4) f(y);
        });
    for (std::thread& t : ts) t.join();
}

void perspective(const Climate& c, float yaw, const View& v, int w, int h, const std::string& out) {
    const Level level = levelForCard(3);
    const float pitch = 7.0f * kPi / 180.0f, f = 0.5f * float(w) / std::tan(0.5f * 70.0f * kPi / 180.0f);
    const Vec3 fw = wayOf(yaw, pitch), rt = unit(-std::sin(yaw), 0.0f, std::cos(yaw));
    const Vec3 up{rt.y * fw.z - rt.z * fw.y, rt.z * fw.x - rt.x * fw.z, rt.x * fw.y - rt.y * fw.x};
    std::vector<uint32_t> img(size_t(w) * size_t(h));
    rows(h, [&](int y) {
        for (int x = 0; x < w; ++x) {
            const float px = float(x) + 0.5f - 0.5f * float(w), py = 0.5f * float(h) - float(y) - 0.5f;
            const Vec3 d = unit(fw.x * f + rt.x * px + up.x * py, fw.y * f + rt.y * px + up.y * py,
                                fw.z * f + rt.z * px + up.z * py);
            img[size_t(y) * size_t(w) + size_t(x)] = argbOf(seen(c, d, v, level, 1.0f / f));
        }
    });
    write(out, img, w, h);
}

void strip(const Climate& c, const View& v, const std::string& out) {
    const Level level = levelForCard(3);
    const int w = level.width / 4, h = level.height / 4;
    std::vector<uint32_t> img(size_t(w) * size_t(h));
    rows(h, [&](int y) {
        const float rise = riseOfRow((float(y) + 0.5f) / float(h)), flat = std::sqrt(1.0f - rise * rise);
        for (int x = 0; x < w; ++x) {
            const float a = 2.0f * kPi * (float(x) + 0.5f) / float(w);
            const Vec3 d{flat * std::cos(a), rise, flat * std::sin(a)};
            img[size_t(y) * size_t(w) + size_t(x)] = argbOf(seen(c, d, v, level, 2.0f * kPi / float(w)));
        }
    });
    write(out, img, w, h);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: horizon_preview <outdir> [width height]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int w = argc > 3 ? std::atoi(argv[2]) : 960, h = argc > 3 ? std::atoi(argv[3]) : 420;
    for (const char* region : kRegions) {
        const Climate c = climateOf(region);
        const float yaw = bestYaw(c);
        std::string name = region;
        std::replace(name.begin(), name.end(), ' ', '-');
        View day;
        day.toSun = wayOf(yaw + 0.5f * kPi, 25.0f * kPi / 180.0f);
        perspective(c, yaw, day, w, h, dir + "/" + name + ".bmp");
        View dusk;
        dusk.night = 0.5f;
        dusk.toSun = wayOf(yaw - 0.35f, 4.0f * kPi / 180.0f);
        perspective(c, yaw, dusk, w, h, dir + "/" + name + "-dusk.bmp");
        strip(c, day, dir + "/" + name + "-strip.bmp");
        std::printf("%s\n", region);
        std::fflush(stdout);
    }
    return 0;
}
