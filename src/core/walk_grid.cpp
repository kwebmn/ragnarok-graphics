#include "core/walk_grid.hpp"

#include <algorithm>
#include <map>

#include "core/reference_index.hpp"

namespace uagfx::walkgrid {
namespace {

constexpr char kMagic[] = "UAWALK1\n";

uint32_t le(const std::string& s, size_t at, int bytes) {
    uint32_t v = 0;
    for (int i = bytes - 1; i >= 0; --i) v = v << 8 | static_cast<unsigned char>(s[at + size_t(i)]);
    return v;
}

void put(std::string& s, uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) s.push_back(char((v >> (8 * i)) & 0xFF));
}

}  // namespace

uint8_t Grid::at(int x, int y) const {
    if (x < 0 || y < 0 || x >= w || y >= h || rows.size() != size_t(h) + 1) return kWall;
    int left = x;
    for (uint32_t r = rows[size_t(y)]; r < rows[size_t(y) + 1]; ++r) {
        if (left < runs[r].first) return runs[r].second;
        left -= runs[r].first;
    }
    return kWall;
}

std::vector<uint8_t> Grid::around(int x, int y, int most) const {
    std::vector<uint8_t> found;
    auto look = [&](int xx, int yy) {
        const uint8_t p = at(xx, yy);
        if (p != kWall && std::find(found.begin(), found.end(), p) == found.end()) found.push_back(p);
    };
    look(x, y);
    for (int r = 1; r <= most && found.empty(); ++r) {  // the ring alone: its rows, then its columns between
        for (int xx = x - r; xx <= x + r; ++xx) {
            look(xx, y - r);
            look(xx, y + r);
        }
        for (int yy = y - r + 1; yy <= y + r - 1; ++yy) {
            look(x - r, yy);
            look(x + r, yy);
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

bool read(const std::string& file, Grids& out) {
    std::string data;
    if (file.compare(0, 8, kMagic) == 0) data = file;  // the payload itself (the tests)
    else if (!refindex::gunzip(file, data)) return false;
    if (data.size() < 12 || data.compare(0, 8, kMagic) != 0) return false;
    const uint32_t count = le(data, 8, 4);
    size_t at = 12;
    Grids maps;
    for (uint32_t m = 0; m < count; ++m) {
        if (at >= data.size()) return false;
        const size_t n = static_cast<unsigned char>(data[at]);
        if (at + 1 + n + 8 > data.size()) return false;
        const std::string name = data.substr(at + 1, n);
        at += 1 + n;
        Grid g;
        g.w = int(le(data, at, 2));
        g.h = int(le(data, at + 2, 2));
        const uint32_t runs = le(data, at + 4, 4);
        at += 8;
        if (at + size_t(runs) * 3 > data.size()) return false;
        g.runs.reserve(runs);
        g.rows.reserve(size_t(g.h) + 1);
        g.rows.push_back(0);
        int x = 0;
        for (uint32_t r = 0; r < runs; ++r, at += 3) {
            const uint16_t length = uint16_t(le(data, at, 2));
            const uint8_t part = static_cast<uint8_t>(data[at + 2]);
            if (length == 0 || x + length > g.w) return false;  // a run within its row
            g.runs.emplace_back(length, part);
            if (part != kWall) g.partCount = std::max(g.partCount, part + 1);
            x += length;
            if (x == g.w) {
                g.rows.push_back(r + 1);
                x = 0;
            }
        }
        if (x != 0 || g.rows.size() != size_t(g.h) + 1) return false;
        maps.emplace(name, std::move(g));
    }
    out = std::move(maps);
    return true;
}

Grid fromCells(int w, int h, const std::vector<uint8_t>& walkable) {
    std::vector<int> label(size_t(w) * size_t(h), -1);
    int parts = 0;
    std::vector<size_t> stack;
    for (size_t start = 0; start < label.size(); ++start) {
        if (!walkable[start] || label[start] >= 0) continue;
        label[start] = parts;
        stack.assign(1, start);
        while (!stack.empty()) {
            const size_t c = stack.back();
            stack.pop_back();
            const int x = int(c % size_t(w)), y = int(c / size_t(w));
            const int next[4][2] = {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}};
            for (const auto& n : next) {
                if (n[0] < 0 || n[1] < 0 || n[0] >= w || n[1] >= h) continue;
                const size_t j = size_t(n[1]) * size_t(w) + size_t(n[0]);
                if (walkable[j] && label[j] < 0) {
                    label[j] = parts;
                    stack.push_back(j);
                }
            }
        }
        ++parts;
    }
    Grid g;
    g.w = w;
    g.h = h;
    g.rows.push_back(0);
    for (int y = 0; y < h; ++y) {
        int x = 0;
        while (x < w) {
            const int v = label[size_t(y) * size_t(w) + size_t(x)];
            const int s = x;
            while (x < w && label[size_t(y) * size_t(w) + size_t(x)] == v) ++x;
            const uint8_t part = v < 0 ? kWall : uint8_t(std::min(v, 254));
            g.runs.emplace_back(uint16_t(x - s), part);
            if (part != kWall) g.partCount = std::max(g.partCount, part + 1);
        }
        g.rows.push_back(uint32_t(g.runs.size()));
    }
    return g;
}

std::string payload(const Grids& maps) {
    std::string out(kMagic, 8);
    put(out, uint32_t(maps.size()), 4);
    const std::map<std::string, const Grid*> sorted = [&] {
        std::map<std::string, const Grid*> s;
        for (const auto& m : maps) s.emplace(m.first, &m.second);
        return s;
    }();
    for (const auto& [name, g] : sorted) {
        out.push_back(char(name.size()));
        out += name;
        put(out, uint32_t(g->w), 2);
        put(out, uint32_t(g->h), 2);
        put(out, uint32_t(g->runs.size()), 4);
        for (const auto& r : g->runs) {
            put(out, r.first, 2);
            out.push_back(char(r.second));
        }
    }
    return out;
}

}  // namespace uagfx::walkgrid
