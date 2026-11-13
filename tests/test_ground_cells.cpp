#include <utility>
#include <vector>

#include "core/ground_cells.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A cell of `side` at (ix, iz), its corners at height y (up along -y).
void cell(std::vector<seams::Triangle>& out, int ix, int iz, float side, float y) {
    seams::Vertex c[4];
    for (int k = 0; k < 4; ++k) {
        c[k].pos[0] = (float(ix) + float(k & 1)) * side + 3.0f;
        c[k].pos[1] = y;
        c[k].pos[2] = (float(iz) + float(k >> 1)) * side - 7.0f;
    }
    seams::Triangle a, b;
    a.v[0] = c[0], a.v[1] = c[1], a.v[2] = c[2];
    b.v[0] = c[2], b.v[1] = c[1], b.v[2] = c[3];
    out.push_back(a);
    out.push_back(b);
}

}  // namespace

// 3.0.18-beta.252 (R-9UNG8S): the halls' floors of a castle (a carpet, green tiles - green as grass) taken out of the
// grass's ground - a cell whose middle the caller says is such a floor has no ground then; counted
TEST(ground_cells_the_halls_floors_grow_no_grass) {
    ground_cells::Grid g;
    g.originX = 100.0f;
    g.originZ = -50.0f;
    g.side = 10.0f;
    g.width = 3;
    g.height = 2;
    g.span = {-1.0f, 2.0f, -1.0f, 2.0f, -1.0f, 2.0f, 1e30f, -1e30f, -1.0f, 2.0f, -1.0f, 2.0f};
    std::vector<std::pair<float, float>> asked;
    const size_t dropped = ground_cells::dropWhere(g, [&](float x, float z) {
        asked.emplace_back(x, z);
        return x > 120.0f || (x < 110.0f && z > -40.0f);  // the third column; the first cell of the second row
    });
    CHECK_EQ(dropped, size_t(2));            // (2, 0) and (2, 1); (0, 1) had no ground already
    CHECK_EQ(asked.size(), size_t(5));        // the cells with ground, at their middles
    CHECK(asked[0].first == 105.0f && asked[0].second == -45.0f);
    CHECK(g.span[4] > g.span[5] && g.span[10] > g.span[11]);  // the third column: no ground
    CHECK(g.span[0] <= g.span[1] && g.span[2] <= g.span[3] && g.span[8] <= g.span[9]);
    CHECK(g.span[6] > g.span[7]);
    ground_cells::Grid none;
    CHECK_EQ(ground_cells::dropWhere(none, [](float, float) { return true; }), size_t(0));
}
