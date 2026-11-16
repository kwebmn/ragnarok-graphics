// 3.0.20-beta.30 (the owner's «6,7,8,9», 6 - «Горизонт за краем карты в 3D-режиме»: past a map's edge nothing, the
// world an island in the sky): the land round a map past its ground's edges - the edge's heights going over into hills
// and mountains to the horizon, the edge's texture, a lightmap cell of the map's mean light, each chunk's skirt
#include <cmath>
#include <set>
#include <vector>

#include "core/outskirts.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// 16 x 16 cells, a zoom of 10: the west half texture 0, the east half 1 (a quarter of a texture a cell); flat at -20
// (up along -y) but its west edge's column at -30; the lightmaps two - brightness 100 and 200
gnd::Ground groundOf() {
    gnd::Ground g;
    g.width = g.height = 16;
    g.zoom = 10.0f;
    g.textures = {"a.bmp", "b.bmp"};
    for (int k = 0; k < 2; ++k) {
        gnd::Lightmap l{};
        for (int i = 0; i < 64; ++i) l.brightness[i] = uint8_t(k == 0 ? 100 : 200);
        for (int i = 0; i < 192; ++i) l.rgb[i] = uint8_t(k == 0 ? 10 : 30);
        g.lightmaps.push_back(l);
    }
    for (int t = 0; t < 2; ++t) {
        gnd::Surface s;
        s.u[0] = 0.0f, s.u[1] = 0.25f, s.u[2] = 0.0f, s.u[3] = 0.25f;
        s.v[0] = 0.0f, s.v[1] = 0.0f, s.v[2] = 0.25f, s.v[3] = 0.25f;
        s.texture = int16_t(t);
        s.lightmap = uint16_t(t);
        g.surfaces.push_back(s);
    }
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            gnd::Cell c;
            for (float& h : c.h) h = x == 0 ? -30.0f : -20.0f;
            c.top = x < 8 ? 0 : 1;
            g.cells.push_back(c);
        }
    return g;
}

bool inside(const terrain::Vertex& v, const gnd::Ground& g) {  // strictly inside the map's ground
    const float cx = v.pos[0] / g.zoom + float(g.width) / 2.0f, cy = v.pos[2] / g.zoom + float(g.height) / 2.0f;
    return cx > 0.01f && cx < float(g.width) - 0.01f && cy > 0.01f && cy < float(g.height) - 0.01f;
}

}  // namespace

TEST(outskirts_the_land_round_a_map_past_its_edges) {
    const gnd::Ground g = groundOf();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    outskirts::Options o;
    const terrain::Mesh m = outskirts::build(g, atlas, terrain::Sun{}, o);
    CHECK(!m.vertices.empty() && !m.chunks.empty());
    CHECK(m.vertices.size() < 150000);  // far off its quads larger
    int outside = 0;
    std::set<int> textures;
    for (const terrain::Vertex& v : m.vertices) outside += inside(v, g) ? 0 : 1;
    CHECK(outside == int(m.vertices.size()));  // none on the map's own ground
    for (const terrain::Batch& b : m.batches) {
        textures.insert(b.texture);
        CHECK(b.layer == terrain::kGround && b.indexCount % 3 == 0);
    }
    CHECK(textures == (std::set<int>{0, 1}));  // the edges' own
    for (const terrain::Chunk& c : m.chunks)  // each chunk's box round its vertices
        for (uint32_t i = c.firstVertex; i < c.firstVertex + c.vertexCount; ++i)
            for (int k = 0; k < 3; ++k)
                CHECK(m.vertices[i].pos[k] >= c.lo[k] - 1e-3f && m.vertices[i].pos[k] <= c.hi[k] + 1e-3f);
    // reaching ring cells off each way
    float most = 0.0f;
    for (const terrain::Vertex& v : m.vertices) most = std::max(most, std::fabs(v.pos[0]) / g.zoom);
    CHECK(most >= float(o.ring) && most <= float(o.ring + 8 + 32));
}

TEST(outskirts_heights_from_the_edge_to_the_mountains) {
    const gnd::Ground g = groundOf();
    outskirts::Options o;
    // at the edge itself the map's corners: its west column's -30, the north edge's -20 east of it
    CHECK(std::fabs(outskirts::heightAt(g, o, -0.0f, 8.0f) - -30.0f) < 0.01f);
    CHECK(std::fabs(outskirts::heightAt(g, o, 12.0f, -0.0f) - -20.0f) < 0.01f);
    // a few cells off still near the edge's; far off mountains (up along -y)
    CHECK(std::fabs(outskirts::heightAt(g, o, 12.0f, -2.0f) - -20.0f) < 8.0f);
    float highest = 0.0f;
    for (int k = 0; k < 40; ++k)
        highest = std::min(highest, outskirts::heightAt(g, o, 8.0f + float(k) * 7.0f, -240.0f));
    CHECK(highest < -150.0f);
    // the same place the same height, whatever the build
    CHECK(outskirts::heightAt(g, o, -50.0f, 30.0f) == outskirts::heightAt(g, o, -50.0f, 30.0f));
}

// a lightmap cell of the map's mean light after its own - the outskirts lit as the map is on the whole
TEST(outskirts_lit_by_the_maps_mean_light) {
    const gnd::Ground g = groundOf();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const int mean = terrain::meanLightCell(g);
    CHECK(mean == 2);
    float uv[2];
    terrain::lightmapUv(atlas, mean, 0.5f, 0.5f, uv);
    const int x = int(uv[0] * float(atlas.width)), y = int(uv[1] * float(atlas.height));
    const uint32_t texel = atlas.argb[size_t(y) * size_t(atlas.width) + size_t(x)];
    CHECK((texel >> 24) == 150u && ((texel >> 16) & 0xFFu) == 20u);  // west half 100, east 200; 10 and 30
    const terrain::Mesh m = outskirts::build(g, atlas, terrain::Sun{}, outskirts::Options{});
    CHECK(m.vertices[0].lm[0] == uv[0] && m.vertices[0].lm[1] == uv[1]);
}

// 3.0.20-beta.31 (6 and 7: the sea past a map's wet edge): where the edge lies under the map's water its bed - flat
// under the water's level, no hills or mountains - and the sea's frame round the map at the level (the map's own water
// inside it), as far as the outskirts: four quads; none for a map whose edges are all dry
TEST(outskirts_a_sea_bed_and_the_seas_frame_past_a_wet_edge) {
    gnd::Ground g = groundOf();
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 8; ++x)
            for (float& h : g.cells[size_t(y) * 16 + size_t(x)].h) h = 10.0f;  // the west half under the water
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    outskirts::Options o;
    o.water = true;
    o.waterLevel = 0.0f;
    const terrain::Mesh m = outskirts::build(g, atlas, terrain::Sun{}, o);
    int westNear = 0, westDry = 0, eastNear = 0;
    for (const terrain::Vertex& v : m.vertices) {
        const float cx = v.pos[0] / g.zoom + 8.0f, cy = v.pos[2] / g.zoom + 8.0f;
        if (cy > 2.0f && cy < 14.0f && cx < -1.0f && cx > -200.0f) {
            ++westNear;
            westDry += v.pos[1] <= o.waterLevel ? 1 : 0;  // up along -y: at or over the water
        }
        if (cy > 2.0f && cy < 14.0f && cx > 17.0f && cx < 26.0f) ++eastNear;
    }
    CHECK(westNear > 0 && westDry == 0 && eastNear > 0);  // the west's bed all under the water
    const std::vector<float> frame = outskirts::seaFrame(g, o);
    CHECK(frame.size() == 4 * 6 * 3);  // four quads, two triangles each, x y z a corner
    for (size_t i = 1; i < frame.size(); i += 3) CHECK(frame[i] == o.waterLevel);
    float most = 0.0f;
    for (size_t i = 0; i < frame.size(); i += 3) most = std::max(most, std::fabs(frame[i]) / g.zoom);
    CHECK(most >= float(o.ring));
    const gnd::Ground dry = groundOf();
    CHECK(outskirts::seaFrame(dry, o).empty());  // its edges all over the water
    outskirts::Options none;
    CHECK(outskirts::seaFrame(g, none).empty());  // no water on the map
}

// 3.0.20-beta.31: a map whose edge is wet only where a river or a lake leaves it (prt_fild08 0.17 of its edge's cells,
// pay_fild01 0.13) is no sea's: no water past it, its land all dry - a quarter of the edge at least makes a sea map
// (alberta 0.29, comodo 0.38, izlude 0.91)
TEST(outskirts_a_river_at_the_edge_is_no_sea) {
    gnd::Ground g = groundOf();
    for (int y = 4; y < 12; ++y)  // a river's mouth: three columns of the west edge's middle under the water
        for (int x = 0; x < 3; ++x)
            for (float& h : g.cells[size_t(y) * 16 + size_t(x)].h) h = 10.0f;
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    outskirts::Options o;
    o.water = true;
    o.waterLevel = 0.0f;
    CHECK(outskirts::seaFrame(g, o).empty());
    const terrain::Mesh m = outskirts::build(g, atlas, terrain::Sun{}, o);
    int under = 0;
    for (const terrain::Vertex& v : m.vertices) {
        const float cx = v.pos[0] / g.zoom + 8.0f;
        if (cx < -12.0f && v.pos[1] > o.waterLevel) ++under;  // well off the edge: never under the water
    }
    CHECK(under == 0);
}

namespace {

// groundOf with its outermost `rings` rows and columns of cells without a top (as most of the game's maps: one to
// three rings of nothing round the ground), and one cell in the middle too - a hole the ground closes round
gnd::Ground ringedOf(int rings) {
    gnd::Ground g = groundOf();
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (x < rings || y < rings || x >= 16 - rings || y >= 16 - rings)
                g.cells[size_t(y) * 16 + size_t(x)].top = -1;
    g.cells[8 * 16 + 8].top = -1;
    return g;
}

// the cells (their first corners, cells' units) the mesh's triangles cover within the map's own rectangle
std::set<std::pair<int, int>> coveredIn(const terrain::Mesh& m, const gnd::Ground& g) {
    std::set<std::pair<int, int>> cells;
    for (const terrain::Batch& b : m.batches)
        for (uint32_t i = b.firstIndex; i + 3 <= b.firstIndex + b.indexCount; i += 3) {
            float cx = 0.0f, cy = 0.0f, p[3][2];
            for (uint32_t k = 0; k < 3; ++k) {
                const terrain::Vertex& v = m.vertices[b.firstVertex + m.indices[i + k]];
                p[k][0] = v.pos[0] / g.zoom, p[k][1] = v.pos[2] / g.zoom;
                cx += p[k][0] / 3.0f, cy += p[k][1] / 3.0f;
            }
            // not a skirt's or a curtain's (upright: no size seen from above)
            const float size = (p[1][0] - p[0][0]) * (p[2][1] - p[0][1]) - (p[1][1] - p[0][1]) * (p[2][0] - p[0][0]);
            if (std::fabs(size) < 1e-4f) continue;
            const int x = int(std::floor(cx + float(g.width) / 2.0f)), y = int(std::floor(cy + float(g.height) / 2.0f));
            if (x >= 0 && y >= 0 && x < g.width && y < g.height) cells.insert({x, y});
        }
    return cells;
}

}  // namespace

// 3.0.20-beta.42 (the audit of the horizon): most of the game's maps keep one to three rings of cells without a top
// round their ground (prontera three, prt_fild08 two, geffen one) - beta.30's land began at the map's rectangle, and
// between the ground and it lay a trench of nothing ten to thirty units wide (the sky seen in it from near the edge).
// The land begins where the ground ends: the open cells reached from outside are its own, a hole the ground closes
// round is not; at the ground's edge its heights are the ground's own corners'
TEST(outskirts_begin_where_the_ground_ends_not_at_the_maps_rectangle) {
    const gnd::Ground g = ringedOf(2);
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    outskirts::Options o;
    const terrain::Mesh m = outskirts::build(g, atlas, terrain::Sun{}, o);
    const std::set<std::pair<int, int>> in = coveredIn(m, g);
    int rings = 0, ground = 0;
    for (const auto& c : in) {
        const bool ring = c.first < 2 || c.second < 2 || c.first >= 14 || c.second >= 14;
        rings += ring ? 1 : 0, ground += ring ? 0 : 1;
    }
    CHECK(rings == 16 * 16 - 12 * 12);  // every open cell of the two rings
    CHECK(ground == 0);                 // none of the ground's own, nor the hole in its middle (8, 8)
    // at the ground's edge the ground's own corner heights: its west side's corners (x = 2) -20 (the west column's
    // -30 cells are open now), and no step up or down to the land's first vertices there
    CHECK(std::fabs(outskirts::heightAt(g, o, 2.0f, 8.0f) - -20.0f) < 0.01f);
    CHECK(std::fabs(outskirts::heightAt(g, o, 8.0f, 14.0f) - -20.0f) < 0.01f);
    CHECK(std::fabs(outskirts::heightAt(g, o, 1.0f, 8.0f) - -20.0f) < 4.0f);  // a cell on: near it yet
    float most = 0.0f;  // and still off to the horizon
    for (const terrain::Vertex& v : m.vertices) most = std::max(most, std::fabs(v.pos[0]) / g.zoom);
    CHECK(most >= float(o.ring));
}

// 3.0.20-beta.42: a vertex of the land knows the corner of the ground it lies nearest to and how far off (its
// `blend`: the corner's number row by row of width + 1 in the low 24 bits, the distance in sixteenths of a cell, 255
// at most, in the high 8) - the renderer gives it that corner's colour as the game draws it, going over to the map's
// mean a few cells on (beta.30 took the colour at the rectangle's edge: the filler's grey, half the ground's light)
TEST(outskirts_vertices_know_their_corner_of_the_ground) {
    const gnd::Ground g = ringedOf(1);
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const terrain::Mesh m = outskirts::build(g, atlas, terrain::Sun{}, outskirts::Options{});
    int atEdge = 0, far = 0;
    for (const terrain::Vertex& v : m.vertices) {
        const uint32_t corner = v.blend & 0xFFFFFFu, sixteenths = v.blend >> 24;
        const int i = int(corner % 17u), j = int(corner / 17u);
        CHECK(i >= 1 && i <= 15 && j >= 1 && j <= 15);  // a corner of the ground (its cells 1..14)
        const float cx = v.pos[0] / g.zoom + 8.0f, cy = v.pos[2] / g.zoom + 8.0f;
        const float d = std::hypot(cx - float(i), cy - float(j));
        if (d < 15.9f) CHECK(std::fabs(float(sixteenths) / 16.0f - d) < 0.1f);
        else CHECK(sixteenths == 255u);
        atEdge += d < 0.01f ? 1 : 0, far += sixteenths == 255u ? 1 : 0;
    }
    CHECK(atEdge > 0 && far > 0);
}

// 3.0.20-beta.42: a map its makers hung in the sky (yuno, gonryun: nine tenths and more of its outer band of cells
// open) keeps the sky round it - no land, no sea
TEST(outskirts_none_round_a_map_hung_in_the_sky) {
    gnd::Ground g = groundOf();
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (x < 6 || y < 6 || x >= 10 || y >= 10) g.cells[size_t(y) * 16 + size_t(x)].top = -1;  // an island
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    outskirts::Options o;
    o.water = true;
    o.waterLevel = -100.0f;  // all of it under the water: still none
    CHECK(outskirts::build(g, atlas, terrain::Sun{}, o).vertices.empty());
    CHECK(outskirts::seaFrame(g, o).empty());
}

// 3.0.20-beta.42: the sea of a seaside map covers the open cells within the map's rectangle too (izlude's outer ring
// and half the next) - beta.31's frame began at the rectangle, a strip without water between the map's and the sea
TEST(outskirts_the_sea_covers_the_open_cells_within_the_map) {
    gnd::Ground g = ringedOf(1);
    g.cells[8 * 16 + 8].top = 0;  // no hole
    for (gnd::Cell& c : g.cells)
        for (float& h : c.h) h = 10.0f;  // all under the water
    outskirts::Options o;
    o.water = true;
    o.waterLevel = 0.0f;
    const std::vector<float> frame = outskirts::seaFrame(g, o);
    CHECK(frame.size() > 4 * 6 * 3 && frame.size() % 9 == 0);
    // the ring's cells' middles each under a triangle of it, the ground's own cells' not
    const auto under = [&](float px, float pz) {
        for (size_t t = 0; t + 9 <= frame.size(); t += 9) {
            const float ax = frame[t], az = frame[t + 2], bx = frame[t + 3], bz = frame[t + 5], cx = frame[t + 6],
                        cz = frame[t + 8];
            const float d0 = (bx - ax) * (pz - az) - (bz - az) * (px - ax);
            const float d1 = (cx - bx) * (pz - bz) - (cz - bz) * (px - bx);
            const float d2 = (ax - cx) * (pz - cz) - (az - cz) * (px - cx);
            if ((d0 >= 0 && d1 >= 0 && d2 >= 0) || (d0 <= 0 && d1 <= 0 && d2 <= 0)) return true;
        }
        return false;
    };
    int ring = 0, ground = 0;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const bool covered = under((float(x) + 0.5f - 8.0f) * g.zoom, (float(y) + 0.5f - 8.0f) * g.zoom);
            const bool open = x < 1 || y < 1 || x >= 15 || y >= 15;
            ring += open && covered ? 1 : 0, ground += !open && covered ? 1 : 0;
        }
    CHECK(ring == 16 * 16 - 14 * 14 && ground == 0);
    for (size_t i = 1; i < frame.size(); i += 3) CHECK(frame[i] == o.waterLevel);
}

// 3.0.20-beta.43 (the audit of the horizon): the land's base hangs smooth between the ground's edge and the edge's
// mean far off - every corner of it the mean of its four neighbours (a sheet stretched over the ground's edge: a narrow
// step of the edge dies out within its own width). Beta.30's took each corner's height from its nearest corner of the
// edge alone: a wall or a moat meeting the map's edge ran straight out for sixty cells (two straight banks out of
// Prontera's south gate)
TEST(outskirts_base_hangs_smooth_from_the_grounds_edge) {
    gnd::Ground g = groundOf();
    for (int y = 6; y < 9; ++y)  // a narrow bank at the west edge: three cells 40 over the rest
        for (int x = 0; x < 2; ++x)
            for (float& h : g.cells[size_t(y) * 16 + size_t(x)].h) h = -70.0f;
    outskirts::Options o;
    o.relief = false;  // the base alone: no hills, no mountains
    // at the edge the ground's own corners yet
    CHECK(std::fabs(outskirts::heightAt(g, o, 0.0f, 7.0f) - -70.0f) < 0.01f);
    const std::vector<float> h = outskirts::heightsOf(g, o, -40, -12, 0, 28);  // corners x -40..0, y -12..28
    const int w = 41;
    const auto at = [&](int x, int y) { return h[size_t(y + 12) * size_t(w) + size_t(x + 40)]; };
    float worst = 0.0f;
    for (int y = -11; y < 28; ++y)
        for (int x = -30; x < -1; ++x)  // the sheet itself (its last cells, from 36 off, ease onto the mean)
            worst = std::max(worst, std::fabs(at(x, y) - 0.25f * (at(x - 1, y) + at(x + 1, y) + at(x, y - 1) +
                                                                  at(x, y + 1))));
    CHECK(worst < 0.05f);
    // the bank dies out within a few cells: twelve cells off its line the land where it would be without it
    const float there = at(-12, 7), beside = at(-12, 20);
    CHECK(std::fabs(there - beside) < 4.0f);  // 40 at the edge
    CHECK(at(-1, 7) < at(-1, 20) - 15.0f);    // by the edge the bank still stands (up along -y)
}
