#include "core/ground_heights.hpp"

#include <cmath>
#include <cstring>

namespace uagfx::ground_heights {

bool parse(const uint8_t* data, size_t size, Ground& out, std::string& error) {
    out = Ground{};
    const auto i32 = [&](size_t at) {
        int32_t v = 0;
        std::memcpy(&v, data + at, 4);
        return v;
    };
    if (!data || size < 26 || std::memcmp(data, "GRGN", 4) != 0) {
        error = "not a .gnd";
        return false;
    }
    const int32_t width = i32(6), height = i32(10), textures = i32(18), length = i32(22);
    float zoom = 0.0f;
    std::memcpy(&zoom, data + 14, 4);
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || !(zoom > 0.0f && zoom < 1000.0f) ||
        textures < 0 || textures > 4096 || length < 0 || length > 256) {
        error = "its header makes no sense";
        return false;
    }
    uint64_t at = 26 + uint64_t(textures) * uint64_t(length);
    if (at + 16 > size) {
        error = "cut short in its textures";
        return false;
    }
    const int32_t lightmaps = i32(size_t(at)), lw = i32(size_t(at) + 4), lh = i32(size_t(at) + 8);
    if (lightmaps < 0 || lw < 0 || lh < 0 || lw > 64 || lh > 64) {
        error = "its lightmaps make no sense";
        return false;
    }
    at += 16 + uint64_t(lightmaps) * uint64_t(lw) * uint64_t(lh) * 4;
    if (at + 4 > size) {
        error = "cut short in its lightmaps";
        return false;
    }
    const int32_t surfaces = i32(size_t(at));
    at += 4 + uint64_t(surfaces < 0 ? 0 : surfaces) * 40;
    const uint64_t cells = uint64_t(width) * uint64_t(height);
    if (surfaces < 0 || at + cells * 28 > size) {
        error = "cut short in its cells";
        return false;
    }
    out.width = width;
    out.height = height;
    out.zoom = zoom;
    out.corners.resize(size_t(cells) * 4);
    for (size_t c = 0; c < size_t(cells); ++c) std::memcpy(&out.corners[c * 4], data + at + c * 28, 16);
    return true;
}

bool heightAt(const Ground& g, float x, float z, float& y) {
    if (g.corners.size() < size_t(g.width) * size_t(g.height) * 4) return false;
    const float fx = x / g.zoom + float(g.width) * 0.5f, fz = z / g.zoom + float(g.height) * 0.5f;
    const float cx = std::floor(fx), cz = std::floor(fz);
    if (!(cx >= 0.0f && cz >= 0.0f && cx < float(g.width) && cz < float(g.height))) return false;
    const float* h = &g.corners[(size_t(cz) * size_t(g.width) + size_t(cx)) * 4];
    const float u = fx - cx, v = fz - cz;
    y = (h[0] + (h[1] - h[0]) * u) * (1.0f - v) + (h[2] + (h[3] - h[2]) * u) * v;
    return true;
}

bool clear(const Ground& g, const map::Point& from, const map::Point& to, float skipFrom, float skipTo, float slack) {
    const float dx = to.x - from.x, dz = to.z - from.z, d = std::hypot(dx, dz);
    if (g.corners.empty() || !(d > 0.0f)) return true;
    const float step = g.zoom * 0.25f;  // a quarter cell: a wall a cell thick is met three times or four
    for (float s = std::max(step, skipFrom); s < d - skipTo; s += step) {
        const float t = s / d, line = from.y + (to.y - from.y) * t;
        float y = 0.0f;
        if (heightAt(g, from.x + dx * t, from.z + dz * t, y) && y < line - slack) return false;  // up along -y
    }
    return true;
}

}  // namespace uagfx::ground_heights
