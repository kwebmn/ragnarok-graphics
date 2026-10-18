#include "core/map_textures.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

#include "core/rsm.hpp"
#include "core/rsw.hpp"
#include "core/zip.hpp"

namespace uagfx::map_textures {

std::vector<std::string> groundTextures(const uint8_t* data, size_t size) {
    std::vector<std::string> out;
    // "GRGN", the version (2 bytes), width, height, zoom; then the textures' count and the length of a name
    if (!data || size < 26 || std::memcmp(data, "GRGN", 4) != 0) return out;
    uint32_t count = 0, length = 0;
    std::memcpy(&count, data + 18, 4);
    std::memcpy(&length, data + 22, 4);
    if (!length || length > 256 || count > 4096 || 26 + uint64_t(count) * length > size) return out;
    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reinterpret_cast<const char*>(data + 26 + size_t(i) * length);
        out.emplace_back(name, strnlen(name, length));
    }
    return out;
}

namespace {

// A ground file read to its cells: its textures (normal names), its surfaces, its cells' top surfaces.
struct Ground {
    int width = 0, height = 0;
    std::vector<std::string> textures;
    struct Surface {
        float u[4], v[4];
        int texture;
    };
    std::vector<Surface> surfaces;
    std::vector<int> tops;  // width * height: a surface, or -1
};

bool readGround(const uint8_t* data, size_t size, Ground& g) {
    const std::vector<std::string> names = groundTextures(data, size);
    if (names.empty()) return false;
    const auto i32 = [&](size_t at) {
        int32_t v = 0;
        std::memcpy(&v, data + at, 4);
        return v;
    };
    g.width = i32(6);
    g.height = i32(10);
    const uint32_t length = uint32_t(i32(22));
    size_t at = 26 + names.size() * length;
    if (g.width <= 0 || g.height <= 0 || g.width > 4096 || g.height > 4096 || at + 16 > size) return false;
    const int64_t lightmaps = i32(at), lw = i32(at + 4), lh = i32(at + 8);
    if (lightmaps < 0 || lw < 0 || lh < 0 || lw > 64 || lh > 64) return false;
    const uint64_t light = uint64_t(lightmaps) * uint64_t(lw) * uint64_t(lh) * 4;
    if (light > size - at - 16) return false;
    at += 16 + size_t(light);
    if (at + 4 > size) return false;
    const int64_t surfaces = i32(at);
    at += 4;
    if (surfaces < 0 || uint64_t(surfaces) * 40 > size - at) return false;
    g.surfaces.resize(size_t(surfaces));
    for (Ground::Surface& s : g.surfaces) {
        std::memcpy(s.u, data + at, 16);
        std::memcpy(s.v, data + at + 16, 16);
        int16_t t = 0;
        std::memcpy(&t, data + at + 32, 2);
        s.texture = t;
        at += 40;
    }
    const uint64_t cells = uint64_t(g.width) * uint64_t(g.height);
    if (cells * 28 > size - at) return false;
    g.tops.resize(size_t(cells));
    for (size_t c = 0; c < g.tops.size(); ++c, at += 28) {
        const int top = i32(at + 16);
        g.tops[c] = top >= 0 && top < surfaces ? top : -1;
    }
    for (const std::string& n : names) g.textures.push_back(zip::normalName("data\\texture\\" + n));
    return true;
}

UvRect rectOf(const float u[4], const float v[4]) {
    UvRect r{u[0], v[0], u[0], v[0]};
    for (int i = 1; i < 4; ++i) {
        r.u0 = std::min(r.u0, u[i]);
        r.u1 = std::max(r.u1, u[i]);
        r.v0 = std::min(r.v0, v[i]);
        r.v1 = std::max(r.v1, v[i]);
    }
    return r;
}

bool near(float a, float b) { return std::fabs(a - b) < 1.0f / 1024.0f; }

// one range ends where the other begins (the texture repeats: 1 is 0)
bool meets(float end, float begin) { return near(std::fmod(end, 1.0f), std::fmod(begin, 1.0f)); }

// a and b side by side (across, or else down): b's rectangle takes up where a's ends, or the other way
bool goesOn(const UvRect& a, const UvRect& b, bool across) {
    if (across) return near(a.v0, b.v0) && near(a.v1, b.v1) && (meets(a.u1, b.u0) || meets(b.u1, a.u0));
    return near(a.u0, b.u0) && near(a.u1, b.u1) && (meets(a.v1, b.v0) || meets(b.v1, a.v0));
}

}  // namespace

std::map<std::string, GroundUse> groundUses(const uint8_t* data, size_t size) {
    std::map<std::string, GroundUse> out;
    Ground g;
    if (!readGround(data, size, g)) return out;
    const auto textureOf = [&](int surface) -> const std::string* {
        const int t = g.surfaces[size_t(surface)].texture;
        return t >= 0 && size_t(t) < g.textures.size() ? &g.textures[size_t(t)] : nullptr;
    };
    for (const Ground::Surface& s : g.surfaces) {
        if (s.texture < 0 || size_t(s.texture) >= g.textures.size()) continue;
        GroundUse& use = out[g.textures[size_t(s.texture)]];
        const UvRect r = rectOf(s.u, s.v);
        bool known = false;
        for (const UvRect& k : use.rects)
            known = known || (near(k.u0, r.u0) && near(k.v0, r.v0) && near(k.u1, r.u1) && near(k.v1, r.v1));
        if (!known) use.rects.push_back(r);
    }
    for (int y = 0; y < g.height; ++y)
        for (int x = 0; x < g.width; ++x) {
            const int a = g.tops[size_t(y) * size_t(g.width) + size_t(x)];
            if (a < 0 || !textureOf(a)) continue;
            for (int across = 0; across < 2; ++across) {
                const int nx = x + across, ny = y + 1 - across;
                if (nx >= g.width || ny >= g.height) continue;
                const int b = g.tops[size_t(ny) * size_t(g.width) + size_t(nx)];
                if (b < 0 || g.surfaces[size_t(a)].texture != g.surfaces[size_t(b)].texture) continue;
                GroundUse& use = out[*textureOf(a)];
                ++use.pairs;
                const Ground::Surface &sa = g.surfaces[size_t(a)], &sb = g.surfaces[size_t(b)];
                if (goesOn(rectOf(sa.u, sa.v), rectOf(sb.u, sb.v), across != 0)) ++use.joined;
            }
        }
    return out;
}

std::vector<PatchCell> groundPatch(const uint8_t* data, size_t size, const std::string& texture, int side) {
    std::vector<PatchCell> out;
    Ground g;
    if (side <= 0 || !readGround(data, size, g)) return out;
    const auto takes = [&](int x, int y) {
        const int s = g.tops[size_t(y) * size_t(g.width) + size_t(x)];
        if (s < 0) return false;
        const int t = g.surfaces[size_t(s)].texture;
        return t >= 0 && size_t(t) < g.textures.size() && g.textures[size_t(t)] == texture;
    };
    // the cells taking it, summed over the map: the window holding the most
    const int w = g.width, h = g.height;
    std::vector<int> sum(size_t(w + 1) * size_t(h + 1), 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            sum[size_t(y + 1) * size_t(w + 1) + size_t(x + 1)] = int(takes(x, y)) +
                                                                 sum[size_t(y) * size_t(w + 1) + size_t(x + 1)] +
                                                                 sum[size_t(y + 1) * size_t(w + 1) + size_t(x)] -
                                                                 sum[size_t(y) * size_t(w + 1) + size_t(x)];
    const int sw = std::min(side, w), sh = std::min(side, h);
    int best = 0, bx = 0, by = 0;
    for (int y = 0; y + sh <= h; ++y)
        for (int x = 0; x + sw <= w; ++x) {
            const auto at = [&](int xx, int yy) { return sum[size_t(yy) * size_t(w + 1) + size_t(xx)]; };
            const int n = at(x + sw, y + sh) - at(x, y + sh) - at(x + sw, y) + at(x, y);
            if (n > best) best = n, bx = x, by = y;
        }
    if (!best) return out;
    for (int y = by; y < by + sh; ++y)
        for (int x = bx; x < bx + sw; ++x)
            if (takes(x, y)) {
                const Ground::Surface& s = g.surfaces[size_t(g.tops[size_t(y) * size_t(w) + size_t(x)])];
                PatchCell c;
                c.x = x - bx;
                c.y = y - by;
                std::copy(s.u, s.u + 4, c.u);
                std::copy(s.v, s.v + 4, c.v);
                out.push_back(c);
            }
    return out;
}

std::string mapName(const std::string& text) {
    std::string s = zip::normalName(text);
    const size_t from = s.find_first_not_of(" \t");
    const size_t to = s.find_last_not_of(" \t");
    s = from == std::string::npos ? std::string() : s.substr(from, to - from + 1);
    if (s.compare(0, 5, "data\\") == 0) s = s.substr(5);
    for (const char* ext : {".gnd", ".rsw", ".gat"})
        if (s.size() > 4 && s.compare(s.size() - 4, 4, ext) == 0) s.resize(s.size() - 4);
    if (s.empty() || s.find_first_of("\\:*?\"<>|") != std::string::npos || s.find("..") != std::string::npos)
        return "";
    return s;
}

std::vector<std::string> filesOf(const std::string& map, bool ground, bool models, const Read& read) {
    const std::string name = mapName(map);
    if (name.empty()) return {};
    std::set<std::string> out;
    const auto texture = [&](const std::string& t) {
        if (!t.empty()) out.insert(zip::normalName("data\\texture\\" + t));
    };
    std::vector<uint8_t> data;
    if (ground && read("data\\" + name + ".gnd", data))
        for (const std::string& t : groundTextures(data.data(), data.size())) texture(t);
    if (models && read("data\\" + name + ".rsw", data)) {
        rsw::World world;
        std::string error;
        if (rsw::parse(data.data(), data.size(), world, error)) {
            std::set<std::string> seen;
            for (const rsw::Model& m : world.models) {
                const std::string file = zip::normalName("data\\model\\" + m.file);
                if (!seen.insert(file).second || !read(file, data)) continue;
                rsm::Model model;
                if (rsm::parse(data.data(), data.size(), model, error))
                    for (const rsm::Part& p : model.parts) texture(p.texture);
            }
        }
    }
    return {out.begin(), out.end()};
}

}  // namespace uagfx::map_textures
