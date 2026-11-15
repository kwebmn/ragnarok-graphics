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

// where the edge lies under the map's water none of it yet (its sea: the next step)
TEST(outskirts_none_past_a_wet_edge) {
    gnd::Ground g = groundOf();
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 8; ++x)
            for (float& h : g.cells[size_t(y) * 16 + size_t(x)].h) h = 10.0f;  // the west half under the water
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    outskirts::Options o;
    o.water = true;
    o.waterLevel = 0.0f;
    const terrain::Mesh m = outskirts::build(g, atlas, terrain::Sun{}, o);
    int westNear = 0, eastNear = 0;
    for (const terrain::Vertex& v : m.vertices) {
        const float cx = v.pos[0] / g.zoom + 8.0f, cy = v.pos[2] / g.zoom + 8.0f;
        if (cy > 2.0f && cy < 14.0f && cx < -1.0f && cx > -10.0f) ++westNear;
        if (cy > 2.0f && cy < 14.0f && cx > 17.0f && cx < 26.0f) ++eastNear;
    }
    CHECK(westNear == 0 && eastNear > 0);
}
