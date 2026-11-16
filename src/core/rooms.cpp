#include "core/rooms.hpp"

#include <algorithm>
#include <cmath>

#include "core/outskirts.hpp"

namespace uagfx::rooms {

Map build(const gnd::Ground& g) {
    Map m;
    const int w = std::max(g.width, 0), h = std::max(g.height, 0);
    const size_t cells = size_t(w) * size_t(h);
    if (cells == 0 || g.cells.size() < cells) return m;
    m.width = w, m.height = h;
    m.zoom = g.zoom;
    m.room.assign(cells, 0);
    std::vector<uint8_t> isFloor(cells, 0);
    for (size_t i = 0; i < cells; ++i) {
        const gnd::Cell& c = g.cells[i];
        if (c.top < 0 || size_t(c.top) >= g.surfaces.size()) continue;
        const int texture = g.surfaces[size_t(c.top)].texture;
        isFloor[i] = texture >= 0 && size_t(texture) < g.textures.size() &&
                     !outskirts::fillerTexture(g.textures[size_t(texture)]);
    }
    // the floors joined side to side
    struct Found {
        std::vector<uint32_t> cells;
    };
    std::vector<Found> found;
    std::vector<uint8_t> seen(cells, 0);
    std::vector<uint32_t> stack;
    for (size_t first = 0; first < cells; ++first) {
        if (!isFloor[first] || seen[first]) continue;
        Found f;
        stack.assign(1, uint32_t(first));
        seen[first] = 1;
        while (!stack.empty()) {
            const uint32_t k = stack.back();
            stack.pop_back();
            f.cells.push_back(k);
            const int x = int(k % uint32_t(w)), y = int(k / uint32_t(w));
            const int beside[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& d : beside) {
                const int nx = x + d[0], ny = y + d[1];
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                const size_t j = size_t(ny) * size_t(w) + size_t(nx);
                if (!isFloor[j] || seen[j]) continue;
                seen[j] = 1;
                stack.push_back(uint32_t(j));
            }
        }
        if (int(f.cells.size()) >= kLeastCells) found.push_back(std::move(f));
    }
    std::stable_sort(found.begin(), found.end(),
                     [](const Found& a, const Found& b) { return a.cells.size() > b.cells.size(); });
    if (found.size() > size_t(kMost)) found.resize(size_t(kMost));
    m.count = int(found.size());
    m.floor.assign(found.size() + 1, 0.0f);
    for (size_t r = 0; r < found.size(); ++r) {
        std::vector<float> ups;
        ups.reserve(found[r].cells.size());
        for (const uint32_t k : found[r].cells) {
            m.room[k] = uint8_t(r + 1);
            const float* c = g.cells[k].h;
            ups.push_back(-(c[0] + c[1] + c[2] + c[3]) * 0.25f);
        }
        std::nth_element(ups.begin(), ups.begin() + std::ptrdiff_t(ups.size() / 2), ups.end());
        m.floor[r + 1] = ups[ups.size() / 2];
    }
    // widened: a cell beside a room's takes its number (of two rooms' the larger one's - the lower number)
    m.widened = m.room;
    for (int pass = 0; pass < kWiden; ++pass) {
        const std::vector<uint8_t> was = m.widened;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const size_t i = size_t(y) * size_t(w) + size_t(x);
                if (was[i]) continue;
                uint8_t best = 0;
                const int beside[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (const auto& d : beside) {
                    const int nx = x + d[0], ny = y + d[1];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                    const uint8_t id = was[size_t(ny) * size_t(w) + size_t(nx)];
                    if (id && (!best || id < best)) best = id;
                }
                m.widened[i] = best;
            }
    }
    return m;
}

int roomAt(const Map& m, float x, float z) {
    if (m.width <= 0 || m.height <= 0 || !(m.zoom > 0.0f) || m.widened.size() < size_t(m.width) * size_t(m.height))
        return 0;
    const float cx = std::floor(x / m.zoom + float(m.width) * 0.5f);
    const float cz = std::floor(z / m.zoom + float(m.height) * 0.5f);
    if (!(cx >= 0.0f && cz >= 0.0f && cx < float(m.width) && cz < float(m.height))) return 0;
    return m.widened[size_t(cz) * size_t(m.width) + size_t(cx)];
}

}  // namespace uagfx::rooms
