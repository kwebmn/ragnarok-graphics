#include <cmath>
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
    // the strip inside cell 0 showing cell 1: at its edge (x 15) cell 1's u kept off its rim (2.0.2: 0.06), mirrored
    // to 0.45 inside
    for (const seams::Strip& s : strips)
        if (s.inside == 0 && s.shows == 1) {
            CHECK(s.vertices[0].pos[0] > 14.9f && s.vertices[0].uv0[0] > 0.05f && s.vertices[0].uv0[0] < 0.07f);
            CHECK(s.vertices[14].uv0[0] > 0.44f && s.vertices[14].uv0[0] < 0.46f);
            // half the neighbour's at the edge's middle, 0.05 inside at 0.45 of the cell
            CHECK_EQ(int(s.vertices[2].diffuse >> 24), 128);
            CHECK(int(s.vertices[12].diffuse >> 24) > 10 && int(s.vertices[12].diffuse >> 24) < 16);
        }
}

TEST(seams_strips_lie_on_the_games_triangles) {
    // 2.1.2: a cell whose corners do not lie in one plane (corner 3 raised) - the strip's points on its two triangles
    std::vector<seams::Triangle> ground;
    cell(ground, 0, 0, 10.0f, 0);
    cell(ground, 1, 0, 10.0f, 1);
    for (seams::Triangle& t : ground)
        for (seams::Vertex& v : t.v)
            if (v.pos[0] > 14.0f && v.pos[0] < 16.0f && v.pos[2] > 4.0f) v.pos[1] = -4.0f;  // the shared edge's end
    const std::vector<uint32_t> textures = {1, 2};
    for (const seams::Strip& s : seams::build(ground, textures, 0.5f)) {
        if (s.inside != 0) continue;
        for (const seams::Vertex& v : s.vertices) {
            // cell 0 is split along its corners 1-2 (x 15, z -5 to x 5, z 5): on its far triangle the height runs
            // from the raised corner 3 (x 15, z 5) - linear in x and z there, never above it
            CHECK(v.pos[1] <= 0.0f && v.pos[1] >= -4.001f);
            const float u = (v.pos[0] - 5.0f) / 10.0f, w = (v.pos[2] + 5.0f) / 10.0f;
            if (u + w > 1.0f) CHECK(std::fabs(v.pos[1] + 4.0f * (u + w - 1.0f)) < 1e-3f);
            else CHECK(std::fabs(v.pos[1]) < 1e-3f);
        }
    }
}
