#include "core/ground_heights.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

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
    // 2.7.57: each cell's top's texture (its surface's, 40 bytes a surface after their count) - the laid patches
    const uint64_t firstSurface = at - uint64_t(surfaces) * 40;
    std::vector<int> texture(size_t(cells), -1);
    for (size_t c = 0; c < size_t(cells); ++c) {
        const int32_t top = i32(size_t(at + c * 28 + 16));
        if (top < 0 || top >= surfaces) continue;
        int16_t t = 0;
        std::memcpy(&t, data + firstSurface + uint64_t(top) * 40 + 32, 2);
        texture[c] = t;
    }
    out.laid = laidPatches(width, height, texture);
    return true;
}

std::vector<uint8_t> laidPatches(int width, int height, const std::vector<int>& texture) {
    const size_t cells = size_t(std::max(width, 0)) * size_t(std::max(height, 0));
    std::vector<uint8_t> laid(cells, 0);
    if (texture.size() < cells) return laid;
    // the patches: one texture, side by side (4 neighbours); a rectangle filled whole is laid
    std::vector<int> patch(cells, -1);
    std::vector<uint8_t> whole;
    std::unordered_map<int, std::pair<size_t, size_t>> share;  // texture -> (its cells laid so, its cells)
    std::vector<size_t> stack, members;
    for (size_t start = 0; start < cells; ++start) {
        if (texture[start] < 0 || patch[start] >= 0) continue;
        const int t = texture[start], id = int(whole.size());
        int x0 = width, z0 = height, x1 = -1, z1 = -1;
        members.clear();
        stack.assign(1, start);
        patch[start] = id;
        while (!stack.empty()) {
            const size_t c = stack.back();
            stack.pop_back();
            members.push_back(c);
            const int x = int(c % size_t(width)), z = int(c / size_t(width));
            x0 = std::min(x0, x), x1 = std::max(x1, x), z0 = std::min(z0, z), z1 = std::max(z1, z);
            const int around[4][2] = {{x + 1, z}, {x - 1, z}, {x, z + 1}, {x, z - 1}};
            for (const auto& n : around) {
                if (n[0] < 0 || n[1] < 0 || n[0] >= width || n[1] >= height) continue;
                const size_t m = size_t(n[1]) * size_t(width) + size_t(n[0]);
                if (texture[m] != t || patch[m] >= 0) continue;
                patch[m] = id;
                stack.push_back(m);
            }
        }
        const bool filled = size_t(x1 - x0 + 1) * size_t(z1 - z0 + 1) == members.size();
        whole.push_back(filled ? 1 : 0);
        auto& s = share[t];
        if (filled) s.first += members.size();
        s.second += members.size();
    }
    for (size_t c = 0; c < cells; ++c) {
        if (patch[c] < 0) continue;
        const auto& s = share[texture[c]];
        laid[c] = whole[size_t(patch[c])] || s.first * 2 >= s.second ? 1 : 0;
    }
    return laid;
}

bool laidAt(const Ground& g, float x, float z) {
    if (g.laid.size() < size_t(g.width) * size_t(g.height)) return false;
    const float fx = std::floor(x / g.zoom + float(g.width) * 0.5f);
    const float fz = std::floor(z / g.zoom + float(g.height) * 0.5f);
    if (!(fx >= 0.0f && fz >= 0.0f && fx < float(g.width) && fz < float(g.height))) return false;
    return g.laid[size_t(fz) * size_t(g.width) + size_t(fx)] != 0;
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
