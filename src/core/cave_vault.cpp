#include "core/cave_vault.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

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

bool postName(const std::string& file) {
    static const std::string kWords[] = {"\xB1\xE2\xB5\xD5", "\xC1\xBE\xC0\xAF\xBC\xAE"};  // 기둥, 종유석
    for (size_t i = 0; i < file.size();) {
        // cp949: a byte from 0x81 leads a character of two
        const bool wide = static_cast<unsigned char>(file[i]) >= 0x81 && i + 1 < file.size();
        if (wide)
            for (const std::string& word : kWords)
                if (file.compare(i, word.size(), word) == 0) return true;
        i += wide ? 2 : 1;
    }
    return false;
}

bool postOf(const rsm::Model& model, const rsw::Model& at, Post& out) {
    if (model.faces.empty() || !postName(at.file)) return false;
    std::vector<float> corners;
    corners.reserve(model.faces.size() * 9);
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (const rsm::Face& f : model.faces)
        for (int k = 0; k < 3; ++k) {
            float p[3] = {f.pos[k][0], f.pos[k][1], f.pos[k][2]};
            rsw::placeInMap(at, p);
            for (int a = 0; a < 3; ++a) {
                lo[a] = std::min(lo[a], p[a]);
                hi[a] = std::max(hi[a], p[a]);
            }
            corners.insert(corners.end(), p, p + 3);
        }
    const float top = -lo[1], tall = hi[1] - lo[1];  // up along -y
    const float widest = std::max(hi[0] - lo[0], hi[2] - lo[2]);
    if (!(tall >= kPostLeast) || widest > tall) return false;
    float headLo[2] = {1e30f, 1e30f}, headHi[2] = {-1e30f, -1e30f};  // its top tenth's own width, x and z
    for (size_t i = 0; i + 2 < corners.size(); i += 3) {
        if (-corners[i + 1] < top - 0.1f * tall) continue;
        headLo[0] = std::min(headLo[0], corners[i]);
        headHi[0] = std::max(headHi[0], corners[i]);
        headLo[1] = std::min(headLo[1], corners[i + 2]);
        headHi[1] = std::max(headHi[1], corners[i + 2]);
    }
    const float across[2] = {headHi[0] - headLo[0], headHi[1] - headLo[1]};
    if (std::min(across[0], across[1]) < kPostHead * widest) return false;
    out.x = 0.5f * (headLo[0] + headHi[0]);
    out.z = 0.5f * (headLo[1] + headHi[1]);
    out.radius = 0.5f * std::max(across[0], across[1]);
    out.top = top;
    return true;
}

void refine(Vault& v, int times) {
    if (times <= 1 || v.width <= 0 || v.height <= 0 || v.up.size() < size_t(v.width) * size_t(v.height)) return;
    const int w = v.width * times, h = v.height * times;
    std::vector<float> fine(size_t(w) * size_t(h));
    for (int z = 0; z < h; ++z) {
        const float gz = std::clamp((float(z) + 0.5f) / float(times) - 0.5f, 0.0f, float(v.height - 1));
        const int z0 = int(gz), z1 = std::min(z0 + 1, v.height - 1);
        const float fz = gz - float(z0);
        for (int x = 0; x < w; ++x) {
            const float gx = std::clamp((float(x) + 0.5f) / float(times) - 0.5f, 0.0f, float(v.width - 1));
            const int x0 = int(gx), x1 = std::min(x0 + 1, v.width - 1);
            const float fx = gx - float(x0);
            const auto at = [&](int cx, int cz) { return v.up[size_t(cz) * size_t(v.width) + size_t(cx)]; };
            fine[size_t(z) * size_t(w) + size_t(x)] = (at(x0, z0) * (1.0f - fx) + at(x1, z0) * fx) * (1.0f - fz) +
                                                      (at(x0, z1) * (1.0f - fx) + at(x1, z1) * fx) * fz;
        }
    }
    blur(fine, w, h, times / 2, true);
    blur(fine, w, h, times / 2, false);
    v.up.swap(fine);
    v.width = w;
    v.height = h;
    v.zoom /= float(times);
}

size_t restOn(Vault& v, const ground_heights::Ground& g, const std::vector<Post>& posts) {
    if (v.width <= 0 || v.height <= 0 || v.up.size() < size_t(v.width) * size_t(v.height)) return 0;
    if (v.wallTop - v.floorLevel < kLeastRelief) return 0;  // a plain dome: no walls' height to reach
    size_t met = 0;
    for (const Post& p : posts) {
        float y = 0.0f, own = 0.0f;
        if (!ground_heights::heightAt(g, p.x, p.z, y) || !heightAt(v, p.x, p.z, own)) continue;
        const float floor = -y, cap = p.top - kPostSink;
        if (p.top - floor < kPostReach * (v.wallTop - floor)) continue;  // a stone under the walls' height
        if (own <= cap) continue;  // the vault at its head already, or under it
        const float inner = p.radius + kPostPad, outer = inner + kPostSpread;
        const float mx = p.x / v.zoom + float(v.width) * 0.5f, mz = p.z / v.zoom + float(v.height) * 0.5f;
        const int reach = int(std::ceil(outer / v.zoom)) + 1;
        const int x0 = std::max(0, int(mx) - reach), x1 = std::min(v.width - 1, int(mx) + reach);
        const int z0 = std::max(0, int(mz) - reach), z1 = std::min(v.height - 1, int(mz) + reach);
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x) {
                const float dx = (float(x) + 0.5f - mx) * v.zoom, dz = (float(z) + 0.5f - mz) * v.zoom;
                const float d = std::sqrt(dx * dx + dz * dz);
                float& up = v.up[size_t(z) * size_t(v.width) + size_t(x)];
                if (d >= outer || up <= cap) continue;
                const float u = std::clamp((d - inner) / kPostSpread, 0.0f, 1.0f);
                up = std::min(up, cap + (up - cap) * (1.0f - (1.0f - u) * (1.0f - u)));  // steep by the head
            }
        ++met;
    }
    return met;
}

bool build(const ground_heights::Ground& g, const std::vector<Post>& posts, Vault& out, size_t* met) {
    if (met) *met = 0;
    if (!build(g, out)) return false;
    if (posts.empty()) return true;
    Vault fine = out;
    refine(fine, kFine);
    const size_t n = restOn(fine, g, posts);
    if (n == 0) return true;
    out = std::move(fine);
    if (met) *met = n;
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

void rockOf(bool lightKnown, const float ambient[3], const float diffuse[3], float out[3]) {
    for (int k = 0; k < 3; ++k)
        out[k] = lightKnown ? std::clamp((ambient[k] + diffuse[k] * 0.35f) * kRock[k], 0.06f, 0.6f) : kRockUnlit[k];
}

bool forMap(const std::string& map) {
    std::string m;
    for (char c : map) m += char(std::tolower(static_cast<unsigned char>(c)));
    return m.find("dun") != std::string::npos || m.rfind("anthell", 0) == 0;
}

}  // namespace uagfx::cave_vault
