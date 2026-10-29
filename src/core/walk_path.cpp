#include "core/walk_path.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

namespace uagfx::walkpath {
namespace {

constexpr uint32_t kStep = 10, kDiagonal = 14;  // tenths of a cell

// The eight ways and their costs
constexpr int kDx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
constexpr int kDy[8] = {0, 0, 1, -1, 1, -1, 1, -1};

// A diagonal move from (x, y) by (dx, dy) keeps clear of a wall's corner: both cells beside it walkable
bool diagonalOk(const Cells& c, int x, int y, int dx, int dy) { return c.at(x + dx, y) && c.at(x, y + dy); }

}  // namespace

Cells cellsOf(const walkgrid::Grid& g) {
    Cells c;
    c.w = g.w;
    c.h = g.h;
    c.walk.assign(size_t(std::max(0, g.w)) * size_t(std::max(0, g.h)), 0);
    if (g.rows.size() != size_t(g.h) + 1) return c;
    for (int y = 0; y < g.h; ++y) {
        int x = 0;
        for (uint32_t r = g.rows[size_t(y)]; r < g.rows[size_t(y) + 1] && r < g.runs.size(); ++r) {
            const auto& [length, part] = g.runs[r];
            for (int i = 0; i < length && x < g.w; ++i, ++x)
                c.walk[size_t(y) * size_t(g.w) + size_t(x)] = part != walkgrid::kWall ? 1 : 0;
        }
    }
    return c;
}

Field fieldTo(const Cells& c, Cell goal, int reach) {
    Field f;
    f.w = c.w;
    f.h = c.h;
    f.goal = goal;
    f.dist.assign(size_t(std::max(0, c.w)) * size_t(std::max(0, c.h)), kFar);
    if (c.w <= 0 || c.h <= 0) return f;
    using Item = std::pair<uint32_t, uint32_t>;  // distance, cell
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    const auto seed = [&](int x, int y, uint32_t d) {
        if (!c.at(x, y)) return;
        const size_t i = size_t(y) * size_t(c.w) + size_t(x);
        if (d >= f.dist[i]) return;
        f.dist[i] = d;
        open.push(Item{d, uint32_t(i)});
    };
    if (c.at(goal.first, goal.second)) {
        seed(goal.first, goal.second, 0);
    } else {  // a goal on a wall: the walkable cells round it, each its own way off it
        for (int dy = -reach; dy <= reach; ++dy)
            for (int dx = -reach; dx <= reach; ++dx)
                seed(goal.first + dx, goal.second + dy,
                     uint32_t(std::lround(std::sqrt(float(dx * dx + dy * dy)) * float(kStep))));
    }
    while (!open.empty()) {
        const auto [d, i] = open.top();
        open.pop();
        if (d != f.dist[i]) continue;  // a nearer way to it came first
        const int x = int(i % uint32_t(c.w)), y = int(i / uint32_t(c.w));
        for (int k = 0; k < 8; ++k) {
            const int nx = x + kDx[k], ny = y + kDy[k];
            if (!c.at(nx, ny) || (k >= 4 && !diagonalOk(c, x, y, kDx[k], kDy[k]))) continue;
            const uint32_t nd = d + (k >= 4 ? kDiagonal : kStep);
            const size_t ni = size_t(ny) * size_t(c.w) + size_t(nx);
            if (nd < f.dist[ni]) {
                f.dist[ni] = nd;
                open.push(Item{nd, uint32_t(ni)});
            }
        }
    }
    return f;
}

std::vector<Cell> pathFrom(const Field& f, const Cells& c, Cell from) {
    std::vector<Cell> out;
    if (f.w != c.w || f.h != c.h || f.dist.empty()) return out;
    Cell at = from;
    if (c.at(from.first, from.second) && f.at(from.first, from.second) == kFar) return out;  // a part apart
    if (f.at(at.first, at.second) == kFar) {  // off the walked cells: its nearest neighbour that reaches the goal
        uint32_t best = kFar;
        for (int k = 0; k < 8; ++k) {
            const uint32_t d = f.at(from.first + kDx[k], from.second + kDy[k]);
            if (d < best) {
                best = d;
                at = Cell{from.first + kDx[k], from.second + kDy[k]};
            }
        }
        if (best == kFar) return out;
    }
    out.push_back(at);
    for (size_t guard = 0; guard < f.dist.size() && f.at(at.first, at.second) > 0; ++guard) {
        uint32_t best = f.at(at.first, at.second);
        Cell next = at;
        for (int k = 0; k < 8; ++k) {
            const int nx = at.first + kDx[k], ny = at.second + kDy[k];
            if (k >= 4 && !diagonalOk(c, at.first, at.second, kDx[k], kDy[k])) continue;
            const uint32_t d = f.at(nx, ny);
            if (d < best) {
                best = d;
                next = Cell{nx, ny};
            }
        }
        if (next == at) break;  // a seed round a goal on a wall: its ground reached
        at = next;
        out.push_back(at);
    }
    return out;
}

bool clear(const Cells& c, Cell a, Cell b) {
    // the cells a line between the two middles passes through (a grid walk), each walkable; where it passes a corner
    // exactly, both cells beside it
    int x = a.first, y = a.second;
    const int dx = b.first - a.first, dy = b.second - a.second;
    const int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
    const int nx = std::abs(dx), ny = std::abs(dy);
    if (!c.at(x, y)) return false;
    for (int ix = 0, iy = 0; ix < nx || iy < ny;) {
        // which border comes first: (1 + 2 ix) / nx against (1 + 2 iy) / ny, in whole numbers
        const long long cmp = (1LL + 2 * ix) * ny - (1LL + 2 * iy) * nx;
        if (cmp == 0) {  // through a corner: both of its sides
            if (!c.at(x + sx, y) || !c.at(x, y + sy)) return false;
            x += sx;
            y += sy;
            ++ix;
            ++iy;
        } else if (cmp < 0) {
            x += sx;
            ++ix;
        } else {
            y += sy;
            ++iy;
        }
        if (!c.at(x, y)) return false;
    }
    return true;
}

std::vector<Cell> straightened(const Cells& c, const std::vector<Cell>& path) {
    if (path.size() <= 2) return path;
    std::vector<Cell> out{path.front()};
    size_t i = 0;
    while (i + 1 < path.size()) {
        size_t j = i + 1;
        while (j + 1 < path.size() && clear(c, path[i], path[j + 1])) ++j;  // as far as a straight line goes
        out.push_back(path[j]);
        i = j;
    }
    return out;
}

OnWay onWay(const std::vector<std::pair<float, float>>& way, float x, float y, float from, float back, float ahead) {
    OnWay best;
    if (way.empty()) return best;
    best.x = way[0].first;
    best.y = way[0].second;
    best.off = std::hypot(x - best.x, y - best.y);
    const float lo = from - back, hi = from + ahead;
    float run = 0;  // the way's length up to the leg's start
    bool found = false;
    for (size_t i = 0; i + 1 < way.size(); ++i) {
        const float ax = way[i].first, ay = way[i].second, dx = way[i + 1].first - ax, dy = way[i + 1].second - ay;
        const float len = std::hypot(dx, dy);
        if (run + len < lo || run > hi || len < 1e-6f) {
            run += len;
            continue;
        }
        float t = ((x - ax) * dx + (y - ay) * dy) / (len * len);  // along the leg, kept within it and the window
        t = std::clamp(t, std::max(0.0f, (lo - run) / len), std::min(1.0f, (hi - run) / len));
        const float px = ax + dx * t, py = ay + dy * t, off = std::hypot(x - px, y - py);
        if (!found || off < best.off - 1e-4f) {
            found = true;
            best.at = run + len * t;
            best.off = off;
            best.leg = i;
            best.x = px;
            best.y = py;
        }
        run += len;
    }
    return best;
}

std::vector<Cell> rest(const std::vector<Cell>& way, Cell player, const OnWay& p) {
    std::vector<Cell> out{player};
    for (size_t i = p.leg + 1; i < way.size(); ++i)
        if (way[i] != out.back()) out.push_back(way[i]);
    return out;
}

bool keep(Kept& k, const Field& f, const Cells& c, Cell player, float off) {
    if (k.goal == f.goal && k.way.size() >= 2) {
        std::vector<std::pair<float, float>> corners;
        for (const Cell& w : k.way) corners.emplace_back(float(w.first), float(w.second));
        const OnWay p = onWay(corners, float(player.first), float(player.second), k.at, kBack, kAhead);
        if (p.off >= 0 && p.off <= off) {
            k.at = p.at;
            k.off = p.off;
            k.rest = rest(k.way, player, p);
            return false;
        }
    }
    k.way = straightened(c, pathFrom(f, c, player));
    if (k.way.size() < 2 && player != f.goal) k.way = {player, f.goal};  // none over the cells: straight to it
    k.goal = f.goal;
    k.at = 0;
    k.off = 0;
    k.rest = k.way;
    return true;
}

}  // namespace uagfx::walkpath
