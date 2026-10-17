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

TEST(ground_cells_know_the_ground_from_a_deck_above_it) {
    // 2.2.1: umbala - the ground at 0, a wooden deck 12 units up (y -12) over cell (1, 0): the grass on the ground only
    std::vector<seams::Triangle> ground;
    cell(ground, 0, 0, 10.0f, 0.0f);
    cell(ground, 1, 0, 10.0f, 0.0f);
    cell(ground, 0, 1, 10.0f, -2.0f);  // a little higher ground
    const ground_cells::Grid g = ground_cells::build(ground);
    CHECK(g.side > 9.9f && g.width == 2 && g.height == 2);
    CHECK(ground_cells::onGround(g, 18.0f, 0.1f, -2.0f, 0.75f));    // on cell (1, 0)'s ground
    CHECK(!ground_cells::onGround(g, 18.0f, -12.0f, -2.0f, 0.75f));  // the deck over it
    CHECK(ground_cells::onGround(g, 8.0f, -2.2f, 8.0f, 0.75f));     // the higher cell's own height
    CHECK(!ground_cells::onGround(g, 18.0f, -2.0f, 8.0f, 0.75f));   // cell (1, 1): no ground there
    CHECK(ground_cells::onGround(g, 500.0f, -40.0f, 500.0f, 0.75f));  // outside the grid: nothing known
    CHECK(ground_cells::onGround(ground_cells::Grid{}, 0.0f, -99.0f, 0.0f, 0.75f));
}
