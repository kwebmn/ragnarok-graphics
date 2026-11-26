#include "core/cave_vault.hpp"

#include "core/outskirts.hpp"

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

bool buildCeiling(const ground_heights::Ground& g, float modelsTop, const rooms::Map* rooms, Vault& out) {
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
    out.ceiling = out.wallTop - out.floorLevel >= kLeastRelief
                      ? out.wallTop + kCeilingOver
                      : out.floorLevel + (modelsTop > 0.0f ? modelsTop + kCeilingOver : kCeilingFlat);
    out.up.resize(cells);
    for (size_t i = 0; i < cells; ++i) out.up[i] = std::max(out.ceiling, floor[i] + kLeastOver);
    if (!rooms || rooms->count <= 0 || rooms->width != g.width || rooms->height != g.height ||
        rooms->widened.size() < cells || rooms->floor.size() <= size_t(rooms->count))
        return true;
    // 3.0.20-beta.57: over each room's own floor; a cell of no room its nearest room's (from the rooms outwards)
    const float room = modelsTop > 0.0f ? modelsTop + kCeilingOver : kCeilingFlat;
    std::vector<uint8_t> nearest(rooms->widened.begin(), rooms->widened.begin() + std::ptrdiff_t(cells));
    std::vector<uint32_t> edge, next;
    for (size_t i = 0; i < cells; ++i)
        if (nearest[i]) edge.push_back(uint32_t(i));
    while (!edge.empty()) {
        next.clear();
        for (const uint32_t k : edge) {
            const int x = int(k % uint32_t(g.width)), y = int(k / uint32_t(g.width));
            const int beside[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& d : beside) {
                const int nx = x + d[0], ny = y + d[1];
                if (nx < 0 || ny < 0 || nx >= g.width || ny >= g.height) continue;
                const size_t j = size_t(ny) * size_t(g.width) + size_t(nx);
                if (nearest[j]) continue;
                nearest[j] = nearest[k];
                next.push_back(uint32_t(j));
            }
        }
        edge.swap(next);
    }
    std::vector<float> ceilings;
    for (int r = 1; r <= rooms->count; ++r) ceilings.push_back(rooms->floor[size_t(r)] + room);
    for (size_t i = 0; i < cells; ++i)
        if (nearest[i]) out.up[i] = rooms->floor[nearest[i]] + room;
    out.ceiling = percentile(ceilings, 0.5f);
    out.perRoom = true;
    return true;
}

float wallsByModels(std::vector<float> tall) { return tall.empty() ? 0.0f : percentile(std::move(tall), 0.8f); }

float tallOf(const rsm::Model& model, const rsw::Model& at) {
    float lo = 1e30f, hi = -1e30f;
    for (const rsm::Face& f : model.faces)
        for (int k = 0; k < 3; ++k) {
            float p[3] = {f.pos[k][0], f.pos[k][1], f.pos[k][2]};
            rsw::placeInMap(at, p);
            lo = std::min(lo, p[1]);
            hi = std::max(hi, p[1]);
        }
    return hi > lo ? hi - lo : 0.0f;
}

bool ceilingFor(const std::string& map, bool outdoors) {
    if (outdoors || map.empty() || forMap(map)) return false;
    std::string m;
    for (char c : map) m += char(std::tolower(static_cast<unsigned char>(c)));
    if (m.rfind("in_", 0) == 0) return true;
    for (const char* word : {"_in", "church", "castle", "gl_cas", "gl_knt", "tower", "pryd"})
        if (m.find(word) != std::string::npos) return true;
    return false;
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

namespace {

// a part of a texture's picture some cells show, and how much raised ground and how many floors show it
struct Use {
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    float rock = 0.0f, floors = 0.0f;
};
// a ground looked over once: each texture's measure as a rock (rockPicture's), its surfaces' spans, its parts' uses
struct Scan {
    std::vector<float> score;
    std::vector<std::vector<float>> spans;
    std::vector<std::vector<Use>> uses;
};

Scan scan(const gnd::Ground& g) {
    Scan out;
    const size_t cells = size_t(std::max(g.width, 0)) * size_t(std::max(g.height, 0));
    if (cells == 0 || g.cells.size() < cells || g.textures.empty()) return out;
    std::vector<float> ups(cells);
    for (size_t i = 0; i < cells; ++i) {
        const float* h = g.cells[i].h;
        ups[i] = -(h[0] + h[1] + h[2] + h[3]) * 0.25f;  // up positive
    }
    const float floorLevel = percentile(ups, kFloorShare), wallTop = percentile(ups, kWallShare);
    const float raisedFrom = floorLevel + kRockRaised * (wallTop - floorLevel);
    out.score.assign(g.textures.size(), 0.0f);
    out.spans.resize(g.textures.size());
    out.uses.resize(g.textures.size());
    // a surface of a texture the file has
    const auto surfaceOf = [&](int32_t index) -> const gnd::Surface* {
        if (index < 0 || size_t(index) >= g.surfaces.size()) return nullptr;
        const gnd::Surface& s = g.surfaces[size_t(index)];
        return s.texture >= 0 && size_t(s.texture) < g.textures.size() ? &s : nullptr;
    };
    const auto useOf = [&](const gnd::Surface& s) -> Use& {
        Use u;
        u.u0 = std::min(std::min(s.u[0], s.u[1]), std::min(s.u[2], s.u[3]));
        u.u1 = std::max(std::max(s.u[0], s.u[1]), std::max(s.u[2], s.u[3]));
        u.v0 = std::min(std::min(s.v[0], s.v[1]), std::min(s.v[2], s.v[3]));
        u.v1 = std::max(std::max(s.v[0], s.v[1]), std::max(s.v[2], s.v[3]));
        std::vector<Use>& known = out.uses[size_t(s.texture)];
        for (Use& k : known)
            if (std::fabs(k.u0 - u.u0) + std::fabs(k.v0 - u.v0) + std::fabs(k.u1 - u.u1) + std::fabs(k.v1 - u.v1) <
                1.0f / 64.0f)
                return k;
        known.push_back(u);
        return known.back();
    };
    // how much of its picture lies along a cell's side (whichever way round it is laid)
    const auto rocky = [&](const gnd::Surface& s, float area) {
        out.score[size_t(s.texture)] += area;
        out.spans[size_t(s.texture)].push_back(std::max(std::fabs(s.u[1] - s.u[0]), std::fabs(s.u[2] - s.u[0])));
        useOf(s).rock += area;
    };
    const float zoom = std::max(g.zoom, 1e-3f);
    for (int y = 0; y < g.height; ++y)
        for (int x = 0; x < g.width; ++x) {
            const size_t i = size_t(y) * size_t(g.width) + size_t(x);
            const gnd::Cell& c = g.cells[i];
            if (const gnd::Surface* top = surfaceOf(c.top)) {
                const float lo = std::min(std::min(c.h[0], c.h[1]), std::min(c.h[2], c.h[3]));
                const float hi = std::max(std::max(c.h[0], c.h[1]), std::max(c.h[2], c.h[3]));
                if (hi - lo >= kRockSteep || (ups[i] >= raisedFrom && ups[i] < wallTop - 1.0f)) rocky(*top, 1.0f);
                else if (ups[i] < raisedFrom) useOf(*top).floors += 1.0f;
            }
            for (int side = 0; side < 2; ++side) {  // its front (towards y + 1), its right (towards x + 1)
                const gnd::Surface* face = surfaceOf(side ? c.right : c.front);
                const int nx = side ? x + 1 : x, ny = side ? y : y + 1;
                if (!face || nx >= g.width || ny >= g.height) continue;
                const gnd::Cell& n = g.cells[size_t(ny) * size_t(g.width) + size_t(nx)];
                const float own = side ? 0.5f * (c.h[1] + c.h[3]) : 0.5f * (c.h[2] + c.h[3]);
                const float next = side ? 0.5f * (n.h[0] + n.h[2]) : 0.5f * (n.h[0] + n.h[1]);
                rocky(*face, std::fabs(own - next) / zoom);
            }
        }
    return out;
}

// the textures by their measure as a rock, the most first: kRockLeast at least, the makers' fillers never
std::vector<size_t> ranked(const gnd::Ground& g, const Scan& s) {
    std::vector<size_t> order;
    for (size_t t = 0; t < s.score.size(); ++t)
        if (s.score[t] >= kRockLeast && !outskirts::fillerTexture(g.textures[t])) order.push_back(t);
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return s.score[a] > s.score[b]; });
    return order;
}

float cellsOf(const Scan& s, size_t texture) {
    const float span = s.spans[texture].empty() ? 0.0f : percentile(s.spans[texture], 0.5f);
    return span > 1e-4f ? std::clamp(1.0f / span, 1.0f, 16.0f) : 4.0f;
}

}  // namespace

Rock rockPicture(const gnd::Ground& g) {
    const Scan s = scan(g);
    const std::vector<size_t> order = ranked(g, s);
    Rock rock;
    if (order.empty()) return rock;
    rock.texture = int(order[0]);
    rock.cells = cellsOf(s, order[0]);
    return rock;
}

Patch rockPatch(const gnd::Ground& g, const ground_cells::PictureOf& pictureOf) {
    Patch out;
    if (!pictureOf) return out;
    const Scan s = scan(g);
    std::vector<size_t> order = ranked(g, s);
    if (order.size() > size_t(kRockTries)) order.resize(size_t(kRockTries));
    for (const size_t t : order) {
        ground_cells::Picture p;
        if (!pictureOf(t, p) || !p.argb || p.width <= 0 || p.height <= 0) continue;
        const int w = p.width, h = p.height;
        const size_t count = size_t(w) * size_t(h);
        std::vector<uint8_t> shown(count, 0);  // the parts the raised ground shows more than the floors do
        bool any = false;
        for (const Use& u : s.uses[t]) {
            if (!(u.rock > u.floors)) continue;
            const int x0 = std::clamp(int(std::lround(u.u0 * float(w))), 0, w);
            const int x1 = std::clamp(int(std::lround(u.u1 * float(w))), 0, w);
            const int y0 = std::clamp(int(std::lround(u.v0 * float(h))), 0, h);
            const int y1 = std::clamp(int(std::lround(u.v1 * float(h))), 0, h);
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) shown[size_t(y) * size_t(w) + size_t(x)] = 1;
            any = any || (x1 > x0 && y1 > y0);
        }
        if (!any) continue;
        std::vector<float> bright(count);  // each texel's brightness, a see-through one black
        for (size_t i = 0; i < count; ++i) {
            const uint32_t c = p.argb[i];
            bright[i] = (c >> 24) == 0 ? 0.0f
                                       : (0.299f * float((c >> 16) & 0xFF) + 0.587f * float((c >> 8) & 0xFF) +
                                          0.114f * float(c & 0xFF)) / 255.0f;
        }
        std::vector<float> even = bright;  // evened over a few texels: a crack is still rock, a soft edge is not
        const int reach = std::max(2, w / 40);
        blur(even, w, h, reach, true);
        blur(even, w, h, reach, false);
        std::vector<float> lit;  // the rock's usual brightness: the shown texels that are not the blackness
        for (size_t i = 0; i < count; ++i)
            if (shown[i] && bright[i] >= 0.08f) lit.push_back(even[i]);
        if (lit.empty()) continue;
        const float dimmest = kPatchBright * percentile(lit, 0.5f);
        // the largest square of good texels: each texel the side of the largest one ending at it
        std::vector<int> ending(count, 0);
        int side = 0;
        double sum[3] = {0.0, 0.0, 0.0};
        size_t seen = 0;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const size_t i = size_t(y) * size_t(w) + size_t(x);
                if (!shown[i] || even[i] < dimmest) continue;
                sum[0] += double((p.argb[i] >> 16) & 0xFF);
                sum[1] += double((p.argb[i] >> 8) & 0xFF);
                sum[2] += double(p.argb[i] & 0xFF);
                ++seen;
                ending[i] = x > 0 && y > 0 ? 1 + std::min(std::min(ending[i - 1], ending[i - size_t(w)]),
                                                          ending[i - size_t(w) - 1])
                                           : 1;
                side = std::max(side, ending[i]);
            }
        const float texels = float(w) / (cellsOf(s, t) * std::max(g.zoom, 1e-3f));
        if (!out.known && seen) {  // the rock's colour, should no sheet give a patch
            for (int k = 0; k < 3; ++k) out.mean[k] = float(sum[k] / (255.0 * double(seen)));
            out.known = true;
            out.texels = texels;
        }
        const int least = std::max(int(kPatchLeastUnits * texels + 0.5f), 4);
        if (side < least) continue;
        // the picture's sums from its first corner: a box's mean colour in four reads
        const size_t across = size_t(w) + 1;
        std::vector<uint32_t> sums(across * (size_t(h) + 1) * 3, 0u);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const uint32_t c = p.argb[size_t(y) * size_t(w) + size_t(x)];
                const uint32_t rgb[3] = {(c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF};
                const size_t at = (size_t(y) + 1) * across + size_t(x) + 1;
                for (size_t k = 0; k < 3; ++k)
                    sums[at * 3 + k] = rgb[k] + sums[(at - 1) * 3 + k] + sums[(at - across) * 3 + k] -
                                       sums[(at - across - 1) * 3 + k];
            }
        // a box's hue: its colours' shares of their sum (x1, y1 past its end); its brightness too
        const auto hueOf = [&](int x0, int y0, int x1, int y1, float hue[3]) {
            float total = 0.0f;
            for (size_t k = 0; k < 3; ++k) {
                const size_t a0 = size_t(y0) * across + size_t(x0), a1 = size_t(y0) * across + size_t(x1);
                const size_t b0 = size_t(y1) * across + size_t(x0), b1 = size_t(y1) * across + size_t(x1);
                hue[k] = float(sums[b1 * 3 + k] - sums[a1 * 3 + k] - sums[b0 * 3 + k] + sums[a0 * 3 + k]);
                total += hue[k];
            }
            for (int k = 0; k < 3; ++k) hue[k] = total > 0.0f ? hue[k] / total : 1.0f / 3.0f;
        };
        // how far a square's ninths' hues lie from its own: the furthest of them, and all of them on the whole
        const auto spreadOf = [&](int x, int y, int size, float& mean) {
            float whole[3];
            hueOf(x, y, x + size, y + size, whole);
            float furthest = 0.0f;
            mean = 0.0f;
            for (int by = 0; by < 3; ++by)
                for (int bx = 0; bx < 3; ++bx) {
                    float part[3], off = 0.0f;
                    hueOf(x + bx * size / 3, y + by * size / 3, x + (bx + 1) * size / 3, y + (by + 1) * size / 3,
                          part);
                    for (int k = 0; k < 3; ++k) off = std::max(off, std::fabs(part[k] - whole[k]));
                    furthest = std::max(furthest, off);
                    mean += off / 9.0f;
                }
            return furthest;
        };
        const int full = std::min(side, std::max(int(kPatchUnits * texels + 0.5f), 4));
        int cut = 0, cutX = 0, cutY = 0;
        float cutSpread = 1e9f;
        for (const int size : {full, full * 3 / 4, full / 2}) {
            if (size < least || size == cut) continue;
            int hereX = 0, hereY = 0;
            float here = 1e9f, hereFurthest = 1e9f;  // the most even of this size: its furthest ninth and all of them
            for (int y = size - 1; y < h; ++y)
                for (int x = size - 1; x < w; ++x) {
                    if (ending[size_t(y) * size_t(w) + size_t(x)] < size) continue;
                    float mean = 0.0f;
                    const float furthest = spreadOf(x - size + 1, y - size + 1, size, mean);
                    if (furthest + mean < here)
                        here = furthest + mean, hereFurthest = furthest, hereX = x - size + 1, hereY = y - size + 1;
                }
            if (here > 1e8f) continue;
            if (here < cutSpread) cutSpread = here, cut = size, cutX = hereX, cutY = hereY;
            if (hereFurthest <= kPatchSame) {  // the largest of one stuff
                cut = size, cutX = hereX, cutY = hereY;
                break;
            }
        }
        if (cut <= 0) continue;
        out.texture = int(t);
        out.side = cut;
        out.x = cutX;
        out.y = cutY;
        out.texels = texels;
        double mean[3] = {0.0, 0.0, 0.0};
        for (int y = out.y; y < out.y + cut; ++y)
            for (int x = out.x; x < out.x + cut; ++x) {
                const uint32_t c = p.argb[size_t(y) * size_t(w) + size_t(x)];
                mean[0] += double((c >> 16) & 0xFF);
                mean[1] += double((c >> 8) & 0xFF);
                mean[2] += double(c & 0xFF);
            }
        for (int k = 0; k < 3; ++k) out.mean[k] = float(mean[k] / (255.0 * double(cut) * double(cut)));
        out.known = true;
        // straight lines from side to side - bricks' courses, tiles' joints, a panel's frame: no rock to lay in pieces
        float lines = 0.0f;
        for (int along = 0; along < 2; ++along) {  // the rows' edges, then the columns'
            std::vector<float> edges(size_t(cut) - 1, 0.0f);
            for (int a = 0; a + 1 < cut; ++a)
                for (int b = 0; b < cut; ++b) {
                    const size_t here = along ? size_t(out.y + b) * size_t(w) + size_t(out.x + a)
                                              : size_t(out.y + a) * size_t(w) + size_t(out.x + b);
                    edges[size_t(a)] += std::fabs(bright[here + (along ? 1 : size_t(w))] - bright[here]) / float(cut);
                }
            const float most = *std::max_element(edges.begin(), edges.end());
            lines = std::max(lines, most / std::max(percentile(edges, 0.5f), 1e-4f));
        }
        if (lines >= kPatchLines) out.side = 0;  // its colour alone
        return out;
    }
    return out;
}

void rockLight(bool lightKnown, const float ambient[3], const float diffuse[3], float out[3]) {
    for (int k = 0; k < 3; ++k)
        out[k] = lightKnown ? std::clamp(ambient[k] + diffuse[k] * 0.35f, kRockLightLeast, 1.0f) : kRockLightUnlit;
}

Mean meanOf(const uint32_t* argb, size_t count) {
    Mean m;
    double sum[3] = {0.0, 0.0, 0.0};
    size_t seen = 0;
    for (size_t i = 0; argb && i < count; ++i) {
        if ((argb[i] >> 24) == 0) continue;  // a see-through texel
        sum[0] += double((argb[i] >> 16) & 0xFF);
        sum[1] += double((argb[i] >> 8) & 0xFF);
        sum[2] += double(argb[i] & 0xFF);
        ++seen;
    }
    if (seen == 0) return m;
    for (int k = 0; k < 3; ++k) m.rgb[k] = float(sum[k] / (255.0 * double(seen)));
    m.known = true;
    return m;
}

void wallTone(const rsm::Model& model, const rsw::Model& at, const std::vector<Mean>& means, Tone& sum) {
    if (model.faces.empty()) return;
    std::vector<float> corners;
    corners.reserve(model.faces.size() * 9);
    float lo = 1e30f, hi = -1e30f;
    for (const rsm::Face& f : model.faces)
        for (int k = 0; k < 3; ++k) {
            float p[3] = {f.pos[k][0], f.pos[k][1], f.pos[k][2]};
            rsw::placeInMap(at, p);
            lo = std::min(lo, p[1]);
            hi = std::max(hi, p[1]);
            corners.insert(corners.end(), p, p + 3);
        }
    const float middle = 0.5f * (lo + hi);  // up along -y: the upper half lies at it or under it in y
    for (size_t i = 0; i < model.faces.size(); ++i) {
        const int texture = model.faces[i].texture;
        if (texture < 0 || size_t(texture) >= means.size() || !means[size_t(texture)].known) continue;
        const float* a = &corners[i * 9];
        const float* b = a + 3;
        const float* c = a + 6;
        if ((a[1] + b[1] + c[1]) / 3.0f > middle) continue;
        const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        const double area = 0.5 * std::sqrt(double(n[0]) * n[0] + double(n[1]) * n[1] + double(n[2]) * n[2]);
        for (int k = 0; k < 3; ++k) sum.rgb[k] += area * double(means[size_t(texture)].rgb[k]);
        sum.weight += area;
    }
}

bool toneOf(const Tone& sum, float out[3]) {
    if (!(sum.weight > 1e-6)) return false;
    for (int k = 0; k < 3; ++k) out[k] = float(sum.rgb[k] / sum.weight);
    return true;
}

void ceilingColour(bool wallsKnown, const float walls[3], bool lightKnown, const float ambient[3],
                   const float diffuse[3], float out[3]) {
    if (!wallsKnown) {
        rockOf(lightKnown, ambient, diffuse, out);
        return;
    }
    float light[3];
    rockLight(lightKnown, ambient, diffuse, light);
    for (int k = 0; k < 3; ++k) out[k] = std::clamp(walls[k] * light[k] * kCeilingShade, 0.04f, 0.9f);
}

bool forMap(const std::string& map) {
    std::string m;
    for (char c : map) m += char(std::tolower(static_cast<unsigned char>(c)));
    // 3.0.24-beta.10: the Comodo town's cave - a rock roof over it in the 3D view (its fields cmd_fild open)
    return m.find("dun") != std::string::npos || m.rfind("anthell", 0) == 0 || m == "comodo";
}

}  // namespace uagfx::cave_vault
