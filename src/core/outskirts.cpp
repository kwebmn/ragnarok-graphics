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
constexpr int kBand = 6;            // 3.0.20-beta.42: the map's outer band of cells ...
constexpr float kHung = 0.9f;       // ... so much of it open: a map its makers hung in the sky (yuno, gonryun)

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

// What the ground is at a corner of its edge: the corners' height there, its window's mean height, the share of the
// window's cells under the water, its most common texture
struct EdgePoint {
    float exact = 0.0f, smooth = 0.0f, wet = 0.0f;
    int texture = -1;
    bool made = false;
};

// 3.0.20-beta.42 (the audit of the horizon): the land's plan. Beta.30's land began at the map's rectangle - but most
// maps keep one to three rings of cells without a top round their ground, and between the ground and the land lay a
// trench of nothing. The plan: which cells are the ground's (a top), which open ones the land fills (all past the
// rectangle, within it those reached from outside through open cells - a hole the ground closes round is the map's
// own), and for every corner as far as the land goes the nearest corner of the ground and how far it is (cells) - the
// land's heights, pictures and colours go by that corner, as beta.30's went by the rectangle's edge
// a cell none of whose corners lie under the map's water (up along -y: under it greater)
bool dryCell(const gnd::Cell& c, const Options& o) {
    if (!o.water) return true;
    for (const float h : c.h)
        if (h > o.waterLevel) return false;
    return true;
}

class Plan {
public:
    Plan(const gnd::Ground& g, const Options& o) : g_(g), o_(o), w_(g.width), h_(g.height) {
        r_ = ((std::max(o.ring, 0) + kChunkCells - 1) / kChunkCells) * kChunkCells + 2 * kChunkCells;
        side_ = w_ + 2 * r_ + 1;
        rows_ = h_ + 2 * r_ + 1;
        // the maps' filler round their edges (BACKSIDE.BMP, BLACK.BMP): never the outskirts' picture; 3.0.20-beta.48
        // its cells the land's own (top() takes them for open)
        backside_.resize(g.textures.size(), false);
        for (size_t t = 0; t < g.textures.size(); ++t) backside_[t] = fillerTexture(g.textures[t]);
        // the open cells reached from outside, and the outer band's share of open ones
        out_.assign(size_t(w_) * size_t(h_), 0);
        std::vector<int> stack;
        size_t band = 0, bandOpen = 0;
        for (int y = 0; y < h_; ++y)
            for (int x = 0; x < w_; ++x) {
                const bool open = !top(x, y);
                // hung in the sky by the cells with no top at all - a ring of filler is no sky
                if (x < kBand || y < kBand || x >= w_ - kBand || y >= h_ - kBand)
                    ++band, bandOpen += topped(x, y) ? 0 : 1;
                if (open && (x == 0 || y == 0 || x == w_ - 1 || y == h_ - 1)) {
                    out_[size_t(y) * size_t(w_) + size_t(x)] = 1;
                    stack.push_back(y * w_ + x);
                }
            }
        hung_ = band > 0 && float(bandOpen) >= kHung * float(band);
        while (!stack.empty()) {
            const int at = stack.back();
            stack.pop_back();
            const int x = at % w_, y = at / w_;
            const int nx[4] = {x - 1, x + 1, x, x}, ny[4] = {y, y, y - 1, y + 1};
            for (int k = 0; k < 4; ++k) {
                if (nx[k] < 0 || ny[k] < 0 || nx[k] >= w_ || ny[k] >= h_ || top(nx[k], ny[k])) continue;
                uint8_t& seen = out_[size_t(ny[k]) * size_t(w_) + size_t(nx[k])];
                if (seen) continue;
                seen = 1;
                stack.push_back(ny[k] * w_ + nx[k]);
            }
        }
        // 3.0.20-beta.48: a filler's cell the land's wherever it lies - one the ground closes round too
        for (int y = 0; y < h_; ++y)
            for (int x = 0; x < w_; ++x)
                if (topped(x, y) && !top(x, y)) out_[size_t(y) * size_t(w_) + size_t(x)] = 1;
        // every corner's nearest corner of the ground: the way to it (cells), two sweeps over the corners
        // (8SSEDT); a corner of the ground itself 0, 0
        way_.assign(size_t(side_) * size_t(rows_) * 2, kFar);
        for (int y = 0; y < h_; ++y)
            for (int x = 0; x < w_; ++x) {
                if (!top(x, y)) continue;
                any_ = true;
                for (int k = 0; k < 4; ++k) {
                    int16_t* v = wayAt(x + (k & 1), y + (k >> 1));
                    v[0] = v[1] = 0;
                }
            }
        if (!any_) return;
        const auto take = [&](int16_t* p, int i, int j, int oi, int oj) {
            const int ni = i + oi, nj = j + oj;
            if (ni < -r_ || nj < -r_ || ni > w_ + r_ || nj > h_ + r_) return;
            const int16_t* n = wayAt(ni, nj);
            if (n[0] == kFar) return;
            const int dx = n[0] + oi, dy = n[1] + oj;
            if (p[0] == kFar || dx * dx + dy * dy < int(p[0]) * p[0] + int(p[1]) * p[1])
                p[0] = int16_t(dx), p[1] = int16_t(dy);
        };
        for (int j = -r_; j <= h_ + r_; ++j) {
            for (int i = -r_; i <= w_ + r_; ++i) {
                int16_t* p = wayAt(i, j);
                take(p, i, j, -1, 0), take(p, i, j, 0, -1), take(p, i, j, -1, -1), take(p, i, j, 1, -1);
            }
            for (int i = w_ + r_; i >= -r_; --i) take(wayAt(i, j), i, j, 1, 0);
        }
        for (int j = h_ + r_; j >= -r_; --j) {
            for (int i = w_ + r_; i >= -r_; --i) {
                int16_t* p = wayAt(i, j);
                take(p, i, j, 1, 0), take(p, i, j, 0, 1), take(p, i, j, 1, 1), take(p, i, j, -1, 1);
            }
            for (int i = -r_; i <= w_ + r_; ++i) take(wayAt(i, j), i, j, -1, 0);
        }
        heights_.assign(size_t(side_) * size_t(rows_), kUnset);
        edges_.assign(size_t(w_ + 1) * size_t(h_ + 1), EdgePoint{});
        // a corner of the ground: its cells' own heights there (their mean where they meet at a step)
        exact_.assign(size_t(w_ + 1) * size_t(h_ + 1), 0.0f);
        for (int j = 0; j <= h_; ++j)
            for (int i = 0; i <= w_; ++i) {
                double sum = 0.0;
                int n = 0;
                for (int y = j - 1; y <= j; ++y)
                    for (int x = i - 1; x <= i; ++x)
                        if (const gnd::Cell* c = top(x, y)) sum += c->h[(i - x) + 2 * (j - y)], ++n;
                if (n) exact_[size_t(j) * size_t(w_ + 1) + size_t(i)] = float(sum / n);
            }
        // a texture's cells a picture: the mean over its cells
        std::map<int, std::pair<double, int>> spans;
        for (int y = 0; y < h_; ++y)
            for (int x = 0; x < w_; ++x) {
                const gnd::Cell* c = top(x, y);
                if (!c) continue;
                const gnd::Surface& s = g.surfaces[size_t(c->top)];
                const float span = 0.5f * (std::fabs(s.u[1] - s.u[0]) + std::fabs(s.v[2] - s.v[0]));
                if (span <= 0.0f) continue;
                spans[s.texture].first += span;
                ++spans[s.texture].second;
            }
        for (const auto& t : spans)
            spanOf_[t.first] = std::clamp(float(t.second.first / t.second.second), 0.0625f, 1.0f);
        // the ground's edge: its corners that are an open cell's the land fills too. 3.0.20-beta.31: a sea's map - a
        // quarter of them under the water at least (alberta 0.29, comodo 0.38, izlude 0.91); a river's or a lake's
        // at the edge (prt_fild08 0.17, pay_fild01 0.13) no sea: the edge all dry
        std::vector<uint32_t> rim;
        for (int j = 0; j <= h_; ++j)
            for (int i = 0; i <= w_; ++i) {
                const int16_t* v = wayAt(i, j);
                if (v[0] != 0 || v[1] != 0) continue;
                if (covered(i - 1, j - 1) || covered(i, j - 1) || covered(i - 1, j) || covered(i, j))
                    rim.push_back(uint32_t(j) * uint32_t(w_ + 1) + uint32_t(i));
            }
        size_t wet = 0;
        for (const uint32_t c : rim) {
            EdgePoint& e = edges_[c];
            e = make(int(c % uint32_t(w_ + 1)), int(c / uint32_t(w_ + 1)));
            wet += e.wet >= 0.5f ? 1 : 0;
        }
        wetShare_ = rim.empty() ? 0.0f : float(wet) / float(rim.size());
        sea_ = o.water && wetShare_ >= kSeaShare;
        double sum = 0.0;  // the dry corners' - the land far off never sinks under the water
        size_t dry = 0;
        for (const uint32_t c : rim) {
            EdgePoint& e = edges_[c];
            if (!sea_) e.wet = 0.0f;
            if (e.wet < 0.5f) sum += e.smooth, ++dry;
        }
        if (!dry)
            for (const uint32_t c : rim) sum += edges_[c].smooth, ++dry;
        meanHeight = dry ? float(sum / double(dry)) : 0.0f;
        settle();
    }

    bool any() const { return any_; }    // a cell of ground at all
    bool hung() const { return hung_; }  // in the sky by its makers: no land round it
    bool sea() const { return sea_; }
    float wetShare() const { return wetShare_; }
    int reach() const { return r_; }
    // an open cell the land fills: any past the rectangle, within it one reached from outside
    bool covered(int x, int y) const {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return true;
        return out_[size_t(y) * size_t(w_) + size_t(x)] != 0;
    }
    bool ground(int x, int y) const { return top(x, y) != nullptr; }
    // a corner's (as far as reach() off the rectangle; past it: the last one's) way to the ground, cells
    float distance(int i, int j) const {
        const int16_t* v = wayAt(clampI(i), clampJ(j));
        return std::sqrt(float(int(v[0]) * v[0] + int(v[1]) * v[1]));
    }
    // its nearest corner of the ground, row by row of width + 1
    uint32_t corner(int i, int j) const {
        i = clampI(i), j = clampJ(j);
        const int16_t* v = wayAt(i, j);
        return uint32_t(std::clamp(j + v[1], 0, h_)) * uint32_t(w_ + 1) + uint32_t(std::clamp(i + v[0], 0, w_));
    }
    const EdgePoint& edge(int i, int j) const {
        const uint32_t c = corner(i, j);
        EdgePoint& e = edges_[c];
        if (!e.made) {
            e = make(int(c % uint32_t(w_ + 1)), int(c / uint32_t(w_ + 1)));
            if (!sea_) e.wet = 0.0f;
        }
        return e;
    }
    // the land's height at a corner (up along -y): at the ground's edge the ground's own, a few cells on its edge's
    // smoothed, then the edge's mean, the hills, the mountains to the horizon
    float height(int i, int j) const {
        i = clampI(i), j = clampJ(j);
        float& kept = heights_[size_t(j + r_) * size_t(side_) + size_t(i + r_)];
        if (kept != kUnset) return kept;
        const float d = distance(i, j);
        // 3.0.20-beta.43: the sheet settled between the ground's edge and the edge's mean (settle) - beta.30's base
        // went by each corner's nearest corner of the edge alone, its ridges and valleys straight out
        float base = base_[size_t(j + r_) * size_t(side_) + size_t(i + r_)];
        if (d > 0.0f) {
            // the sheet's last cells eased onto the mean it ends at: no crease round the map kSettle cells off
            base += (meanHeight - base) * smoothBetween(0.6f * float(kSettle), float(kSettle), d);
            const EdgePoint& e = edge(i, j);
            // 3.0.20-beta.31: past a wet edge the sea's bed - flat, 15 units under the water's level at least
            if (o_.water) {
                const float bed = std::max(e.smooth, o_.waterLevel + 15.0f);
                base += (bed - base) * e.wet * smoothBetween(0.0f, 8.0f, d);
            }
            if (o_.relief) {
                const float cx = float(i), cy = float(j);
                const float hills = kHills * fbm(cx / 26.0f, cy / 26.0f, o_.seed) * smoothBetween(0.0f, 12.0f, d);
                const float mountains = kMountains * smoothBetween(40.0f, 240.0f, d) *
                                        (0.3f + 0.7f * ridged(cx / 110.0f, cy / 110.0f, o_.seed + 7u));
                base -= (hills + mountains) * (1.0f - e.wet);
            }
        }
        kept = base;
        return base;
    }
    float spanOf(int texture) const {
        const auto it = spanOf_.find(texture);
        return it == spanOf_.end() ? 0.25f : it->second;
    }
    float meanHeight = 0.0f;  // of the edge's dry corners' smoothed heights: the land far off settles to it

private:
    static constexpr int16_t kFar = 16000;
    static constexpr float kUnset = -3.0e38f;
    int clampI(int i) const { return std::clamp(i, -r_, w_ + r_); }
    int clampJ(int j) const { return std::clamp(j, -r_, h_ + r_); }
    int16_t* wayAt(int i, int j) { return &way_[(size_t(j + r_) * size_t(side_) + size_t(i + r_)) * 2]; }
    const int16_t* wayAt(int i, int j) const { return &way_[(size_t(j + r_) * size_t(side_) + size_t(i + r_)) * 2]; }
    // a cell with a top at all (the makers' filler too)
    bool topped(int x, int y) const {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return false;
        const gnd::Cell& c = g_.cells[size_t(y) * size_t(w_) + size_t(x)];
        return c.top >= 0 && size_t(c.top) < g_.surfaces.size();
    }
    // a cell of the ground: with a top that is not the makers' filler lying dry (3.0.20-beta.48, fillerCell's rule)
    const gnd::Cell* top(int x, int y) const {
        if (!topped(x, y)) return nullptr;
        const gnd::Cell& c = g_.cells[size_t(y) * size_t(w_) + size_t(x)];
        const int t = g_.surfaces[size_t(c.top)].texture;
        if (!(t >= 0 && size_t(t) < backside_.size() && backside_[size_t(t)])) return &c;
        return dryCell(c, o_) ? nullptr : &c;
    }
    // 3.0.20-beta.43: the land's base - a sheet between the ground's edge (its corners' own heights) and the edge's
    // mean kSettle cells off, every corner between the mean of its four neighbours: sweeps to and fro, each corner
    // taken past the mean of its neighbours (successive over-relaxation), from a first guess by the distance alone
    void settle() {
        base_.assign(size_t(side_) * size_t(rows_), meanHeight);
        std::vector<uint32_t> between;
        const int most = kSettle * kSettle;
        for (int j = -r_; j <= h_ + r_; ++j)
            for (int i = -r_; i <= w_ + r_; ++i) {
                const int16_t* v = wayAt(i, j);
                const int d2 = int(v[0]) * v[0] + int(v[1]) * v[1];
                if (d2 >= most) continue;
                const size_t at = size_t(j + r_) * size_t(side_) + size_t(i + r_);
                const float own = exact_[corner(i, j)];
                if (d2 == 0) {
                    base_[at] = own;
                    continue;
                }
                base_[at] = own + (meanHeight - own) * smoothBetween(0.0f, float(kSettle), std::sqrt(float(d2)));
                between.push_back(uint32_t(at));
            }
        constexpr int kSweeps = 120;
        constexpr float kOver = 1.9f;
        const ptrdiff_t row = side_;
        for (int sweep = 0; sweep < kSweeps; ++sweep) {
            const auto relax = [&](uint32_t at) {
                float* p = &base_[at];
                const float mean = 0.25f * (p[-1] + p[1] + p[-row] + p[row]);
                *p += kOver * (mean - *p);
            };
            if (sweep & 1)
                for (size_t k = between.size(); k-- > 0;) relax(between[k]);
            else
                for (const uint32_t at : between) relax(at);
        }
    }
    EdgePoint make(int x, int y) const {
        EdgePoint p;
        p.made = true;
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
    int w_ = 0, h_ = 0, r_ = 0, side_ = 0, rows_ = 0;
    std::vector<uint8_t> out_;      // a cell of the map row by row: 1 open and reached from outside
    std::vector<int16_t> way_;      // a corner's way to its nearest corner of the ground: x, y (cells)
    mutable std::vector<float> heights_;
    mutable std::vector<EdgePoint> edges_;  // by the ground's corners, made when first asked for
    std::vector<float> exact_;              // the ground's corners' own heights
    std::vector<float> base_;               // 3.0.20-beta.43: the land's base, every corner as far as it goes
    std::map<int, float> spanOf_;
    std::vector<bool> backside_;
    bool any_ = false, hung_ = false, sea_ = false;
    float wetShare_ = 0.0f;
};

struct Part {
    std::vector<terrain::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<int32_t> grid;  // a corner's vertex, -1 none yet
};

// 3.0.20-beta.31 (6 and 7: the sea past a map's wet edges), 3.0.20-beta.42 (over the open cells within the map too -
// its own water ends where its ground does): four quads round the rectangle, then the open cells within it the land
// fills, their rows' runs joined downwards
std::vector<float> seaOf(const gnd::Ground& g, const Options& o, const Plan& plan) {
    std::vector<float> out;
    if (!o.water || !plan.any() || plan.hung() || !plan.sea()) return out;
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
    struct Box {
        int x0, x1, y0, y1;  // cells: x0 .. x1 - 1, y0 .. y1 - 1
    };
    std::vector<Box> open, done;
    for (int y = 0; y <= g.height; ++y) {
        std::vector<Box> next;
        for (int x = 0; y < g.height && x < g.width;) {
            if (!plan.covered(x, y)) {
                ++x;
                continue;
            }
            int end = x;
            while (end < g.width && plan.covered(end, y)) ++end;
            Box b{x, end, y, y + 1};
            for (Box& before : open)
                if (before.x0 == b.x0 && before.x1 == b.x1 && before.y1 == y) {  // the same run a row up: one box
                    b.y0 = before.y0;
                    before.y1 = -1;  // taken on
                }
            next.push_back(b);
            x = end;
        }
        for (const Box& before : open)
            if (before.y1 >= 0) done.push_back(before);
        open.swap(next);
    }
    for (const Box& b : done)
        quad(x0 + float(b.x0) * g.zoom, z0 + float(b.y0) * g.zoom, x0 + float(b.x1) * g.zoom,
             z0 + float(b.y1) * g.zoom);
    return out;
}

}  // namespace

bool fillerCell(const gnd::Ground& g, const Options& o, int x, int y) {
    if (x < 0 || y < 0 || x >= g.width || y >= g.height) return false;
    const size_t at = size_t(y) * size_t(g.width) + size_t(x);
    if (at >= g.cells.size()) return false;
    const gnd::Cell& c = g.cells[at];
    if (c.top < 0 || size_t(c.top) >= g.surfaces.size()) return false;
    const int t = g.surfaces[size_t(c.top)].texture;
    return t >= 0 && size_t(t) < g.textures.size() && fillerTexture(g.textures[size_t(t)]) && dryCell(c, o);
}

std::vector<uint8_t> splitFiller(gnd::Ground& g, const Options& o) {
    const size_t before = g.textures.size();
    std::map<int32_t, int32_t> copies;  // a filler's surface -> its copy
    std::map<int, int> again;           // a filler's texture -> its own named again
    for (int y = 0; y < g.height; ++y)
        for (int x = 0; x < g.width; ++x) {
            if (!fillerCell(g, o, x, y)) continue;
            gnd::Cell& c = g.cells[size_t(y) * size_t(g.width) + size_t(x)];
            if (size_t(g.surfaces[size_t(c.top)].texture) >= before) continue;  // a copy's already
            auto copy = copies.find(c.top);
            if (copy == copies.end()) {
                gnd::Surface s = g.surfaces[size_t(c.top)];
                auto t = again.find(s.texture);
                if (t == again.end()) {
                    g.textures.push_back(g.textures[size_t(s.texture)]);
                    t = again.emplace(int(s.texture), int(g.textures.size() - 1)).first;
                }
                s.texture = int16_t(t->second);
                g.surfaces.push_back(s);
                copy = copies.emplace(c.top, int32_t(g.surfaces.size() - 1)).first;
            }
            c.top = copy->second;
        }
    if (g.textures.size() == before) return {};
    std::vector<uint8_t> own(g.textures.size(), 0);
    for (size_t t = before; t < g.textures.size(); ++t) own[t] = 1;
    return own;
}

bool fillerTexture(const std::string& name) {
    const size_t cut = name.find_last_of("\\/");
    std::string file = cut == std::string::npos ? name : name.substr(cut + 1);
    for (char& c : file) c = char(std::tolower(uint8_t(c)));
    return file == "backside.bmp" || file == "black.bmp";
}

float heightAt(const gnd::Ground& g, const Options& o, float cx, float cy) {
    const Plan plan(g, o);
    if (!plan.any()) return 0.0f;
    return plan.height(int(std::lround(cx)), int(std::lround(cy)));
}

std::vector<float> heightsOf(const gnd::Ground& g, const Options& o, int x0, int y0, int x1, int y1) {
    std::vector<float> out;
    const Plan plan(g, o);
    if (!plan.any() || x1 < x0 || y1 < y0) return out;
    out.reserve(size_t(x1 - x0 + 1) * size_t(y1 - y0 + 1));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) out.push_back(plan.height(x, y));
    return out;
}

std::vector<float> seaFrame(const gnd::Ground& g, const Options& o) {
    if (!o.water || g.width <= 0 || g.height <= 0 || g.cells.size() < size_t(g.width) * size_t(g.height)) return {};
    const Plan plan(g, o);
    return seaOf(g, o, plan);
}

terrain::Mesh build(const gnd::Ground& g, const terrain::Atlas& atlas, const terrain::Sun& sun, const Options& o,
                    std::vector<float>* sea) {
    terrain::Mesh m;
    if (sea) sea->clear();
    if (g.width <= 0 || g.height <= 0 || g.cells.size() < size_t(g.width) * size_t(g.height)) return m;
    const Plan plan(g, o);
    if (!plan.any() || plan.hung()) return m;  // 3.0.20-beta.42: a map hung in the sky keeps the sky round it
    if (sea) *sea = seaOf(g, o, plan);
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
            const int q = dmin < 32.0f ? 1 : dmin < 96.0f ? 2 : dmin < 192.0f ? 4 : dmin < 288.0f ? 8 : 16;
            const int n = kChunkCells / q;
            std::map<int, Part> parts;
            const auto vertexOf = [&](Part& part, int texture, int i, int j) -> uint32_t {
                int32_t& at = part.grid[size_t(j) * size_t(n + 1) + size_t(i)];
                if (at >= 0) return uint32_t(at);
                const int cx = ox + i * q, cy = oy + j * q;
                terrain::Vertex v{};
                v.pos[0] = (float(cx) - half[0]) * g.zoom;
                v.pos[1] = plan.height(cx, cy);
                v.pos[2] = (float(cy) - half[1]) * g.zoom;
                v.plain = v.pos[1];
                const float across = 2.0f * float(q) * g.zoom;
                const float dx = (plan.height(cx + q, cy) - plan.height(cx - q, cy)) / across;
                const float dz = (plan.height(cx, cy + q) - plan.height(cx, cy - q)) / across;
                const float len = std::sqrt(dx * dx + 1.0f + dz * dz);
                v.normal[0] = dx / len, v.normal[1] = -1.0f / len, v.normal[2] = dz / len;
                const float into[3] = {-v.normal[0], -v.normal[1], -v.normal[2]};
                const float light = float((terrain::lit(0xFFFFFFFFu, sun, into) >> 8) & 0xFFu);
                const float ratio = flatLight > 0.0f ? std::clamp(light / flatLight, 0.35f, 2.0f) : 1.0f;
                const uint32_t grey = uint32_t(std::clamp(ratio * 127.5f, 0.0f, 255.0f) + 0.5f);
                v.color = 0xFF000000u | grey << 16 | grey << 8 | grey;
                const float span = plan.spanOf(texture);
                v.uv[0] = float(cx) * span;
                v.uv[1] = float(cy) * span;
                v.lm[0] = lm[0], v.lm[1] = lm[1];
                // 3.0.20-beta.42: its corner of the ground and how far off, for its colour (the renderer's)
                const uint32_t off = uint32_t(std::clamp(std::lround(plan.distance(cx, cy) * 16.0f), 0L, 255L));
                v.blend = (plan.corner(cx, cy) & 0xFFFFFFu) | off << 24;
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
                    // the ground's own, or a hole it closes round (q 1 wherever a chunk touches the rectangle)
                    if (q == 1 && !plan.covered(x0, y0)) continue;
                    const float mx = float(x0) + 0.5f * float(q), my = float(y0) + 0.5f * float(q);
                    const float bx = std::clamp(mx, 0.0f, float(g.width)), by = std::clamp(my, 0.0f, float(g.height));
                    if (std::hypot(mx - bx, my - by) > float(o.ring)) continue;
                    // its picture: its corner's nearest to the ground (a larger one's middle)
                    int ci = x0 + q / 2, cj = y0 + q / 2;
                    if (q == 1) {
                        float least = 1e30f;
                        for (int k = 0; k < 4; ++k) {
                            const float d = plan.distance(x0 + (k & 1), y0 + (k >> 1));
                            if (d < least) least = d, ci = x0 + (k & 1), cj = y0 + (k >> 1);
                        }
                    }
                    const int texture = plan.edge(ci, cj).texture;
                    if (texture < 0) continue;
                    Part& part = parts[texture];
                    if (part.grid.empty()) part.grid.assign(size_t(n + 1) * size_t(n + 1), -1);
                    const uint32_t a = vertexOf(part, texture, i, j), b = vertexOf(part, texture, i + 1, j);
                    const uint32_t c = vertexOf(part, texture, i, j + 1), d = vertexOf(part, texture, i + 1, j + 1);
                    part.indices.insert(part.indices.end(), {a, b, c, b, d, c});
                    // the chunk's edges a skirt down; where the ground begins a curtain
                    const auto hang = [&](uint32_t p0, uint32_t p1, float down) {
                        const uint32_t u0 = under(part, p0, down), u1 = under(part, p1, down);
                        part.indices.insert(part.indices.end(), {p0, p1, u0, p1, u1, u0});
                    };
                    if (i == 0) hang(a, c, kSkirt);
                    if (i == n - 1) hang(b, d, kSkirt);
                    if (j == 0) hang(a, b, kSkirt);
                    if (j == n - 1) hang(c, d, kSkirt);
                    if (q == 1) {  // 3.0.20-beta.42: by the ground's own cells, wherever they lie
                        if (plan.ground(x0 - 1, y0)) hang(a, c, kCurtain);  // the ground west of it
                        if (plan.ground(x0 + 1, y0)) hang(b, d, kCurtain);  // east
                        if (plan.ground(x0, y0 - 1)) hang(a, b, kCurtain);  // north
                        if (plan.ground(x0, y0 + 1)) hang(c, d, kCurtain);  // south
                    }
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
