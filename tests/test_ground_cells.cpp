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

