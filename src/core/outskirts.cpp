#include "core/outskirts.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace uagfx::outskirts {
namespace {

constexpr float kHills = 14.0f, kMountains = 380.0f;  // units up
constexpr float kSkirt = 10.0f, kCurtain = 8.0f;      // units down
constexpr float kSeaShare = 0.25f;  // 3.0.20-beta.31: the edge's corners under the water a sea's map has

float hash2(int x, int y, uint32_t seed) {
    uint32_t h = uint32_t(x) * 0x8DA6B343u ^ uint32_t(y) * 0xD8163841u ^ (seed + 0x9E3779B9u) * 0xCB1AB31Fu;
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    h ^= h >> 15;
    return float(h & 0xFFFFFFu) / 16777216.0f;
}

float smooth01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float smoothBetween(float a, float b, float x) { return smooth01((x - a) / (b - a)); }

float valueNoise(float x, float y, uint32_t seed) {
    const float fx0 = std::floor(x), fy0 = std::floor(y);
    const int ix = int(fx0), iy = int(fy0);
    const float fx = smooth01(x - fx0), fy = smooth01(y - fy0);
    const float a = hash2(ix, iy, seed), b = hash2(ix + 1, iy, seed), c = hash2(ix, iy + 1, seed),
                d = hash2(ix + 1, iy + 1, seed);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

float fbm(float x, float y, uint32_t seed) {  // 0-1
    float sum = 0.0f, amp = 1.0f, total = 0.0f, f = 1.0f;
    for (int k = 0; k < 4; ++k, amp *= 0.5f, f *= 2.0f) {
        sum += amp * valueNoise(x * f, y * f, seed + uint32_t(k) * 101u);
        total += amp;
    }
    return sum / total;
}

float ridged(float x, float y, uint32_t seed) {  // 0-1, sharp ridges
    float sum = 0.0f, amp = 1.0f, total = 0.0f, f = 1.0f;
    for (int k = 0; k < 4; ++k, amp *= 0.5f, f *= 2.0f) {
        const float n = 1.0f - std::fabs(2.0f * valueNoise(x * f, y * f, seed + uint32_t(k) * 211u) - 1.0f);
        sum += amp * n * n;
        total += amp;
    }
    return sum / total;
}

// What the edge is at a corner of it: the corners' height there, its window's mean height, the share of the window's
// cells under the water, its most common texture
struct EdgePoint {
    float exact = 0.0f, smooth = 0.0f, wet = 0.0f;
    int texture = -1;
};

class Edge {
public:
    Edge(const gnd::Ground& g, const Options& o) : g_(g), o_(o) {
        // the maps' filler round their edges (BACKSIDE.bmp): never the outskirts' picture
        backside_.resize(g.textures.size(), false);
        for (size_t t = 0; t < g.textures.size(); ++t) {
            std::string low = g.textures[t];
            for (char& c : low) c = char(std::tolower(uint8_t(c)));
            backside_[t] = low.find("backside") != std::string::npos;
        }
        points_.resize(size_t(2 * (g.width + g.height)));
        for (int x = 0; x <= g.width; ++x) {
            points_[indexOf(x, 0)] = make(x, 0);
            points_[indexOf(x, g.height)] = make(x, g.height);
        }
        for (int y = 0; y <= g.height; ++y) {
            points_[indexOf(0, y)] = make(0, y);
            points_[indexOf(g.width, y)] = make(g.width, y);
        }
        // a texture's cells a picture: the mean over its cells
        std::map<int, std::pair<double, int>> spans;
        for (int y = 0; y < g.height; ++y)
            for (int x = 0; x < g.width; ++x) {
                const gnd::Cell& c = g.cells[size_t(y) * size_t(g.width) + size_t(x)];
                if (c.top < 0 || size_t(c.top) >= g.surfaces.size()) continue;
                const gnd::Surface& s = g.surfaces[size_t(c.top)];
                const float span = 0.5f * (std::fabs(s.u[1] - s.u[0]) + std::fabs(s.v[2] - s.v[0]));
                if (span <= 0.0f) continue;
                spans[s.texture].first += span;
                ++spans[s.texture].second;
            }
        for (const auto& t : spans)
            spanOf_[t.first] = std::clamp(float(t.second.first / t.second.second), 0.0625f, 1.0f);
        // 3.0.20-beta.31: a sea's map - a quarter of its edge under the water at least (alberta 0.29, comodo 0.38,
        // izlude 0.91); a river's or a lake's at the edge (prt_fild08 0.17, pay_fild01 0.13) no sea: the edge all dry
        sea_ = wetShare() >= kSeaShare;
        if (!sea_)
            for (EdgePoint& e : points_) e.wet = 0.0f;
        double sum = 0.0;  // the dry corners' - the land far off never sinks under the water
        size_t dry = 0;
        for (const EdgePoint& e : points_)
            if (e.wet < 0.5f) sum += e.smooth, ++dry;
        if (!dry)
            for (const EdgePoint& e : points_) sum += e.smooth, ++dry;
        meanHeight = dry ? float(sum / double(dry)) : 0.0f;
    }
    // a place on the border, between its corners
    EdgePoint at(float bx, float by) const {
        const bool side = bx <= 0.0f || bx >= float(g_.width);  // x fixed: along y
        const float along = side ? by : bx;
        const float fixed = side ? bx : by;
        const int lo = std::clamp(int(std::floor(along)), 0, side ? g_.height : g_.width);
        const int hi = std::min(lo + 1, side ? g_.height : g_.width);
        const float t = std::clamp(along - float(lo), 0.0f, 1.0f);
        const int f = int(std::lround(fixed));
        const EdgePoint& a = points_[side ? indexOf(f, lo) : indexOf(lo, f)];
        const EdgePoint& b = points_[side ? indexOf(f, hi) : indexOf(hi, f)];
        EdgePoint p;
        p.exact = a.exact + (b.exact - a.exact) * t;
        p.smooth = a.smooth + (b.smooth - a.smooth) * t;
        p.wet = a.wet + (b.wet - a.wet) * t;
        p.texture = t < 0.5f ? a.texture : b.texture;
        return p;
    }
    float spanOf(int texture) const {
        const auto it = spanOf_.find(texture);
        return it == spanOf_.end() ? 0.25f : it->second;
    }

private:
    size_t indexOf(int x, int y) const {  // round the border, clockwise from the first corner
        const int w = g_.width, h = g_.height;
        if (y <= 0) return size_t(std::clamp(x, 0, w));
        if (x >= w) return size_t(w + std::clamp(y, 0, h));
        if (y >= h) return size_t(w + h + (w - std::clamp(x, 0, w)));
        return size_t((2 * w + h + (h - std::clamp(y, 0, h))) % (2 * (w + h)));
    }
    const gnd::Cell* top(int x, int y) const {
        if (x < 0 || y < 0 || x >= g_.width || y >= g_.height) return nullptr;
        const gnd::Cell& c = g_.cells[size_t(y) * size_t(g_.width) + size_t(x)];
        return c.top >= 0 && size_t(c.top) < g_.surfaces.size() ? &c : nullptr;
    }
    EdgePoint make(int x, int y) const {
        EdgePoint p;
        double exact = 0.0;
        int n = 0;
        for (int j = y - 1; j <= y; ++j)
            for (int i = x - 1; i <= x; ++i)
                if (const gnd::Cell* c = top(i, j)) {
                    exact += c->h[(x - i) + 2 * (y - j)];
                    ++n;
                }
        double sum = 0.0;
        int cells = 0, wet = 0;
        for (int j = y - kWindow; j < y + kWindow; ++j)
            for (int i = x - kWindow; i < x + kWindow; ++i)
                if (const gnd::Cell* c = top(i, j)) {
                    const float mean = 0.25f * (c->h[0] + c->h[1] + c->h[2] + c->h[3]);
                    sum += mean;
                    ++cells;
                    wet += o_.water && mean > o_.waterLevel ? 1 : 0;
                }
        p.smooth = cells ? float(sum / cells) : 0.0f;
        p.exact = n ? float(exact / n) : p.smooth;
        p.wet = cells ? float(wet) / float(cells) : 0.0f;
        // its picture: the most common one round it but the filler, further in when there is none of another near
        for (int reach = kWindow; reach <= kWindow * 8 && p.texture < 0; reach *= 2) {
            std::map<int, int> textures;
            for (int j = y - reach; j < y + reach; ++j)
                for (int i = x - reach; i < x + reach; ++i)
                    if (const gnd::Cell* c = top(i, j)) {
                        const int t = g_.surfaces[size_t(c->top)].texture;
                        if (t >= 0 && (size_t(t) >= backside_.size() || !backside_[size_t(t)])) ++textures[t];
                    }
            int most = 0;
            for (const auto& t : textures)
                if (t.second > most) most = t.second, p.texture = t.first;
        }
        return p;
    }
    const gnd::Ground& g_;
    const Options& o_;
    std::vector<EdgePoint> points_;
    std::map<int, float> spanOf_;
    std::vector<bool> backside_;
    bool sea_ = false;

public:
    float meanHeight = 0.0f;  // of the edge's dry corners' smoothed heights: the land far off settles to it
    bool sea() const { return sea_; }
    float wetShare() const {  // the edge's corners under the water, of all
        size_t wet = 0;
        for (const EdgePoint& e : points_) wet += e.wet >= 0.5f ? 1 : 0;
        return points_.empty() ? 0.0f : float(wet) / float(points_.size());
    }
};

float heightOf(const gnd::Ground& g, const Options& o, const Edge& edge, float cx, float cy, float* wet = nullptr,
               int* texture = nullptr) {
    const float bx = std::clamp(cx, 0.0f, float(g.width)), by = std::clamp(cy, 0.0f, float(g.height));
    const float d = std::hypot(cx - bx, cy - by);
    const EdgePoint e = edge.at(bx, by);
    if (wet) *wet = e.wet;
    if (texture) *texture = e.texture;
    // the edge's own at it, smoothed a few cells on; 3.0.20-beta.30: the edge's mean further off - its ridges and
    // valleys went on straight out to the horizon
    float base = e.exact + (e.smooth - e.exact) * smoothBetween(0.0f, 8.0f, d);
    base += (edge.meanHeight - base) * smoothBetween(8.0f, 60.0f, d);
    // 3.0.20-beta.31: past a wet edge the sea's bed - flat, 15 units under the water's level at least
    if (o.water) {
        const float bed = std::max(e.smooth, o.waterLevel + 15.0f);
        base += (bed - base) * e.wet * smoothBetween(0.0f, 8.0f, d);
    }
    const float hills = kHills * fbm(cx / 26.0f, cy / 26.0f, o.seed) * smoothBetween(0.0f, 12.0f, d);
    const float mountains =
        kMountains * smoothBetween(40.0f, 240.0f, d) * (0.3f + 0.7f * ridged(cx / 110.0f, cy / 110.0f, o.seed + 7u));
    return base - (hills + mountains) * (1.0f - e.wet);
}

struct Part {
    std::vector<terrain::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<int32_t> grid;  // a corner's vertex, -1 none yet
};

}  // namespace

float heightAt(const gnd::Ground& g, const Options& o, float cx, float cy) {
    const Edge edge(g, o);
    return heightOf(g, o, edge, cx, cy);
}

std::vector<float> seaFrame(const gnd::Ground& g, const Options& o) {
    std::vector<float> out;
    if (!o.water || g.width <= 0 || g.height <= 0) return out;
    const Edge edge(g, o);
    if (!edge.sea()) return out;
    const float x0 = -0.5f * float(g.width) * g.zoom, x1 = -x0, z0 = -0.5f * float(g.height) * g.zoom, z1 = -z0;
    const float r = float(o.ring) * g.zoom, X0 = x0 - r, X1 = x1 + r, Z0 = z0 - r, Z1 = z1 + r;
    const auto quad = [&](float ax, float az, float bx, float bz) {
        const float c[4][2] = {{ax, az}, {bx, az}, {ax, bz}, {bx, bz}};
        for (const int k : {0, 1, 2, 1, 3, 2}) out.insert(out.end(), {c[k][0], o.waterLevel, c[k][1]});
    };
    quad(X0, Z0, X1, z0);  // north
    quad(X0, z1, X1, Z1);  // south
    quad(X0, z0, x0, z1);  // west
    quad(x1, z0, X1, z1);  // east
    return out;
}

terrain::Mesh build(const gnd::Ground& g, const terrain::Atlas& atlas, const terrain::Sun& sun, const Options& o) {
    terrain::Mesh m;
    if (g.width <= 0 || g.height <= 0 || g.cells.size() < size_t(g.width) * size_t(g.height)) return m;
    const Edge edge(g, o);
    float lm[2];
    terrain::lightmapUv(atlas, terrain::meanLightCell(g), 0.5f, 0.5f, lm);
    const float half[2] = {float(g.width) / 2.0f, float(g.height) / 2.0f};
    const float flat[3] = {0.0f, 1.0f, 0.0f};  // the game's normal of a flat top: into the ground
    const float flatLight = float((terrain::lit(0xFFFFFFFFu, sun, flat) >> 8) & 0xFFu);
    const int start = -((o.ring + kChunkCells - 1) / kChunkCells) * kChunkCells;
    for (int oy = start; oy < g.height + o.ring; oy += kChunkCells)
        for (int ox = start; ox < g.width + o.ring; ox += kChunkCells) {
            const float gapX = float(std::max({0, ox - g.width, -(ox + kChunkCells)}));
            const float gapY = float(std::max({0, oy - g.height, -(oy + kChunkCells)}));
            const float dmin = std::hypot(gapX, gapY);
            if (dmin > float(o.ring)) continue;
            if (ox >= 0 && oy >= 0 && ox + kChunkCells <= g.width && oy + kChunkCells <= g.height) continue;
            const int q = dmin < 32.0f ? 1 : dmin < 96.0f ? 2 : dmin < 192.0f ? 4 : dmin < 288.0f ? 8 : 16;
            const int n = kChunkCells / q;
            std::map<int, Part> parts;
            const auto vertexOf = [&](Part& part, int texture, int i, int j) -> uint32_t {
                int32_t& at = part.grid[size_t(j) * size_t(n + 1) + size_t(i)];
                if (at >= 0) return uint32_t(at);
                const float cx = float(ox + i * q), cy = float(oy + j * q);
                terrain::Vertex v{};
                v.pos[0] = (cx - half[0]) * g.zoom;
                v.pos[1] = heightOf(g, o, edge, cx, cy);
                v.pos[2] = (cy - half[1]) * g.zoom;
                v.plain = v.pos[1];
                const float e = 0.5f * float(q);
                const float across = 2.0f * e * g.zoom;
                const float dx = (heightOf(g, o, edge, cx + e, cy) - heightOf(g, o, edge, cx - e, cy)) / across;
                const float dz = (heightOf(g, o, edge, cx, cy + e) - heightOf(g, o, edge, cx, cy - e)) / across;
                const float len = std::sqrt(dx * dx + 1.0f + dz * dz);
                v.normal[0] = dx / len, v.normal[1] = -1.0f / len, v.normal[2] = dz / len;
                const float into[3] = {-v.normal[0], -v.normal[1], -v.normal[2]};
                const float light = float((terrain::lit(0xFFFFFFFFu, sun, into) >> 8) & 0xFFu);
                const float ratio = flatLight > 0.0f ? std::clamp(light / flatLight, 0.35f, 2.0f) : 1.0f;
                const uint32_t grey = uint32_t(std::clamp(ratio * 127.5f, 0.0f, 255.0f) + 0.5f);
                v.color = 0xFF000000u | grey << 16 | grey << 8 | grey;
                const float span = edge.spanOf(texture);
                v.uv[0] = cx * span;
                v.uv[1] = cy * span;
                v.lm[0] = lm[0], v.lm[1] = lm[1];
                at = int32_t(part.vertices.size());
                part.vertices.push_back(v);
                return uint32_t(at);
            };
            // a vertex hanging `down` units under another (a skirt's, a curtain's)
            const auto under = [&](Part& part, uint32_t from, float down) {
                terrain::Vertex v = part.vertices[from];
                v.pos[1] += down;
                v.plain = v.pos[1];
                part.vertices.push_back(v);
                return uint32_t(part.vertices.size() - 1);
            };
            for (int j = 0; j < n; ++j)
                for (int i = 0; i < n; ++i) {
                    const int x0 = ox + i * q, y0 = oy + j * q;
                    if (x0 >= 0 && y0 >= 0 && x0 + q <= g.width && y0 + q <= g.height) continue;  // the map's own
                    const float mx = float(x0) + 0.5f * float(q), my = float(y0) + 0.5f * float(q);
                    const float bx = std::clamp(mx, 0.0f, float(g.width)), by = std::clamp(my, 0.0f, float(g.height));
                    if (std::hypot(mx - bx, my - by) > float(o.ring)) continue;
                    float wet = 0.0f;
                    int texture = -1;
                    heightOf(g, o, edge, mx, my, &wet, &texture);
                    if (texture < 0) continue;
                    Part& part = parts[texture];
                    if (part.grid.empty()) part.grid.assign(size_t(n + 1) * size_t(n + 1), -1);
                    const uint32_t a = vertexOf(part, texture, i, j), b = vertexOf(part, texture, i + 1, j);
                    const uint32_t c = vertexOf(part, texture, i, j + 1), d = vertexOf(part, texture, i + 1, j + 1);
                    part.indices.insert(part.indices.end(), {a, b, c, b, d, c});
                    // the chunk's edges a skirt down, the map's edge a curtain
                    const auto hang = [&](uint32_t p0, uint32_t p1, float down) {
                        const uint32_t u0 = under(part, p0, down), u1 = under(part, p1, down);
                        part.indices.insert(part.indices.end(), {p0, p1, u0, p1, u1, u0});
                    };
                    if (i == 0) hang(a, c, kSkirt);
                    if (i == n - 1) hang(b, d, kSkirt);
                    if (j == 0) hang(a, b, kSkirt);
                    if (j == n - 1) hang(c, d, kSkirt);
                    const bool alongY = y0 >= 0 && y0 + q <= g.height, alongX = x0 >= 0 && x0 + q <= g.width;
                    if (alongY && x0 + q == 0) hang(b, d, kCurtain);    // west of the map: its east side
                    if (alongY && x0 == g.width) hang(a, c, kCurtain);  // east of it: its west side
                    if (alongX && y0 + q == 0) hang(c, d, kCurtain);    // north
                    if (alongX && y0 == g.height) hang(a, b, kCurtain);  // south
                }
            if (parts.empty()) continue;
            terrain::Chunk ch;
            ch.cellX = ox;
            ch.cellY = oy;
            ch.firstVertex = uint32_t(m.vertices.size());
            ch.firstBatch = uint32_t(m.batches.size());
            for (int k = 0; k < 3; ++k) ch.lo[k] = 1e30f, ch.hi[k] = -1e30f;
            for (const auto& t : parts) {
                terrain::Batch batch;
                batch.texture = t.first;
                batch.layer = terrain::kGround;
                batch.firstVertex = uint32_t(m.vertices.size());
                batch.vertexCount = uint32_t(t.second.vertices.size());
                batch.firstIndex = uint32_t(m.indices.size());
                batch.indexCount = uint32_t(t.second.indices.size());
                for (const terrain::Vertex& v : t.second.vertices)
                    for (int k = 0; k < 3; ++k)
                        ch.lo[k] = std::min(ch.lo[k], v.pos[k]), ch.hi[k] = std::max(ch.hi[k], v.pos[k]);
                m.vertices.insert(m.vertices.end(), t.second.vertices.begin(), t.second.vertices.end());
                m.indices.insert(m.indices.end(), t.second.indices.begin(), t.second.indices.end());
                m.batches.push_back(batch);
            }
            ch.vertexCount = uint32_t(m.vertices.size()) - ch.firstVertex;
            ch.batchCount = uint32_t(m.batches.size()) - ch.firstBatch;
            m.chunks.push_back(ch);
        }
    return m;
}

}  // namespace uagfx::outskirts
