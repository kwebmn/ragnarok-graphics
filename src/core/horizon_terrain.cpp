#include "core/horizon_terrain.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::horizon {
namespace {

float clamp01(float v) { return std::min(1.0f, std::max(0.0f, v)); }
float smooth(float a, float b, float x) {
    const float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
float mixf(float a, float b, float t) { return a + (b - a) * t; }
Rgb mix(const Rgb& a, const Rgb& b, float t) { return {mixf(a.r, b.r, t), mixf(a.g, b.g, t), mixf(a.b, b.b, t)}; }
Rgb scaled(const Rgb& a, float s) { return {a.r * s, a.g * s, a.b * s}; }
float noise(float x, float z, uint32_t seed) { return noised(x, z, seed).v; }

// The far land's height by its kind - mountains, mesas, islands, pillars - over the near hills
float farLand(const Climate& c, float x, float z, float far, float hills) {
    const float mx = x * c.mountFreq, mz = z * c.mountFreq;
    switch (c.land) {
    case Land::Desert: {  // dunes along a wind; mesas with terraced cliffs far off
        const float wind = 0.3f * noise(x * 0.0006f, z * 0.0006f, c.seed + 7);
        const float dune = 1.0f - std::fabs(noise(x * 0.0021f + wind, z * 0.0009f, c.seed + 5));
        const float m = 0.5f + 0.5f * fbm(mx, mz, 4, c.seed + 9);
        const float mesa = smooth(0.52f, 0.56f, m) * (0.8f + 0.2f * smooth(0.6f, 0.75f, m));
        const float steps = std::floor(m * 6.0f) / 6.0f;
        return c.hillAmp * (0.35f + 0.65f * dune * dune) + far * c.mountAmp * mesa * (0.55f + 0.45f * steps);
    }
    case Land::Sea: {  // islands out of the sea (below 0)
        const float isl = 0.5f + 0.5f * fbm(mx, mz, 6, c.seed + 3);
        const float land = smooth(0.56f, 0.70f, isl);
        const float e = 0.5f + 0.5f * eroded(mx * 3.0f, mz * 3.0f, 7, c.seed + 11);
        // 3.0.22-beta.13 (R-VDN9YH): low near the map, whole far off as the other lands' mountains - Comodo's stood
        // whole a step past the beach, a green wall up to the strip's roof
        return land * c.mountAmp * (0.25f + 0.75f * e) * (0.15f + 0.85f * far) - (1.0f - land) * 60.0f;
    }
    case Land::Karst: {  // tall narrow pillars with rounded tops
        const float n = 0.5f + 0.5f * fbm(mx, mz, 4, c.seed + 9);
        const float pillar = smooth(0.56f, 0.61f, n);
        const float top = 0.7f + 0.3f * (0.5f + 0.5f * eroded(mx * 2.0f, mz * 2.0f, 5, c.seed + 13));
        return hills + far * c.mountAmp * pillar * top;
    }
    case Land::Canyon: {  // terraced plateaus cut by winding canyons
        const float m = 0.5f + 0.5f * fbm(mx, mz, 5, c.seed + 9);
        const float s = m * 5.0f, f = s - std::floor(s);
        const float terrace = (std::floor(s) + smooth(0.7f, 1.0f, f)) / 5.0f;
        const float cut = smooth(0.0f, 0.12f, std::fabs(noise(mx * 1.7f, mz * 1.7f, c.seed + 21)));
        return hills + far * c.mountAmp * terrace * (0.35f + 0.65f * cut);
    }
    default: {  // Hills, Alps, Waste: eroded ridges
        const float e = std::max(0.0f, 0.5f + 0.5f * eroded(mx, mz, 9, c.seed));  // pow wants it >= 0
        const float ridge = 1.0f - std::fabs(noise(mx * 0.7f, mz * 0.7f, c.seed + 77));
        float m;
        if (c.land == Land::Alps)
            m = std::pow(e, 1.6f) * (0.6f + 0.4f * ridge * ridge);
        else if (c.land == Land::Waste)
            m = std::pow(e, 1.4f) * (0.6f + 0.4f * ridge);
        else
            m = e * e * (0.75f + 0.25f * ridge * ridge);
        return hills + far * c.mountAmp * m;
    }
    }
}

}  // namespace

uint32_t hash2(int x, int y, uint32_t seed) {
    uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

Noise noised(float x, float z, uint32_t seed) {
    const float ix = std::floor(x), iz = std::floor(z), fx = x - ix, fz = z - iz;
    const float ux = fx * fx * fx * (fx * (fx * 6.0f - 15.0f) + 10.0f);
    const float uz = fz * fz * fz * (fz * (fz * 6.0f - 15.0f) + 10.0f);
    const float dux = 30.0f * fx * fx * (fx * (fx - 2.0f) + 1.0f), duz = 30.0f * fz * fz * (fz * (fz - 2.0f) + 1.0f);
    const int i = int(ix), j = int(iz);
    const auto at = [&](int a, int b) { return float(hash2(a, b, seed) & 0xFFFFFFu) / 16777216.0f; };
    const float a = at(i, j), b = at(i + 1, j), c = at(i, j + 1), d = at(i + 1, j + 1);
    const float k1 = b - a, k2 = c - a, k4 = a - b - c + d;
    Noise n;
    n.v = 2.0f * (a + k1 * ux + k2 * uz + k4 * ux * uz) - 1.0f;
    n.dx = 2.0f * dux * (k1 + k4 * uz);
    n.dz = 2.0f * duz * (k2 + k4 * ux);
    return n;
}

float fbm(float x, float z, int octaves, uint32_t seed) {
    float sum = 0.0f, amp = 0.5f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * noise(x, z, seed + uint32_t(i) * 17u);
        amp *= 0.5f;
        const float nx = 1.6f * x - 1.2f * z, nz = 1.2f * x + 1.6f * z;
        x = nx;
        z = nz;
    }
    return sum;
}

float eroded(float x, float z, int octaves, uint32_t seed) {
    float sum = 0.0f, amp = 1.0f, dx = 0.0f, dz = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        const Noise n = noised(x, z, seed + uint32_t(i) * 31u);
        dx += n.dx;
        dz += n.dz;
        sum += amp * n.v / (1.0f + dx * dx + dz * dz);
        amp *= 0.5f;
        const float nx = 1.6f * x - 1.2f * z, nz = 1.2f * x + 1.6f * z;
        x = nx;
        z = nz;
    }
    return sum;
}

float heightAt(const Climate& c, float mapRadius, float x, float z) {
    const float d = std::sqrt(x * x + z * z);
    const float belt = smooth(mapRadius, mapRadius + kLandRise, d);
    if (belt <= 0.0f) return 0.0f;
    const float hills = c.hillAmp * (0.5f + 0.5f * fbm(x * c.hillFreq, z * c.hillFreq, 5, c.seed + 101));
    const float far = smooth(c.mountStart, c.mountFull, d);
    const float h = farLand(c, x, z, far, hills) * belt;
    // a soft roof: no land rises over kRoof of its distance - the strip's top row (kRiseHigh) stays sky, a peak
    // that would pass it rounded rather than cut flat by the strip's edge
    if (h <= 0.0f) return h;
    const float roof = kRoof * d;
    return roof * std::tanh(h / roof);
}

Vec3 normalAt(const Climate& c, float mapRadius, float x, float z, float eps) {
    const float hx = heightAt(c, mapRadius, x + eps, z) - heightAt(c, mapRadius, x - eps, z);
    const float hz = heightAt(c, mapRadius, x, z + eps) - heightAt(c, mapRadius, x, z - eps);
    const float nx = -hx, ny = 2.0f * eps, nz = -hz, l = std::sqrt(nx * nx + ny * ny + nz * nz);
    return {nx / l, ny / l, nz / l};
}

Rgb albedoAt(const Climate& c, float x, float z, float height, float upness) {
    const float tex = 0.5f + 0.5f * fbm(x * 0.004f, z * 0.004f, 4, c.seed + 400);
    if (c.land == Land::Desert || c.land == Land::Canyon) {
        const float rk = smooth(0.80f, 0.65f, upness);
        Rgb rock = scaled(c.rock, 0.8f + 0.4f * tex);
        if (c.land == Land::Canyon)  // bands in the cliffs
            rock = scaled(rock, 0.85f + 0.3f * (0.5f + 0.5f * std::sin(height * 0.02f + 3.0f * tex)));
        Rgb a = mix(scaled(c.sand, 0.9f + 0.2f * tex), rock, rk);
        const float sn = smooth(c.snowLine - 180.0f, c.snowLine + 120.0f, height) * smooth(0.42f, 0.66f, upness);
        return mix(a, c.snow, sn);
    }
    if (c.land == Land::Sea) {
        if (height < 0.5f) return {0.02f, 0.09f, 0.15f};             // the sea
        if (height < 25.0f) return scaled(c.sand, 0.9f + 0.2f * tex);  // a beach
    }
    const float forestMask = smooth(0.48f, 0.56f, 0.5f + 0.5f * fbm(x * 0.0011f, z * 0.0011f, 5, c.seed + 500)) *
                             smooth(0.70f, 0.85f, upness) * smooth(c.treeLine + 150.0f, c.treeLine - 150.0f, height);
    const Rgb grass = scaled(c.grass, 0.75f + 0.5f * tex);
    const Rgb forest = scaled(c.forest, 0.7f + 0.6f * (0.5f + 0.5f * noise(x * 0.05f, z * 0.05f, c.seed + 600)));
    Rgb a = mix(grass, forest, forestMask);
    a = mix(a, scaled(c.rock, 0.75f + 0.5f * tex), smooth(0.78f, 0.6f, upness));
    const float snowAt = height + 260.0f * fbm(x * 0.003f, z * 0.003f, 3, c.seed + 700);
    const float sn = smooth(c.snowLine - 180.0f, c.snowLine + 120.0f, snowAt) * smooth(0.42f, 0.66f, upness);
    return mix(a, c.snow, sn);
}

Hit march(const Climate& c, float mapRadius, float eyeHeight, float dirX, float dirUp, float dirZ, int steps,
          float pixelAngle) {
    Hit out;
    const float flat = std::sqrt(dirX * dirX + dirZ * dirZ);
    if (flat < 1e-4f) return out;
    const float maxH = std::max(c.mountAmp, 0.0f) + c.hillAmp + 200.0f, tMax = 80000.0f;
    float t = mapRadius / flat, prev = t, least = 1e9f;
    if (eyeHeight + dirUp * t <= 0.0f) return out;  // it meets the ground within the map: the game's own
    int i = 0;
    for (; i < steps && t < tMax; ++i) {
        const float x = dirX * t, y = eyeHeight + dirUp * t, z = dirZ * t;
        if (y > maxH && dirUp > 0.0f) return out.coverage > 0.0f ? out : Hit{};
        const float h = heightAt(c, mapRadius, x, z), dy = y - h;
        if (dy / t < least) {  // the closest pass so far: the skyline's soft edge takes its land
            least = dy / t;
            out.x = x;
            out.z = z;
            out.height = h;
            out.distance = t;
            out.coverage = least < pixelAngle ? clamp01(1.0f - least / pixelAngle) : 0.0f;
        }
        if (dy < 0.0015f * t) {
            float a = prev, b = t;
            for (int k = 0; k < 6; ++k) {
                const float m = 0.5f * (a + b);
                if (eyeHeight + dirUp * m - heightAt(c, mapRadius, dirX * m, dirZ * m) < 0.0f)
                    b = m;
                else
                    a = m;
            }
            out.hit = true;
            out.distance = b;
            out.x = dirX * b;
            out.z = dirZ * b;
            out.height = heightAt(c, mapRadius, out.x, out.z);
            out.coverage = 1.0f;
            return out;
        }
        prev = t;
        t += std::max(0.004f * t + 1.0f, 0.4f * dy);
    }
    // the steps ran out on a ray along the horizon: far land in the haze, as the eye would meet it
    if (i >= steps && dirUp < 0.08f && eyeHeight + dirUp * t < maxH) {
        out.hit = true;
        out.distance = t;
        out.x = dirX * t;
        out.z = dirZ * t;
        out.height = heightAt(c, mapRadius, out.x, out.z);
        out.coverage = 1.0f;
    }
    return out;
}

float shadowAt(const Climate& c, float mapRadius, Vec3 at, Vec3 normal, Vec3 toSun, int steps) {
    if (steps <= 0 || toSun.y <= 0.0f) return steps <= 0 ? 1.0f : 0.0f;
    float lit = 1.0f, st = 8.0f;
    for (int i = 0; i < steps && st < 9000.0f; ++i) {
        const float x = at.x + normal.x + toSun.x * st, y = at.y + normal.y + toSun.y * st;
        const float z = at.z + normal.z + toSun.z * st;
        const float gap = y - heightAt(c, mapRadius, x, z);
        lit = std::min(lit, 12.0f * gap / st);
        if (lit < 0.001f) break;
        st += std::max(8.0f, gap * 0.5f);
    }
    return clamp01(lit);
}

float hazeOf(const Climate& c, float distance, float height) {
    return 1.0f - std::exp(-distance * c.haze * (1.0f - 0.45f * smooth(0.0f, 3000.0f, height)));
}

float seaShine(float rise) {
    const float g = std::clamp(1.0f - 12.0f * std::fabs(rise), 0.0f, 1.0f);
    return g * g;
}

}  // namespace uagfx::horizon

namespace uagfx::horizon {

LightSet lightAt(float night) {
    static const LightSet kDay{{1.90f, 1.75f, 1.50f}, {0.042f, 0.126f, 0.328f}};
    static const LightSet kDusk{{1.70f, 0.85f, 0.36f}, {0.050f, 0.070f, 0.160f}};
    static const LightSet kNight{{0.10f, 0.12f, 0.18f}, {0.020f, 0.030f, 0.060f}};
    const float n = clamp01(night);
    const LightSet& a = n < 0.5f ? kDay : kDusk;
    const LightSet& b = n < 0.5f ? kDusk : kNight;
    const float t = n < 0.5f ? n / 0.5f : (n - 0.5f) / 0.5f;
    return {mix(a.sun, b.sun, t), mix(a.ambient, b.ambient, t)};
}

LightSet lightAt(float night, float rise) {
    LightSet l = lightAt(night);
    const float low = (1.0f - clamp01(night)) * smooth(0.30f, 0.05f, rise);
    if (low > 0.0f) {
        const LightSet warm = lightAt(0.5f);
        l.sun = mix(l.sun, warm.sun, low);
        l.ambient = mix(l.ambient, warm.ambient, 0.5f * low);
    }
    l.sun = scaled(l.sun, smooth(-0.02f, 0.10f, rise));
    return l;
}

Rgb toDisplay(const Rgb& linear) {
    const auto curve = [](float v) {
        const float x = std::max(0.0f, v) * kExposure;
        return std::pow(clamp01(x * (2.51f * x + 0.03f) / (x * (2.43f * x + 0.59f) + 0.14f)), 1.0f / 2.2f);
    };
    return {curve(linear.r), curve(linear.g), curve(linear.b)};
}

}  // namespace uagfx::horizon
