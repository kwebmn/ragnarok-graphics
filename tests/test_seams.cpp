#include <algorithm>
#include <cmath>
#include <cstdio>
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
    for (seams::Triangle& t : ground)  // 2.7.57: half a tile on - a repeat (v 1 to 0) is no jump
        if (t.group == 2)
            for (seams::Vertex& v : t.v) v.uv0[1] += 0.5f;
    const std::vector<uint32_t> textures = {7, 8, 7};  // groups 0 and 2: one texture, but its coordinates jump
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

TEST(seams_strips_follow_the_cells_crease) {
    // 2.7.5 (the tester: «что-то мигает на холмах, когда кручу камерой» - gone with «Стыки травы и дорог» off): each
    // little quad of a strip split along its cell's own crease - split the other way, a strip cut across the crease
    // of a hill's cell and ran under the ground there. Both ways a cell can be split, corner 3 raised 4 units.
    for (const bool across03 : {false, true}) {
        std::vector<seams::Triangle> ground;
        cell(ground, 0, 0, 10.0f, 0);
        cell(ground, 1, 0, 10.0f, 1);
        if (across03) {  // cell 0 split along its corners 0-3 instead
            seams::Vertex c[4] = {ground[0].v[0], ground[0].v[1], ground[0].v[2], ground[1].v[2]};
            ground[0].v[0] = c[0], ground[0].v[1] = c[1], ground[0].v[2] = c[3];
            ground[1].v[0] = c[0], ground[1].v[1] = c[3], ground[1].v[2] = c[2];
        }
        for (seams::Triangle& t : ground)
            for (seams::Vertex& v : t.v)
                if (v.pos[0] > 14.0f && v.pos[0] < 16.0f && v.pos[2] > 4.0f) v.pos[1] = -4.0f;
        // cell 0's ground at (x, z): its two triangles (up is -y)
        const auto groundY = [&](float x, float z) {
            const float u = (x - 5.0f) / 10.0f, w = (z + 5.0f) / 10.0f;
            if (!across03) return u + w > 1.0f ? -4.0f * (u + w - 1.0f) : 0.0f;
            return u >= w ? -4.0f * w : -4.0f * u;
        };
        const std::vector<uint32_t> textures = {1, 2};
        int triangles = 0;
        for (const seams::Strip& s : seams::build(ground, textures, 0.5f)) {
            if (s.inside != 0 || s.shows != 1) continue;  // the neighbour's strip: half a cell wide, its quads square
            for (size_t k = 0; k + 2 < s.indices.size(); k += 3) {
                float c[3] = {0.0f, 0.0f, 0.0f};
                for (size_t n = 0; n < 3; ++n)
                    for (int a = 0; a < 3; ++a) c[a] += s.vertices[s.indices[k + n]].pos[a] / 3.0f;
                const float off = std::fabs(c[1] - groundY(c[0], c[2]));
                if (off > 1e-3f) std::printf("  across03 %d: a triangle %.3f off the ground\n", int(across03), off);
                CHECK(off < 1e-3f);
                ++triangles;
            }
        }
        CHECK(triangles >= 16);
    }
}

TEST(seams_every_strip_lies_on_its_cells_ground) {
    // 2.7.8 (the tester: still «мигает в точке prt_fild 200 200», 2.7.7): a cell's own band - 0.06 of it wide, its
    // quads a quarter long - crossed its cell's crease near the corners and ran up to a third of a unit under a hill's
    // ground (prt_fild08's cells there, offline), hidden in places as the camera turned. Every strip, the bands too,
    // on the game's two triangles of its cell: a block of 3 x 3 twisted cells, the middle one another texture.
    for (const bool across03 : {false, true}) {
        std::vector<seams::Triangle> ground;
        for (int iz = 0; iz < 3; ++iz)
            for (int ix = 0; ix < 3; ++ix) cell(ground, ix, iz, 10.0f, uint32_t(iz * 3 + ix));
        if (across03)
            for (size_t k = 0; k + 1 < ground.size(); k += 2) {  // each cell split along its corners 0-3 instead
                const seams::Vertex c[4] = {ground[k].v[0], ground[k].v[1], ground[k].v[2], ground[k + 1].v[2]};
                ground[k].v[0] = c[0], ground[k].v[1] = c[1], ground[k].v[2] = c[3];
                ground[k + 1].v[0] = c[0], ground[k + 1].v[1] = c[3], ground[k + 1].v[2] = c[2];
            }
        for (seams::Triangle& t : ground)
            for (seams::Vertex& v : t.v) {
                const float gx = std::round((v.pos[0] - 5.0f) / 10.0f), gz = std::round((v.pos[2] + 5.0f) / 10.0f);
                v.pos[1] = -3.0f * (gx - 1.0f) * (gz - 1.0f);  // every cell twisted by 3 units, none a wall
            }
        // the ground's height at (x, z) on the cell `group`'s two triangles
        const auto groundY = [&](uint32_t group, float x, float z, float& y) {
            for (const seams::Triangle& t : ground) {
                if (t.group != group) continue;
                const float* p0 = t.v[0].pos;
                const float* p1 = t.v[1].pos;
                const float* p2 = t.v[2].pos;
                const float d = (p1[2] - p2[2]) * (p0[0] - p2[0]) + (p2[0] - p1[0]) * (p0[2] - p2[2]);
                const float l0 = ((p1[2] - p2[2]) * (x - p2[0]) + (p2[0] - p1[0]) * (z - p2[2])) / d;
                const float l1 = ((p2[2] - p0[2]) * (x - p2[0]) + (p0[0] - p2[0]) * (z - p2[2])) / d;
                if (l0 < -1e-4f || l1 < -1e-4f || 1.0f - l0 - l1 < -1e-4f) continue;
                y = l0 * p0[1] + l1 * p1[1] + (1.0f - l0 - l1) * p2[1];
                return true;
            }
            return false;
        };
        const std::vector<uint32_t> textures = {2, 2, 2, 2, 1, 2, 2, 2, 2};
        float worst = 0.0f;
        int bands = 0, missed = 0;
        for (const seams::Strip& s : seams::build(ground, textures, 0.5f)) {
            bands += s.inside == s.shows ? 1 : 0;
            for (size_t k = 0; k + 2 < s.indices.size(); k += 3) {
                const seams::Vertex& a = s.vertices[s.indices[k]];
                const seams::Vertex& b = s.vertices[s.indices[k + 1]];
                const seams::Vertex& c = s.vertices[s.indices[k + 2]];
                for (int i = 0; i <= 10; ++i)
                    for (int j = 0; i + j <= 10; ++j) {  // points all over the triangle, its edges too
                        const float l1 = float(i) / 10.0f, l2 = float(j) / 10.0f, l0 = 1.0f - l1 - l2;
                        float at[3];
                        for (int n = 0; n < 3; ++n) at[n] = l0 * a.pos[n] + l1 * b.pos[n] + l2 * c.pos[n];
                        float y = 0.0f;
                        if (!groundY(s.inside, at[0], at[2], y)) {
                            ++missed;
                            continue;
                        }
                        worst = std::max(worst, std::fabs(at[1] - y));
                    }
            }
        }
        std::printf("  across03 %d: %d own bands, the farthest point %.4f off its cell's ground\n", int(across03),
                    bands, worst);
        // the middle cell's and its four neighbours' bands at their edges with it (2.7.57: the texture 2 around it
        // repeats from cell to cell - no seams between those)
        CHECK(bands >= 5);
        CHECK_EQ(missed, 0);  // every point over its own cell
        CHECK(worst < 1e-3f);
    }
}

TEST(seams_strips_the_same_when_the_ground_comes_back_a_hair_off) {
    // 2.7.9 (the tester: the neighbours' strips - magenta in [debug] seams_view - still blinked at prt_fild08 200 200,
    // the map's middle): the ground comes back from the screen with a rounding error that changes as the camera turns.
    // The grid's origin is taken from the first corner - a hair under a line, every cell's place moved by one - and
    // the order of two cells by their keys, z unsigned in their low half, put the row at z -1 after the one at 0: that
    // row of edges took the other side's noise, the line between grass and road jumped. Four rows of cells about the
    // world's z 0, another texture each row; the same strips with the ground a thousandth of a unit off either way.
    const auto block = [](float dx, float dz) {
        std::vector<seams::Triangle> ground;
        for (int iz = -2; iz < 2; ++iz)
            for (int ix = -2; ix < 2; ++ix) {
                seams::Vertex c[4];
                for (int k = 0; k < 4; ++k) {
                    c[k].pos[0] = float(ix + (k & 1)) * 10.0f + dx;
                    c[k].pos[2] = float(iz + (k >> 1)) * 10.0f + dz;
                    c[k].uv0[0] = float(k & 1);
                    c[k].uv0[1] = float(k >> 1);
                }
                seams::Triangle a, b;
                a.v[0] = c[0], a.v[1] = c[1], a.v[2] = c[2];
                b.v[0] = c[2], b.v[1] = c[1], b.v[2] = c[3];
                a.group = b.group = uint32_t((iz + 2) * 4 + ix + 2);
                ground.push_back(a);
                ground.push_back(b);
            }
        return ground;
    };
    std::vector<uint32_t> textures;
    for (int g = 0; g < 16; ++g) textures.push_back(uint32_t(1 + (g / 4) % 2));  // rows of two textures in turn
    // each strip's vertices by the noise's side (green) and the band's flag (red)
    const auto sides = [&](float dx, float dz) {
        std::vector<std::vector<uint32_t>> out(16 * 16);
        for (const seams::Strip& s : seams::build(block(dx, dz), textures, 0.5f))
            for (const seams::Vertex& v : s.vertices) out[s.inside * 16 + s.shows].push_back(v.specular & 0xffff00u);
        return out;
    };
    const auto exact = sides(0.0f, 0.0f);
    size_t strips = 0;
    for (const auto& s : exact) strips += s.empty() ? 0 : 1;
    CHECK(strips >= 24);  // the rows' edges both ways and the bands
    for (const float dz : {-1e-3f, 1e-3f})
        for (const float dx : {-1e-3f, 0.0f, 1e-3f}) {
            const auto off = sides(dx, dz);
            int differ = 0;
            for (size_t k = 0; k < exact.size(); ++k) differ += off[k] != exact[k] ? 1 : 0;
            if (differ) std::printf("  ground off by (%g, %g): %d strips otherwise\n", dx, dz, differ);
            CHECK_EQ(differ, 0);
        }
}

// 3.0.0-beta.2 (R-45AV5H, Aldebaran's square): the game draws a side face between two cells of one height too - a
// wall of no height, its two triangles of no area, with the wall's texture and a dark lightmap. Its normal is
// nothing, so it passed as a face of the ground, took the cell its middle fell in and put the wall's texture along
// that cell's edges: dark lines and smudges round the paving's squares.
TEST(seams_a_wall_of_no_height_is_no_ground) {
    std::vector<seams::Triangle> ground;
    cell(ground, 0, 0, 10.0f, 0);
    cell(ground, 1, 0, 10.0f, 0);
    seams::Vertex corner[4];  // on the cells' shared edge x = 15, its top and its foot at one height
    for (int k = 0; k < 4; ++k) {
        corner[k].pos[0] = 15.0f;
        corner[k].pos[2] = (k & 1) ? 5.0f : -5.0f;
        corner[k].uv0[0] = float(k & 1);
        corner[k].uv0[1] = float(k >> 1);
    }
    seams::Triangle a, b;
    a.v[0] = corner[0], a.v[1] = corner[1], a.v[2] = corner[2];
    b.v[0] = corner[2], b.v[1] = corner[1], b.v[2] = corner[3];
    a.group = b.group = 1;
    ground.push_back(a);
    ground.push_back(b);
    const std::vector<uint32_t> textures = {7, 9};  // the paving's, the wall's
    CHECK(seams::build(ground, textures, 0.45f).empty());  // the paving goes on: no seam
}

TEST(seams_none_over_a_step) {
    // 2.1.6: two textures, the second cell a step higher - a wall between them, no seam (it blinked at the wall's foot)
    std::vector<seams::Triangle> ground;
    cell(ground, 0, 0, 10.0f, 0);
    cell(ground, 1, 0, 10.0f, 1);
    for (size_t k = 2; k < ground.size(); ++k)
        for (seams::Vertex& v : ground[k].v) v.pos[1] = -3.0f;
    const std::vector<uint32_t> textures = {1, 2};
    CHECK(seams::build(ground, textures, 0.45f).empty());
}

namespace {

// A ground of n x n cells, its textures, heights, colours and second coordinates from a fixed sequence.
void varied(std::vector<seams::Triangle>& ground, std::vector<uint32_t>& textures, int n) {
    uint32_t seed = 7;
    const auto next = [&] {
        seed = seed * 1664525u + 1013904223u;
        return seed;
    };
    for (int z = 0; z < n; ++z)
        for (int x = 0; x < n; ++x) {
            const uint32_t group = uint32_t(textures.size());
            textures.push_back(1 + next() % 3);
            seams::Vertex c[4];
            for (int k = 0; k < 4; ++k) {
                c[k].pos[0] = float(x + (k & 1)) * 10.0f;
                c[k].pos[2] = float(z + (k >> 1)) * 10.0f;
                c[k].pos[1] = -0.3f * float((x + (k & 1) + (z + (k >> 1)) * 3) % 5);
                c[k].uv0[0] = float(k & 1) * (next() % 4 == 0 ? 0.5f : 1.0f);
                c[k].uv0[1] = float(k >> 1);
                c[k].uv1[0] = float(next() % 256) / 256.0f;
                c[k].uv1[1] = float(next() % 256) / 256.0f;
                c[k].diffuse = next();
                c[k].specular = next();
            }
            seams::Triangle a, b;
            a.v[0] = c[0], a.v[1] = c[1], a.v[2] = c[2];
            b.v[0] = c[2], b.v[1] = c[1], b.v[2] = c[3];
            a.group = b.group = group;
            ground.push_back(a);
            ground.push_back(b);
        }
}

// FNV-1a over the strips: their groups, every vertex's fields and the indices.
uint64_t hashOf(const std::vector<seams::Strip>& strips) {
    uint64_t h = 1469598103934665603ull;
    const auto add = [&](const void* p, size_t n) {
        for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const uint8_t*>(p)[i]) * 1099511628211ull;
    };
    for (const seams::Strip& s : strips) {
        add(&s.inside, 4);
        add(&s.shows, 4);
        for (const seams::Vertex& v : s.vertices) {
            add(v.pos, 12);
            add(&v.diffuse, 4);
            add(&v.specular, 4);
            add(v.uv0, 8);
            add(v.uv1, 8);
        }
        add(s.indices.data(), s.indices.size() * 2);
    }
    return h;
}

// 2.7.6: the strips' triangles as a set - each triangle's three vertices (in its own order) with its strip's groups,
// summed: the same triangles in another order give the same number.
uint64_t trianglesSetHashOf(const std::vector<seams::Strip>& strips) {
    uint64_t sum = 0;
    for (const seams::Strip& s : strips)
        for (size_t k = 0; k + 2 < s.indices.size(); k += 3) {
            uint64_t h = 1469598103934665603ull;
            const auto add = [&](const void* p, size_t n) {
                for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const uint8_t*>(p)[i]) * 1099511628211ull;
            };
            add(&s.inside, 4);
            add(&s.shows, 4);
            for (size_t n = 0; n < 3; ++n) {
                const seams::Vertex& v = s.vertices[s.indices[k + n]];
                add(v.pos, 12);
                add(&v.diffuse, 4);
                add(&v.specular, 4);
                add(v.uv0, 8);
                add(v.uv1, 8);
            }
            sum += h;
        }
    return sum;
}

}  // namespace

TEST(seams_none_where_a_texture_repeats) {
    // 2.7.57 (R-NSHM8K: straight lines across Prontera's paving; R-ENZVF2: one along a field's road): one texture whose
    // tile ends at the edge (u 1) and starts again past it (u 0) goes on - the same texel; 1.9-2.7.56 took it for a
    // jump and laid its own picture mirrored over every fourth cell's edge
    std::vector<seams::Triangle> ground;
    cell(ground, 0, 0, 10.0f, 0);
    cell(ground, 1, 0, 10.0f, 1);  // u 0-1 again past u 1: the tile repeats
    std::vector<seams::Triangle> shifted = ground;
    for (seams::Triangle& t : shifted)
        if (t.group == 1)
            for (seams::Vertex& v : t.v) v.uv0[1] += 0.5f;  // half a tile off across the edge: a real jump
    const std::vector<uint32_t> textures = {7, 7};
    CHECK(seams::build(ground, textures, 0.45f).empty());
    CHECK(!seams::build(shifted, textures, 0.45f).empty());
}

TEST(seams_strips_stay_as_they_were_byte_for_byte) {
    // 2.5.11: the strips' loops made a few times faster (8.7 ms a frame over cmd_fild02's view, the game's own
    // thread with Direct3D 11) - every vertex as before
    std::vector<seams::Triangle> ground;
    std::vector<uint32_t> textures;
    varied(ground, textures, 12);
    const std::vector<seams::Strip> strips = seams::build(ground, textures, 0.5f);
    size_t vertices = 0;
    for (const seams::Strip& s : strips) vertices += s.vertices.size();
    std::printf("  seams snapshot: %zu strips, %zu vertices, %016llx, triangles %016llx\n", strips.size(), vertices,
                static_cast<unsigned long long>(hashOf(strips)),
                static_cast<unsigned long long>(trianglesSetHashOf(strips)));
    // 2.7.5 split the quads along each cell's crease (every vertex as 2.5.10 made them); 2.7.6 orders the cells by
    // their places - the same triangles as 2.7.5's, in that order; 2.7.8 cuts the triangles the crease runs through
    // (the own bands' at the cells' corners) - the grid's points as before, the crease's after them. 2.7.57: a texture
    // that repeats across an edge (u 1 to 0) seams no more - the strips of the edges of real jumps and of two textures
    CHECK(trianglesSetHashOf(strips) == 0x0ebb3bb22c8bbfc9ull);
    CHECK(hashOf(strips) == 0xb7d520a4a5a51058ull);
}

TEST(seams_strips_the_same_whatever_order_the_ground_comes_in) {
    // 2.7.6 (the tester: the hills still blinked with the seams on, 2.7.5): the strips came out in the order of a hash
    // map's cells - another set of cells in view (the camera turned) and the blended strips that overlap at the
    // cells' corners were drawn in another order, another colour there. The same cells now give the same strips.
    std::vector<seams::Triangle> ground;
    std::vector<uint32_t> textures;
    varied(ground, textures, 12);
    const uint64_t straight = hashOf(seams::build(ground, textures, 0.5f));
    std::vector<seams::Triangle> backwards(ground.rbegin(), ground.rend());
    CHECK(hashOf(seams::build(backwards, textures, 0.5f)) == straight);
    std::vector<seams::Triangle> shuffled = ground;
    for (size_t k = 0; k + 7 < shuffled.size(); k += 7) std::swap(shuffled[k], shuffled[k + 5]);
    CHECK(hashOf(seams::build(shuffled, textures, 0.5f)) == straight);
}

TEST(seams_cache_builds_again_only_when_the_ground_differs) {
    // 2.5.11: standing still the game draws the same ground frame after frame - its strips are not built again
    std::vector<seams::Triangle> ground;
    std::vector<uint32_t> textures;
    varied(ground, textures, 6);
    seams::Cache cache;
    const uint64_t first = hashOf(cache.build(ground, textures, 0.5f));
    CHECK(first == hashOf(seams::build(ground, textures, 0.5f)));
    CHECK_EQ(cache.made(), size_t(1));
    CHECK(hashOf(cache.build(ground, textures, 0.5f)) == first);
    CHECK_EQ(cache.made(), size_t(1));  // the same ground: the same strips, not made again
    ground[3].v[1].pos[1] += 0.25f;     // the camera moved: the ground back in the world a little elsewhere
    CHECK(hashOf(cache.build(ground, textures, 0.5f)) == hashOf(seams::build(ground, textures, 0.5f)));
    CHECK_EQ(cache.made(), size_t(2));
    textures[2] = 9;  // another texture
    cache.build(ground, textures, 0.5f);
    CHECK_EQ(cache.made(), size_t(3));
    cache.build(ground, textures, 0.4f);  // another width
    CHECK_EQ(cache.made(), size_t(4));
    cache.clear();  // another map
    cache.build(ground, textures, 0.4f);
    CHECK_EQ(cache.made(), size_t(5));
}

TEST(seams_none_round_a_patch_laid_by_hand) {
    // 2.7.57 (R-8YJSCR: Prontera's mats torn at their edges; R-NSHM8K: straight lines across its paving): no strip
    // where either cell lies in a patch the map's makers laid (the caller's laid(x, z), core/ground_heights.hpp) -
    // the game's straight edge; between the field's own textures as before
    std::vector<seams::Triangle> ground;
    std::vector<uint32_t> textures;
    for (int z = 0; z < 3; ++z)
        for (int x = 0; x < 5; ++x) {
            const bool mat = z == 1 && (x == 1 || x == 2);
            const bool stone = z == 1 && x == 4;
            textures.push_back(mat ? 9u : stone ? 5u : 1u);
            cell(ground, x, z, 10.0f, uint32_t(textures.size() - 1));
        }
    const auto onMat = [](float x, float z) {  // the cells' corners from (5, -5): the mat's cells at z 5-15, x 15-35
        return z > 5.0f && z < 15.0f && x > 15.0f && x < 35.0f;
    };
    const std::vector<seams::Strip> all = seams::build(ground, textures, 0.45f);
    const std::vector<seams::Strip> kept = seams::build(ground, textures, 0.45f, onMat);
    const auto touches = [&](const std::vector<seams::Strip>& strips, uint32_t texture) {
        for (const seams::Strip& s : strips)
            if (textures[s.inside] == texture || textures[s.shows] == texture) return true;
        return false;
    };
    CHECK(touches(all, 9u) && touches(all, 5u));  // without the map's patches: both seamed
    CHECK(!touches(kept, 9u));                     // the mat keeps its straight edge
    CHECK(touches(kept, 5u));                      // the stone is no laid patch: seamed as before
}

namespace {

// 3.0.5-beta.9: a view of `varied`'s world - the cells from (x0, z0), n a side, the ground back from the screen a hair
// off (`noise` its seed) and its fog (the specular's alpha) by the camera's place
std::vector<seams::Triangle> viewOf(const std::vector<seams::Triangle>& world, int worldSide, int x0, int z0, int n,
                                    uint32_t noise, float camX, float camZ) {
    std::vector<seams::Triangle> out;
    uint32_t seed = noise;
    for (int z = z0; z < z0 + n && z < worldSide; ++z)
        for (int x = x0; x < x0 + n && x < worldSide; ++x)
            for (int k = 0; k < 2; ++k) {
                seams::Triangle t = world[size_t((z * worldSide + x) * 2 + k)];
                for (seams::Vertex& v : t.v) {
                    seed = seed * 1664525u + 1013904223u;
                    v.pos[0] += float(int(seed >> 24) - 128) * 1e-6f;
                    v.pos[1] += float(int((seed >> 16) & 255) - 128) * 1e-6f;
                    const float d = std::hypot(v.pos[0] - camX, v.pos[2] - camZ);
                    v.specular = (v.specular & 0x00FFFFFFu) | uint32_t(std::clamp(255.0f - d, 0.0f, 255.0f)) << 24;
                }
                out.push_back(t);
            }
    return out;
}

}  // namespace

// 3.0.5-beta.9 (R-VMPQKX: the seams built again each frame the camera moved - most of a Radeon 610M laptop's game
// thread on «Високі» with Direct3D 11; the user: «делай 1-3»): each edge's strips kept with what they were made of
// and taken again where their cells are the same, their places and fog made anew from this frame's cells - the
// strips exactly as built afresh, frame after frame of a camera going over the world
TEST(seams_cache_takes_the_same_cells_strips_again_exactly_as_built_afresh) {
    const int side = 24;
    std::vector<seams::Triangle> world;
    std::vector<uint32_t> textures;
    varied(world, textures, side);
    seams::Cache cache;
    size_t frames = 0;
    for (int f = 0; f < 12; ++f) {
        // the camera goes along and turns: the view moves a cell now and then, the fog and the noise every frame
        const std::vector<seams::Triangle> view =
            viewOf(world, side, 2 + f / 3, 3 + f / 4, 14, 100u + uint32_t(f), 60.0f + 5.0f * float(f), 80.0f);
        const uint64_t afresh = hashOf(seams::build(view, textures, 0.5f));
        CHECK(hashOf(cache.build(view, textures, 0.5f)) == afresh);
        ++frames;
    }
    CHECK_EQ(cache.made(), frames);  // each frame's ground differed
    std::printf("  seams cache: %zu edges taken again in %zu frames\n", cache.reused(), frames);
    CHECK(cache.reused() > 5000);  // the frames after the first: most of their ~560 edges' strips taken again
}
