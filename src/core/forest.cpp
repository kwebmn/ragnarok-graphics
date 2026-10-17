#include "core/forest.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::forest {
namespace {

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

// cp949 words
const char* const kTree = "\xb3\xaa\xb9\xab";              // 나무
const char* const kBush = "\xb4\xfd\xba\xd2";              // 덤불
const char* const kPalm = "\xbe\xdf\xc0\xda\xbc\xf6";      // 야자수
const char* const kFir = "\xc4\xa7\xbf\xb1\xbc\xf6";       // 침엽수
const char* const kWillow = "\xb9\xf6\xb5\xe5\xb3\xaa\xb9\xab";  // 버드나무
const char* const kBamboo = "\xb4\xeb\xb3\xaa\xb9\xab";    // 대나무
const char* const kNot[] = {"\xb9\xde\xc4\xa7", "\xb1\xe2\xb5\xd5", "\xc1\xb6\xb0\xa2", "\xb5\xee\xb0\xc9",
                            "\xb1\xd7\xb8\xa9", "\xc0\xc7\xc0\xda", "\xc1\xd9\xb1\xe2"};
// 받침 a stand, 기둥 a post, 조각 a piece, 등걸 a stump, 그릇 a bowl, 의자 a chair, 줄기 a stem

uint32_t mix(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

class Random {
public:
    explicit Random(uint32_t seed) : s_(mix(seed + 0x9e3779b9u)) {}
    float next() {
        s_ = mix(s_ + 0x9e3779b9u);
        return float(s_ & 0xffffff) / float(0x1000000);
    }
    float range(float a, float b) { return a + (b - a) * next(); }

private:
    uint32_t s_;
};

struct V3 {
    float x, y, z;
};
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator*(V3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 norm(V3 a) {
    const float l = std::sqrt(dot(a, a));
    return l > 1e-9f ? a * (1.0f / l) : V3{0, -1, 0};
}

// Builds in a room with up positive (y), turned into the model's (up along -y) as it puts.
class Builder {
public:
    Builder(Mesh& m, float height) : m_(m), height_(std::max(height, 1.0f)) {}

    uint32_t put(V3 p, V3 n, float u, float v, float open, float part, float seed) {
        Vertex x{};
        x.pos[0] = p.x, x.pos[1] = -p.y, x.pos[2] = p.z;
        x.normal[0] = n.x, x.normal[1] = -n.y, x.normal[2] = n.z;
        x.uv[0] = u, x.uv[1] = v;
        x.open = std::clamp(open, 0.0f, 1.0f);
        const float up = std::clamp(p.y / height_, 0.0f, 1.0f);
        x.sway = up * up * (part > 0.5f ? 1.0f : 0.35f);
        x.part = part;
        x.seed = seed;
        m_.verts.push_back(x);
        return uint32_t(m_.verts.size() - 1);
    }
    void quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {  // a b / c d
        for (uint32_t i : {a, b, c, c, b, d}) m_.indices.push_back(i);
    }

    // A tube along a path of points, its radius at each; `sides` round.
    void tube(const std::vector<V3>& path, const std::vector<float>& radius, int sides, float open) {
        if (path.size() < 2) return;
        const uint32_t first = uint32_t(m_.verts.size());
        float along = 0.0f;
        for (size_t i = 0; i < path.size(); ++i) {
            const V3 dir = norm(i + 1 < path.size() ? path[i + 1] - path[i] : path[i] - path[i - 1]);
            const V3 side = norm(cross(dir, std::fabs(dir.y) > 0.9f ? V3{1, 0, 0} : V3{0, 1, 0}));
            const V3 other = cross(side, dir);
            if (i > 0) along += std::sqrt(dot(path[i] - path[i - 1], path[i] - path[i - 1]));
            for (int k = 0; k <= sides; ++k) {
                const float a = float(k) / float(sides) * 6.2831853f;
                const V3 n = side * std::cos(a) + other * std::sin(a);
                put(path[i] + n * radius[i], n, float(k) / float(sides) * 2.0f, along / 6.0f, open, 0.0f, 0.0f);
            }
        }
        const uint32_t ring = uint32_t(sides + 1);
        for (size_t i = 0; i + 1 < path.size(); ++i)
            for (int k = 0; k < sides; ++k) {
                const uint32_t a = first + uint32_t(i) * ring + uint32_t(k);
                quad(a, a + 1, a + ring, a + ring + 1);
            }
    }

    // A card of leaves at p facing n, `size` across, turned by `spin`; its light's normal `light`.
    void card(V3 p, V3 n, V3 light, float size, float spin, float open, float part, float seed) {
        const V3 side0 = norm(cross(n, std::fabs(n.y) > 0.9f ? V3{1, 0, 0} : V3{0, 1, 0}));
        const V3 up0 = cross(side0, n);
        const V3 side = side0 * std::cos(spin) + up0 * std::sin(spin), up = cross(side, n);
        const float h = size * 0.5f;
        const uint32_t a = put(p - side * h + up * h, light, 0, 0, open, part, seed);
        const uint32_t b = put(p + side * h + up * h, light, 1, 0, open, part, seed);
        const uint32_t c = put(p - side * h - up * h, light, 0, 1, open, part, seed);
        const uint32_t d = put(p + side * h - up * h, light, 1, 1, open, part, seed);
        quad(a, b, c, d);
    }

private:
    Mesh& m_;
    float height_;
};

}  // namespace

Kind kindOf(const std::string& file) {
    const size_t cut = file.find_last_of('\\');
    const std::string base = cut == std::string::npos ? file : file.substr(cut + 1);
    if (has(base, kBamboo)) return Kind::None;
    if (has(base, kPalm)) return Kind::Palm;
    if (has(base, kFir)) return Kind::Conifer;
    if (has(base, kWillow)) return Kind::Willow;
    if (has(base, kBush)) return Kind::Bush;
    if (!has(base, kTree)) return Kind::None;
    for (const char* n : kNot)
        if (has(base, n)) return Kind::None;
    return Kind::Broadleaf;
}

bool shapeOf(Kind kind, const rsm::Model& model, Shape& out) {
    out = Shape{};
    if (kind == Kind::None || model.parts.empty()) return false;
    const float width = std::max(model.hi[0] - model.lo[0], model.hi[2] - model.lo[2]);
    const float height = -model.lo[1];
    if (height < 6.0f || width < 4.0f) return false;
    // the trunk: the tallest narrow part
    const rsm::Part* trunk = nullptr;
    for (const rsm::Part& p : model.parts) {
        const float w = std::max(p.hi[0] - p.lo[0], p.hi[2] - p.lo[2]), h = p.hi[1] - p.lo[1];
        if (w < width * 0.3f && h > height * 0.3f && (!trunk || h > trunk->hi[1] - trunk->lo[1])) trunk = &p;
    }
    bool any = false;
    for (int a = 0; a < 3; ++a) {
        out.crownLo[a] = 1e30f;
        out.crownHi[a] = -1e30f;
    }
    for (const rsm::Part& p : model.parts) {
        if (&p == trunk) continue;
        any = true;
        for (int a = 0; a < 3; ++a) {
            out.crownLo[a] = std::min(out.crownLo[a], p.lo[a]);
            out.crownHi[a] = std::max(out.crownHi[a], p.hi[a]);
        }
    }
    if (!any) return false;
    out.kind = kind == Kind::Broadleaf && !trunk ? Kind::Bush : kind;
    out.height = height;
    const float crownBottom = -out.crownHi[1];
    if (trunk && out.kind != Kind::Bush) {
        out.trunkRadius = std::clamp(std::max(trunk->hi[0] - trunk->lo[0], trunk->hi[2] - trunk->lo[2]) * 0.35f,
                                     0.35f, width * 0.1f);
        out.trunkTop = std::clamp(crownBottom + (height - crownBottom) * 0.45f, height * 0.3f, height * 0.9f);
        if (out.kind == Kind::Palm) out.trunkTop = std::max(crownBottom, height * 0.6f);
    }
    return true;
}

Mesh grow(const Shape& shape, uint32_t seed) {
    Mesh m;
    if (shape.kind == Kind::None) return m;
    Random rnd(seed);
    Builder b(m, shape.height);
    // the crown, up positive
    const V3 c{(shape.crownLo[0] + shape.crownHi[0]) * 0.5f, -(shape.crownLo[1] + shape.crownHi[1]) * 0.5f,
               (shape.crownLo[2] + shape.crownHi[2]) * 0.5f};
    const V3 r{std::max((shape.crownHi[0] - shape.crownLo[0]) * 0.5f, 1.0f),
               std::max((shape.crownHi[1] - shape.crownLo[1]) * 0.5f, 1.0f),
               std::max((shape.crownHi[2] - shape.crownLo[2]) * 0.5f, 1.0f)};
    const float across = (r.x + r.z) * 0.5f;
    // the trunk: from the foot (a little wider) up into the crown, bending a little
    V3 top{c.x, shape.trunkTop, c.z};
    if (shape.trunkRadius > 0.0f) {
        const float bend = shape.kind == Kind::Palm ? 0.12f : 0.05f;
        const float ba = rnd.range(0.0f, 6.2831853f);
        const V3 lean{std::cos(ba) * shape.height * bend, 0.0f, std::sin(ba) * shape.height * bend};
        std::vector<V3> path;
        std::vector<float> rad;
        const int steps = 6;
        for (int i = 0; i <= steps; ++i) {
            const float t = float(i) / float(steps);
            path.push_back(V3{c.x, t * shape.trunkTop, c.z} + lean * (t * t));
            const float flare = 1.0f + 0.45f * std::max(0.0f, 1.0f - t * 6.0f);
            rad.push_back(shape.trunkRadius * (1.0f - 0.45f * t) * flare);
        }
        top = path.back();
        b.tube(path, rad, shape.kind == Kind::Palm ? 6 : 8, 0.6f);
    }
    if (shape.kind == Kind::Palm) {
        // fronds from the top: long strips arching out and down
        const int fronds = int(rnd.range(8.0f, 11.0f));
        for (int f = 0; f < fronds; ++f) {
            const float az = (float(f) + rnd.range(-0.3f, 0.3f)) / float(fronds) * 6.2831853f;
            const V3 out{std::cos(az), 0.0f, std::sin(az)};
            const float len = across * rnd.range(0.85f, 1.1f), lift = rnd.range(0.35f, 0.75f);
            const float s = rnd.next();
            const int segs = 5;
            uint32_t prev[2] = {0, 0};
            for (int i = 0; i <= segs; ++i) {
                const float t = float(i) / float(segs);
                const V3 p = top + out * (len * t) + V3{0, (lift * t - t * t * 1.1f) * len * 0.6f, 0};
                const V3 side = norm(cross(out, V3{0, 1, 0}));
                const float w = len * 0.22f * (1.0f - 0.6f * t) + 0.3f;
                const V3 n = norm(V3{0, 1, 0} + out * (t - 0.4f));
                const uint32_t a = b.put(p - side * w, n, 0, t, 0.85f, 2.0f, s);
                const uint32_t d = b.put(p + side * w, n, 1, t, 0.85f, 2.0f, s);
                if (i > 0) b.quad(prev[0], prev[1], a, d);
                prev[0] = a, prev[1] = d;
            }
        }
        return m;
    }
    if (shape.kind == Kind::Conifer) {
        // tiers of twigs round the trunk, wide below and narrow to the tip
        const float bottom = c.y - r.y, height = 2.0f * r.y;
        const int tiers = int(std::clamp(height / 3.0f, 5.0f, 10.0f));
        for (int t = 0; t < tiers; ++t) {
            const float k = (float(t) + 0.5f) / float(tiers);
            const float y = bottom + height * k, reach = across * (1.05f - k) * 1.0f;
            const int twigs = int(std::clamp(reach * 1.6f, 6.0f, 18.0f));
            for (int i = 0; i < twigs; ++i) {
                const float az = (float(i) + rnd.next() * 0.6f) / float(twigs) * 6.2831853f + float(t) * 0.7f;
                const V3 out{std::cos(az), 0.0f, std::sin(az)};
                const float d = reach * rnd.range(0.45f, 0.85f);
                const V3 p = V3{c.x, y, c.z} + out * d + V3{0, -d * 0.18f, 0};
                const V3 light = norm(out * 0.8f + V3{0, 0.6f, 0});
                b.card(p, norm(V3{0, 1, 0} + out * 0.5f), light, reach * 0.75f + 0.8f, az, 0.35f + 0.65f * k, 3.0f,
                       rnd.next());
            }
        }
        b.card(V3{c.x, bottom + height + 0.6f, c.z}, norm(V3{0.3f, 1, 0}), V3{0, 1, 0}, across * 0.35f + 0.6f, 0.0f,
               1.0f, 3.0f, rnd.next());
        return m;
    }
    // branches into the crown (not a bush's)
    std::vector<V3> tips;
    if (shape.trunkRadius > 0.0f) {
        const int branches = int(rnd.range(5.0f, 8.0f));
        for (int i = 0; i < branches; ++i) {
            const float az = (float(i) + rnd.range(-0.3f, 0.3f)) / float(branches) * 6.2831853f;
            const float from = rnd.range(0.55f, 0.95f);
            const V3 start{c.x + (top.x - c.x) * from, shape.trunkTop * from, c.z + (top.z - c.z) * from};
            const float up = rnd.range(0.35f, 0.8f), len = across * rnd.range(0.55f, 0.8f);
            const float droop = shape.kind == Kind::Willow ? -0.5f : 0.0f;
            const V3 dir = norm(V3{std::cos(az), up + droop, std::sin(az)});
            std::vector<V3> path = {start, start + dir * (len * 0.5f),
                                    start + dir * len + V3{0, droop * len * 0.3f, 0}};
            b.tube(path, {shape.trunkRadius * 0.4f, shape.trunkRadius * 0.25f, shape.trunkRadius * 0.12f}, 4, 0.5f);
            tips.push_back(path.back());
        }
    }
    // the leaves: clumps - about the branches' tips, one on top, a few more - each a ball of cards most of them on
    // its skin; a crown of clumps has a lumpy outline and gaps where the branches show, not one ball
    struct Clump {
        V3 at;
        float radius;
    };
    std::vector<Clump> clumps;
    for (const V3& t : tips) clumps.push_back(Clump{t, across * rnd.range(0.38f, 0.52f)});
    clumps.push_back(Clump{V3{c.x, c.y + r.y * 0.45f, c.z}, across * rnd.range(0.45f, 0.6f)});
    const int extra = shape.kind == Kind::Bush ? 5 : 2;
    for (int i = 0; i < extra; ++i) {
        const float az = rnd.range(0.0f, 6.2831853f), d = rnd.range(0.2f, 0.6f);
        clumps.push_back(Clump{c + V3{std::cos(az) * r.x * d, rnd.range(-0.3f, 0.5f) * r.y, std::sin(az) * r.z * d},
                               across * rnd.range(0.4f, 0.55f)});
    }
    for (Clump& k : clumps) {  // kept inside the model's crown
        k.at.x = std::clamp(k.at.x, c.x - r.x + k.radius * 0.6f, c.x + r.x - k.radius * 0.6f);
        k.at.z = std::clamp(k.at.z, c.z - r.z + k.radius * 0.6f, c.z + r.z - k.radius * 0.6f);
        k.at.y = std::clamp(k.at.y, std::max(c.y - r.y + k.radius * 0.5f, 0.3f), c.y + r.y - k.radius * 0.5f);
    }
    const int cards = int(std::clamp(across * across * 1.3f, 220.0f, 1000.0f));
    const float size = across * (shape.kind == Kind::Bush ? 0.24f : 0.2f);
    const float crownBottom = c.y - r.y, crownHeight = 2.0f * r.y;
    for (int i = 0; i < cards; ++i) {
        const Clump& k = clumps[size_t(rnd.next() * float(clumps.size())) % clumps.size()];
        V3 d{rnd.range(-1, 1), rnd.range(-1, 1), rnd.range(-1, 1)};
        const float l2 = dot(d, d);
        if (l2 > 1.0f || l2 < 1e-4f) {
            --i;
            continue;
        }
        d = norm(d) * (0.55f + 0.45f * std::sqrt(rnd.next()));  // most near the skin
        const V3 p = k.at + d * k.radius;
        const float depth = std::sqrt(dot(d, d));
        const V3 outward = norm(norm(p - c) * 0.6f + norm(d) * 0.4f);
        const V3 facing = norm(norm(d) + V3{0, 0.6f, 0} + V3{rnd.range(-0.4f, 0.4f), 0, rnd.range(-0.4f, 0.4f)});
        const float high = std::clamp((p.y - crownBottom) / crownHeight, 0.0f, 1.0f);
        const float open = std::clamp(0.2f + 0.5f * depth * (0.6f + 0.4f * std::max(0.0f, norm(d).y + 0.4f)) +
                                          0.35f * high, 0.0f, 1.0f);
        const V3 light = norm(outward * 0.85f + facing * 0.15f);
        const float droop = shape.kind == Kind::Willow ? -size * 0.6f * (1.0f - high) : 0.0f;
        b.card(p + V3{0, droop, 0}, facing, light, size * rnd.range(0.7f, 1.2f), rnd.range(0.0f, 6.2831853f), open,
               1.0f, rnd.next());
    }
    return m;
}

bool averageColour(const uint32_t* argb, int width, int height, float out[3]) {
    if (!argb || width <= 0 || height <= 0) return false;
    double sum[3] = {0, 0, 0};
    size_t n = 0;
    const size_t step = std::max<size_t>(1, size_t(width) * size_t(height) / 4096);
    for (size_t i = 0; i < size_t(width) * size_t(height); i += step) {
        const uint32_t p = argb[i];
        if ((p >> 24) < 128) continue;
        sum[0] += double((p >> 16) & 255);
        sum[1] += double((p >> 8) & 255);
        sum[2] += double(p & 255);
        ++n;
    }
    if (!n) return false;
    for (int k = 0; k < 3; ++k) out[k] = float(sum[k] / double(n) / 255.0);
    return true;
}

}  // namespace uagfx::forest
