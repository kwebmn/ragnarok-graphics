#include "core/cave_vault.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace uagfx::cave_vault {
namespace {

float percentile(std::vector<float> v, float share) {
    const size_t at = std::min(v.size() - 1, size_t(std::lround(share * float(v.size() - 1))));
    std::nth_element(v.begin(), v.begin() + std::ptrdiff_t(at), v.end());
    return v[at];
}

// One pass of a box blur `radius` cells each way, along x (`stride` 1) or z (stride width), clamped at the edges
void blur(std::vector<float>& v, int width, int height, int radius, bool alongX) {
    std::vector<float> out(v.size());
    const int n = alongX ? width : height;
    for (int line = 0; line < (alongX ? height : width); ++line)
        for (int i = 0; i < n; ++i) {
            float sum = 0.0f;
            int count = 0;
            for (int k = std::max(0, i - radius); k <= std::min(n - 1, i + radius); ++k, ++count)
                sum += v[alongX ? size_t(line) * size_t(width) + size_t(k) : size_t(k) * size_t(width) + size_t(line)];
            out[alongX ? size_t(line) * size_t(width) + size_t(i) : size_t(i) * size_t(width) + size_t(line)] =
                sum / float(count);
        }
    v.swap(out);
}

}  // namespace

bool build(const ground_heights::Ground& g, Vault& out) {
    out = Vault{};
    const size_t cells = size_t(std::max(g.width, 0)) * size_t(std::max(g.height, 0));
    if (cells == 0 || g.corners.size() < cells * 4) return false;
    std::vector<float> floor(cells);
    for (size_t i = 0; i < cells; ++i) {
        const float* h = &g.corners[i * 4];
        floor[i] = -(h[0] + h[1] + h[2] + h[3]) * 0.25f;  // up positive
    }
    out.width = g.width;
    out.height = g.height;
    out.zoom = g.zoom;
    out.wallTop = percentile(floor, kWallShare);
    out.floorLevel = percentile(floor, kFloorShare);
    out.up.resize(cells);
    if (out.wallTop - out.floorLevel < kLeastRelief) {  // the walls the models': a plain dome
        for (size_t i = 0; i < cells; ++i) out.up[i] = floor[i] + kFlatRoom;
        return true;
    }
    const auto least = [&](float f) { return f + (f >= out.wallTop - 1.0f ? kLeastOver : kLeastRoom); };
    for (size_t i = 0; i < cells; ++i)
        out.up[i] = std::clamp(2.0f * out.wallTop - floor[i], least(floor[i]), floor[i] + kMostRoom);
    blur(out.up, g.width, g.height, kSmoothCells, true);
    blur(out.up, g.width, g.height, kSmoothCells, false);
    for (size_t i = 0; i < cells; ++i) out.up[i] = std::clamp(out.up[i], least(floor[i]), floor[i] + kMostRoom);
    return true;
}

bool heightAt(const Vault& v, float x, float z, float& up) {
    if (v.width <= 0 || v.height <= 0 || v.up.size() < size_t(v.width) * size_t(v.height)) return false;
    const float fx = x / v.zoom + float(v.width) * 0.5f, fz = z / v.zoom + float(v.height) * 0.5f;
    if (!(fx >= 0.0f && fz >= 0.0f && fx < float(v.width) && fz < float(v.height))) return false;
    const float gx = std::clamp(fx - 0.5f, 0.0f, float(v.width - 1));
    const float gz = std::clamp(fz - 0.5f, 0.0f, float(v.height - 1));
    const int x0 = int(gx), z0 = int(gz), x1 = std::min(x0 + 1, v.width - 1), z1 = std::min(z0 + 1, v.height - 1);
    const float u = gx - float(x0), w = gz - float(z0);
    const auto at = [&](int cx, int cz) { return v.up[size_t(cz) * size_t(v.width) + size_t(cx)]; };
    up = (at(x0, z0) * (1.0f - u) + at(x1, z0) * u) * (1.0f - w) + (at(x0, z1) * (1.0f - u) + at(x1, z1) * u) * w;
    return true;
}

bool forMap(const std::string& map) {
    std::string m;
    for (char c : map) m += char(std::tolower(static_cast<unsigned char>(c)));
    return m.find("dun") != std::string::npos || m.rfind("anthell", 0) == 0;
}

}  // namespace uagfx::cave_vault
