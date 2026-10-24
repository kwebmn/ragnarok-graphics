#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "core/gnd.hpp"
#include "core/terrain.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.4-beta.9 (the user: «сделай 1» - the own world's prototype on prt_fild08): the ground drawn from the map's own
// .gnd - its cells' heights, textures and lightmaps - smooth where the ground is whole, its steps kept
namespace {

struct Writer {
    std::vector<uint8_t> b;
    void bytes(const void* p, size_t n) {
        b.insert(b.end(), static_cast<const uint8_t*>(p), static_cast<const uint8_t*>(p) + n);
    }
    void i32(int32_t v) { bytes(&v, 4); }
    void i16(int16_t v) { bytes(&v, 2); }
    void u16(uint16_t v) { bytes(&v, 2); }
    void f32(float v) { bytes(&v, 4); }
    void text(const char* s, size_t n) {
        std::vector<char> z(n, 0);
        std::memcpy(z.data(), s, std::min(n, std::strlen(s)));
        bytes(z.data(), n);
    }
};

// 4 x 2 cells, 10 units each: a gentle slope rising along x, a step of 30 units between cells 1 and 2 of the first
// row (a wall's right surface there); two textures, two lightmaps
std::vector<uint8_t> sampleGnd() {
    Writer w;
    w.bytes("GRGN", 4);
    const uint8_t version[2] = {1, 7};
    w.bytes(version, 2);
    w.i32(4);
    w.i32(2);
    w.f32(10.0f);
    w.i32(2);
    w.i32(80);
    w.text("grass.bmp", 80);
    w.text("dirt.bmp", 80);
    w.i32(2);  // lightmaps: 8 x 8, one cell
    w.i32(8);
    w.i32(8);
    w.i32(1);
    for (int l = 0; l < 2; ++l) {
        for (int i = 0; i < 64; ++i) w.bytes(l == 0 ? "\xff" : "\x80", 1);  // brightness
        for (int i = 0; i < 64; ++i) w.bytes("\x10\x20\x30", 3);            // colour
    }
    w.i32(3);  // surfaces: u4, v4, texture, lightmap, BGRA
    for (int s = 0; s < 3; ++s) {
        const float u[4] = {0, 1, 0, 1}, v[4] = {0, 0, 1, 1};
        for (float f : u) w.f32(f);
        for (float f : v) w.f32(f);
        w.i16(int16_t(s == 1 ? 1 : 0));
        w.u16(uint16_t(s == 1 ? 1 : 0));
        w.bytes("\xff\xff\xff\xff", 4);
    }
    // cells: heights (x,y) (x+1,y) (x,y+1) (x+1,y+1), top, front, right; up is -y
    const auto cell = [&](float a, float b, float c, float d, int top, int front, int right) {
        w.f32(a), w.f32(b), w.f32(c), w.f32(d);
        w.i32(top), w.i32(front), w.i32(right);
    };
    cell(0, -2, 0, -2, 0, -1, -1);
    cell(-2, -4, -2, -4, 0, -1, 2);  // its right side a wall down to the next cell
    cell(26, 24, 26, 24, 1, -1, -1);  // 30 lower (the ground's +y is down)
    cell(24, 22, 24, 22, 1, -1, -1);
    cell(0, -2, 0, -2, 0, -1, -1);
    cell(-2, -4, -2, -4, 0, -1, -1);
    cell(-4, -6, -4, -6, 0, -1, -1);
    cell(-6, -8, -6, -8, 0, -1, -1);
    return w.b;
}

gnd::Ground parsed() {
    gnd::Ground g;
    std::string error;
    const std::vector<uint8_t> b = sampleGnd();
    CHECK(gnd::parse(b.data(), b.size(), g, error));
    return g;
}

}  // namespace

TEST(gnd_read_whole) {
    const gnd::Ground g = parsed();
    CHECK(g.width == 4 && g.height == 2 && g.zoom == 10.0f);
    CHECK(g.textures.size() == 2 && g.textures[1] == "dirt.bmp");
    CHECK(g.lightmaps.size() == 2 && g.lightmaps[1].brightness[0] == 0x80 && g.lightmaps[0].rgb[1] == 0x20);
    CHECK(g.surfaces.size() == 3 && g.surfaces[1].texture == 1 && g.surfaces[1].lightmap == 1);
    CHECK(g.surfaces[0].u[1] == 1.0f && g.surfaces[0].v[2] == 1.0f);
    CHECK(g.cells.size() == 8 && g.cells[1].right == 2 && g.cells[2].h[0] == 26.0f);
    std::string error;
    const uint8_t junk[8] = {'G', 'R', 'G', 'X', 1, 7, 0, 0};
    gnd::Ground none;
    CHECK(!gnd::parse(junk, sizeof junk, none, error) && !error.empty());
    std::vector<uint8_t> cut = sampleGnd();
    cut.resize(cut.size() - 20);  // its last cell short
    CHECK(!gnd::parse(cut.data(), cut.size(), none, error));
}

TEST(terrain_the_game_s_cells_where_whole_and_flat) {
    const gnd::Ground g = parsed();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const terrain::Mesh m = terrain::build(g, 1, atlas);
    // 8 tops, one wall: 9 quads, 18 triangles
    CHECK_EQ(m.indices.size(), size_t(18 * 3));
    CHECK_EQ(m.batches.size(), size_t(2));  // by texture
    // cell (0, 0)'s corner (0, 0) at the map's middle minus half: x = (0 - 2) * 10, z = (0 - 1) * 10
    bool found = false;
    for (const terrain::Vertex& v : m.vertices)
        found = found || (v.pos[0] == -20.0f && v.pos[2] == -10.0f && v.pos[1] == 0.0f);
    CHECK(found);
}

TEST(terrain_smooth_where_whole_and_its_steps_kept) {
    const gnd::Ground g = parsed();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const terrain::Mesh m = terrain::build(g, 4, atlas);
    // the corners keep the game's heights: cell (2, 0) still 30 below cell (1, 0) at their common edge
    float leftOfStep = 1e9f, rightOfStep = 1e9f;
    for (const terrain::Vertex& v : m.vertices) {
        if (v.pos[0] == 0.0f && v.pos[2] == -10.0f && v.normal[1] < -0.5f) {  // the edge x = 0, tops only
            if (v.pos[1] < 0.0f) leftOfStep = v.pos[1];
            else rightOfStep = v.pos[1];
        }
    }
    CHECK(leftOfStep == -4.0f && rightOfStep == 26.0f);
    // the slope's middle row of the second line: in between its corners, never past them
    for (const terrain::Vertex& v : m.vertices)
        if (v.pos[2] > 0.0f && v.pos[2] < 10.0f && v.normal[1] < -0.5f) CHECK(v.pos[1] <= 0.01f && v.pos[1] >= -8.01f);
    // normals of the tops face up (-y), unit length
    for (const terrain::Vertex& v : m.vertices) {
        const float l = std::sqrt(v.normal[0] * v.normal[0] + v.normal[1] * v.normal[1] + v.normal[2] * v.normal[2]);
        CHECK(std::fabs(l - 1.0f) < 1e-3f);
    }
}

TEST(terrain_lightmaps_in_one_atlas) {
    const gnd::Ground g = parsed();
    const terrain::Atlas a = terrain::lightmapAtlas(g);
    CHECK(a.width >= 16 && a.height >= 8 && a.argb.size() == size_t(a.width) * size_t(a.height));
    // lightmap 1 at the atlas' second cell: brightness 0x80 in alpha, colour 0x102030
    CHECK_EQ(a.argb[size_t(3) * size_t(a.width) + 8 + 3], 0x80102030u);
    const terrain::Mesh m = terrain::build(g, 1, a);
    for (const terrain::Vertex& v : m.vertices) CHECK(v.lm[0] >= 0.0f && v.lm[0] <= 1.0f && v.lm[1] <= 1.0f);
}

TEST(terrain_in_chunks_for_the_view) {
    // the renderer projects the chunks in view alone: each its own vertices and batches, all of them together the mesh
    const gnd::Ground g = parsed();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const terrain::Mesh m = terrain::build(g, 2, atlas);
    CHECK_EQ(m.chunks.size(), size_t(1));  // 4 x 2 cells: one chunk of kChunk x kChunk
    if (m.chunks.empty()) return;
    const terrain::Chunk& c = m.chunks[0];
    CHECK(c.firstVertex == 0 && c.vertexCount == m.vertices.size());
    CHECK(c.firstBatch == 0 && c.batchCount == m.batches.size());
    uint32_t indices = 0;
    for (const terrain::Batch& b : m.batches) indices += b.indexCount;
    CHECK_EQ(size_t(indices), m.indices.size());
    // its box holds every vertex: x -20..20, z -10..10, heights -8..26
    CHECK(c.lo[0] == -20.0f && c.hi[0] == 20.0f && c.lo[2] == -10.0f && c.hi[2] == 10.0f);
    CHECK(c.lo[1] <= -8.0f && c.hi[1] >= 26.0f);
}

TEST(terrain_batches_hold_their_own_vertices) {
    // 3.0.4-beta.10: a batch's vertices lie together, its indices into them alone - one draw with 16-bit indices,
    // the chunk's vertices not copied for each of its textures
    const gnd::Ground g = parsed();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    for (int n : {1, 3}) {
        const terrain::Mesh m = terrain::build(g, n, atlas);
        uint32_t vertices = 0;
        for (const terrain::Chunk& c : m.chunks) {
            uint32_t at = c.firstVertex;
            for (uint32_t k = c.firstBatch; k < c.firstBatch + c.batchCount; ++k) {
                const terrain::Batch& b = m.batches[k];
                CHECK_EQ(b.firstVertex, at);  // one after another through the chunk's
                CHECK(b.vertexCount > 0 && b.vertexCount <= 65535);
                at += b.vertexCount;
                for (uint32_t i = b.firstIndex; i < b.firstIndex + b.indexCount; ++i)
                    CHECK(m.indices[i] < b.vertexCount);
            }
            CHECK_EQ(at, c.firstVertex + c.vertexCount);
            vertices += c.vertexCount;
        }
        CHECK_EQ(size_t(vertices), m.vertices.size());
        // the dirt's cells (texture 1: (2, 0) and (3, 0)) in its batch alone, the grass' in the other
        for (const terrain::Batch& b : m.batches)
            for (uint32_t v = b.firstVertex; v < b.firstVertex + b.vertexCount; ++v) {
                const terrain::Vertex& x = m.vertices[v];
                const bool dirtCell = x.pos[0] > 0.0f && x.pos[2] < 0.0f && x.normal[1] < -0.5f;
                if (dirtCell) CHECK_EQ(b.texture, 1);
            }
    }
}

TEST(terrain_the_detail_levels_share_their_chunks) {
    // 3.0.4-beta.10: far off the ground is drawn as the game's cells (detail 1) - its chunks the same as the smooth
    // ground's, one for one
    const gnd::Ground g = parsed();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const terrain::Mesh fine = terrain::build(g, 3, atlas), coarse = terrain::build(g, 1, atlas);
    CHECK_EQ(fine.chunks.size(), coarse.chunks.size());
    for (size_t k = 0; k < fine.chunks.size() && k < coarse.chunks.size(); ++k) {
        CHECK(fine.chunks[k].lo[0] == coarse.chunks[k].lo[0] && fine.chunks[k].lo[2] == coarse.chunks[k].lo[2]);
        CHECK(fine.chunks[k].hi[0] == coarse.chunks[k].hi[0] && fine.chunks[k].hi[2] == coarse.chunks[k].hi[2]);
    }
}
