#include "core/seams.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A cell of `side` at (ix, iz) as two triangles, its texture coordinates 0-1 over it.
void cell(std::vector<seams::Triangle>& out, int ix, int iz, float side, uint32_t group) {
    seams::Vertex c[4];
    for (int k = 0; k < 4; ++k) {
        c[k].pos[0] = (float(ix) + float(k & 1)) * side + 5.0f;
        c[k].pos[2] = (float(iz) + float(k >> 1)) * side - 5.0f;
        c[k].uv0[0] = float(k & 1);
        c[k].uv0[1] = float(k >> 1);
    }
    seams::Triangle a, b;
    a.v[0] = c[0], a.v[1] = c[1], a.v[2] = c[2];
    b.v[0] = c[2], b.v[1] = c[1], b.v[2] = c[3];
    a.group = b.group = group;
    out.push_back(a);
    out.push_back(b);
}

}  // namespace

TEST(seams_strips_where_two_textures_meet_none_where_one_goes_on) {
    std::vector<seams::Triangle> ground;
    cell(ground, 0, 0, 10.0f, 0);
    cell(ground, 1, 0, 10.0f, 1);
    cell(ground, 0, 1, 10.0f, 2);
    const std::vector<uint32_t> textures = {7, 8, 7};  // groups 0 and 2: one texture, but its coordinates restart
    CHECK(seams::cellSide(ground) > 9.9f);
    const std::vector<seams::Strip> strips = seams::build(ground, textures, 0.45f);
    // 0|1 both ways, 0|2 both ways (the coordinates jump at their edge), and each cell's own band at its seams
    CHECK_EQ(int(strips.size()), 7);
    for (const seams::Strip& s : strips) CHECK_EQ(int(s.vertices.size()), s.inside == 0 && s.shows == 0 ? 30 : 15);
    // the strip inside cell 0 showing cell 1: at its edge (x 15) cell 1's u kept off its rim (0.12), mirrored to 0.45
    // inside
    for (const seams::Strip& s : strips)
        if (s.inside == 0 && s.shows == 1) {
            CHECK(s.vertices[0].pos[0] > 14.9f && s.vertices[0].uv0[0] > 0.11f && s.vertices[0].uv0[0] < 0.13f);
            CHECK(s.vertices[14].uv0[0] > 0.44f && s.vertices[14].uv0[0] < 0.46f);
            // half the neighbour's at the edge's middle, 0.05 inside at 0.45 of the cell
            CHECK_EQ(int(s.vertices[2].diffuse >> 24), 128);
            CHECK(int(s.vertices[12].diffuse >> 24) > 10 && int(s.vertices[12].diffuse >> 24) < 16);
        }
}
