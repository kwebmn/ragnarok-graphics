#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <utility>
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

// 3.0.4-beta.12: 4 x 2 flat cells of 10, the first row grass grass dirt dirt, the second all grass - seams between
// (1, 0) and (2, 0) (x = 0), and under (2, 0) and (3, 0) (z = 0); `laid` the dirt as a patch laid by hand
gnd::Ground flat() {
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
    w.i32(2);
    w.i32(8);
    w.i32(8);
    w.i32(1);
    for (int l = 0; l < 2; ++l) {
        for (int i = 0; i < 64; ++i) w.bytes("\xff", 1);
        for (int i = 0; i < 64; ++i) w.bytes("\x10\x20\x30", 3);
    }
    w.i32(2);
    for (int s = 0; s < 2; ++s) {
        const float u[4] = {0, 1, 0, 1}, v[4] = {0, 0, 1, 1};
        for (float f : u) w.f32(f);
        for (float f : v) w.f32(f);
        w.i16(int16_t(s));
        w.u16(uint16_t(s));
        w.bytes("\xff\xff\xff\xff", 4);
    }
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 4; ++x) {
            for (int k = 0; k < 4; ++k) w.f32(0.0f);
            w.i32(y == 0 && x >= 2 ? 1 : 0), w.i32(-1), w.i32(-1);
        }
    gnd::Ground g;
    std::string error;
    CHECK(gnd::parse(w.b.data(), w.b.size(), g, error));
    return g;
}

size_t layerBatches(const terrain::Mesh& m, uint8_t layer) {
    size_t n = 0;
    for (const terrain::Batch& b : m.batches) n += b.layer == layer ? 1 : 0;
    return n;
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

TEST(gnd_read_the_newer_versions_too) {
    // 3.0.4-beta.13: 1.8 and 1.9 (88 of the client's 1246 grounds, sva_god1 among them) are 1.7 with the maps' water
    // after the cells - 1.8 its plane, 1.9 a grid of planes; the ground itself read as 1.7's
    for (uint8_t minor : {uint8_t(8), uint8_t(9)}) {
        std::vector<uint8_t> b = sampleGnd();
        b[5] = minor;
        b.insert(b.end(), minor == 8 ? 36 : 56, uint8_t(0));  // the water as the client's files have it
        gnd::Ground g;
        std::string error;
        CHECK(gnd::parse(b.data(), b.size(), g, error));
        CHECK(g.width == 4 && g.height == 2 && g.cells.size() == 8 && g.cells[2].h[0] == 26.0f);
    }
    std::vector<uint8_t> b = sampleGnd();
    b[5] = 6;  // an older one: not known
    gnd::Ground g;
    std::string error;
    CHECK(!gnd::parse(b.data(), b.size(), g, error));
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

namespace {

// 3.0.7-beta.2 (R-V6ZHJX «на стенах не должно быть полос», c_tower2): 2 x 2 cells of 10, (0, 0) 10 over the rest
// with a wall on its right side (surface 1, lightmap 1) and on its far side (surface 2, lightmap 2), each corner of
// theirs its own texture coordinates
gnd::Ground walled() {
    Writer w;
    w.bytes("GRGN", 4);
    const uint8_t version[2] = {1, 7};
    w.bytes(version, 2);
    w.i32(2);
    w.i32(2);
    w.f32(10.0f);
    w.i32(1);
    w.i32(80);
    w.text("wall.bmp", 80);
    w.i32(3);
    w.i32(8);
    w.i32(8);
    w.i32(1);
    for (int l = 0; l < 3; ++l) {
        for (int i = 0; i < 64; ++i) w.bytes("\xff", 1);
        for (int i = 0; i < 64; ++i) w.bytes("\x10\x20\x30", 3);
    }
    w.i32(3);
    for (int s = 0; s < 3; ++s) {
        for (int k = 0; k < 4; ++k) w.f32(0.1f * float(s) + 0.01f * float(k));        // u: corner k
        for (int k = 0; k < 4; ++k) w.f32(0.5f + 0.1f * float(s) + 0.01f * float(k));  // v
        w.i16(0);
        w.u16(uint16_t(s));
        w.bytes("\xff\xff\xff\xff", 4);
    }
    const auto cell = [&](float h, int top, int front, int right) {
        for (int k = 0; k < 4; ++k) w.f32(h);
        w.i32(top), w.i32(front), w.i32(right);
    };
    cell(-10, 0, 2, 1);
    cell(0, 0, -1, -1);
    cell(0, 0, -1, -1);
    cell(0, 0, -1, -1);
    gnd::Ground g;
    std::string error;
    const std::vector<uint8_t> b = w.b;
    CHECK(gnd::parse(b.data(), b.size(), g, error));
    return g;
}

}  // namespace

// The walls' corners as uaRO.exe lays them (FUN_004e4190): the surface's corner k - its texture's coordinates and its
// lightmap's - on the wall's vertex k. A right side: 0 at (x + 1, y + 1) on the cell, 1 at (x + 1, y) on the cell, 2
// and 3 the same on the next cell; a far side: 0 at (x, y + 1), 1 at (x + 1, y + 1) on the cell, 2 and 3 on the next
// row's. The right sides laid the other way along them (3.0.4-beta.9 to 3.0.7-beta.1) had their pictures and light
// turned round: each of c_tower2's walls lit the other way to the next, stripes along the wall
TEST(terrain_walls_lay_their_corners_as_the_game) {
    const gnd::Ground g = walled();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    CHECK_EQ(atlas.height, 8);
    // the places of each wall's corners 0-3: x, height, z (the map's middle at the corner of cell (1, 1))
    const float right[4][3] = {{0, -10, 0}, {0, -10, -10}, {0, 0, 0}, {0, 0, -10}};
    const float far[4][3] = {{-10, -10, 0}, {0, -10, 0}, {-10, 0, 0}, {0, 0, 0}};
    for (int n : {1, 4}) {
        const terrain::Mesh m = terrain::build(g, n, atlas);
        int seen = 0;
        for (const terrain::Vertex& v : m.vertices) {
            const bool isRight = std::fabs(v.normal[0]) > 0.5f, isFar = std::fabs(v.normal[2]) > 0.5f;
            if (!isRight && !isFar) continue;
            const int s = isRight ? 1 : 2;
            const float(*at)[3] = isRight ? right : far;
            int k = 0;
            while (k < 4 && !(v.pos[0] == at[k][0] && v.pos[1] == at[k][1] && v.pos[2] == at[k][2])) ++k;
            CHECK(k < 4);
            if (k == 4) continue;
            ++seen;
            CHECK(std::fabs(v.uv[0] - (0.1f * float(s) + 0.01f * float(k))) < 1e-6f);
            CHECK(std::fabs(v.uv[1] - (0.5f + 0.1f * float(s) + 0.01f * float(k))) < 1e-6f);
            // its lightmap's 6 x 6 middle: corner 0 at its texel (1, 1), 1 at (7, 1), 2 at (1, 7), 3 at (7, 7)
            CHECK(std::fabs(v.lm[0] - float(s * 8 + 1 + (k & 1) * 6) / 2048.0f) < 1e-6f);
            CHECK(std::fabs(v.lm[1] - float(1 + (k >> 1) * 6) / 8.0f) < 1e-6f);
        }
        CHECK_EQ(seen, 8);
    }
}

// 3.0.7-beta.3 (R-YXZM7Y «посмотри насчет освещения стен», c_tower2): a wall lit as uaRO.exe lights it (FUN_004e4190)
// - by its own way across, not as the ground beside it: the shot's walls turned from the sun were 1.6-1.8 times as
// bright as the game's, the floors as bright
TEST(terrain_walls_lit_as_the_game_lights_them) {
    const gnd::Ground g = walled();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const float diffuse[3] = {0.4f, 0.6f, 0.8f}, ambient[3] = {0.2f, 0.3f, 0.1f};
    // longitude 240, latitude 60: the light goes (-0.75, 0.5, -0.433); cell (0, 0) the higher - its right side faces
    // +x (0.75 of the light on it), its far side +z (0.433)
    const terrain::Sun sun = terrain::sunOf(240, 60, diffuse, ambient);
    CHECK(std::fabs(sun.way[0] + 0.75f) < 1e-4f && std::fabs(sun.way[1] - 0.5f) < 1e-4f &&
          std::fabs(sun.way[2] + 0.4330127f) < 1e-4f);
    for (int n : {1, 4}) {
        const terrain::Mesh m = terrain::build(g, n, atlas, nullptr, nullptr, &sun);
        int right = 0, far = 0, tops = 0;
        for (const terrain::Vertex& v : m.vertices) {
            if (!terrain::isWall(v)) {
                ++tops;
                continue;
            }
            // a channel: 255 times min(1, ambient + diffuse * light), cut
            if (std::fabs(v.normal[0]) > 0.5f) {
                ++right;
                CHECK_EQ(v.color, 0xFF7FBFB2u);  // 127 191 178
            } else {
                ++far;
                CHECK_EQ(v.color, 0xFF5F8E71u);  // 95 142 113
            }
        }
        CHECK(right == 4 && far == 4 && tops > 0);
    }
    // turned from the sun: the light all round alone; the surface's own colour in it
    const float up[3] = {0.0f, -1.0f, 0.0f};
    CHECK_EQ(terrain::lit(0xFFFFFFFFu, sun, up), 0xFF334C19u);  // 51 76 25: 0.2 0.3 0.1
    const float toward[3] = {-1.0f, 0.0f, 0.0f};
    CHECK_EQ(terrain::lit(0xFF81FF40u, sun, toward), 0xFF40BF2Cu);  // 129 * 0.5, 255 * 0.75, 64 * 0.7
    // a map without its light: the game's own - longitude and latitude 45, diffuse 1, ambient 0.3
    const terrain::Sun plain{};
    CHECK(std::fabs(plain.way[0] - 0.5f) < 1e-4f && std::fabs(plain.way[1] - 0.7071068f) < 1e-4f &&
          std::fabs(plain.way[2] - 0.5f) < 1e-4f);
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

TEST(terrain_blends_where_two_textures_meet) {
    // 3.0.4-beta.12 (the user chose «Мягкие переходы»): inside each of two cells that meet with other textures, the
    // neighbour's texture over the rows up to the middle - on the ground's own vertices - and the cell's own band
    const gnd::Ground g = flat();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const terrain::Mesh m = terrain::build(g, 3, atlas);
    CHECK(layerBatches(m, terrain::kOwnBand) > 0);
    CHECK(layerBatches(m, terrain::kNeighbour) > 0);
    std::vector<const terrain::Vertex*> ground;
    for (const terrain::Batch& b : m.batches)
        if (b.layer == terrain::kGround)
            for (uint32_t v = b.firstVertex; v < b.firstVertex + b.vertexCount; ++v) ground.push_back(&m.vertices[v]);
    // the seam at x = 0 between grass (1, 0) and dirt (2, 0), z -10..0: the dirt's strip in (1, 0)
    int onGround = 0, strips = 0, beyond = 0, atEdge = 0, lowSide = 0, highSide = 0;
    for (const terrain::Batch& b : m.batches) {
        if (b.layer != terrain::kNeighbour) continue;
        for (uint32_t k = b.firstVertex; k < b.firstVertex + b.vertexCount; ++k) {
            const terrain::Vertex& v = m.vertices[k];
            ++strips;
            for (const terrain::Vertex* x : ground)  // the strips on the ground's own vertices, exactly
                if (x->pos[0] == v.pos[0] && x->pos[1] == v.pos[1] && x->pos[2] == v.pos[2]) {
                    ++onGround;
                    break;
                }
            if (v.pos[2] <= -10.0f || v.pos[2] >= 0.0f) continue;  // along the seam, its ends aside
            const int alpha = int(v.color >> 24), green = int((v.blend >> 8) & 0xFF);
            if (b.texture == 1 && v.pos[0] == 0.0f) {  // at the edge: much of it the dirt's (0.42 by its few cells)
                CHECK(alpha > 90 && alpha <= 128);
                ++atEdge;
                lowSide += green == 255;
            }
            if (b.texture == 1 && v.pos[0] < -5.0f) {  // past the grass cell's middle: none
                CHECK_EQ(alpha, 0);
                ++beyond;
            }
            if (b.texture == 0 && v.pos[0] == 0.0f && green == 0) ++highSide;  // the grass' strip in (2, 0)
        }
    }
    CHECK(strips > 0 && onGround == strips);
    CHECK(atEdge > 0 && beyond > 0);
    // the two strips of one edge count the noise opposite ways: one line between them
    CHECK(lowSide == atEdge && highSide > 0);
}

TEST(terrain_no_blend_over_a_step_nor_far_off_nor_round_a_laid_patch) {
    const gnd::Ground stepped = parsed();  // the dirt 30 below the grass all round
    const terrain::Atlas a1 = terrain::lightmapAtlas(stepped);
    CHECK_EQ(layerBatches(terrain::build(stepped, 3, a1), terrain::kNeighbour), size_t(0));
    const gnd::Ground g = flat();
    const terrain::Atlas a2 = terrain::lightmapAtlas(g);
    CHECK_EQ(layerBatches(terrain::build(g, 1, a2), terrain::kNeighbour), size_t(0));  // the far ground's cells
    std::vector<uint8_t> laid(8, 0);
    laid[2] = laid[3] = 1;  // the dirt laid by hand: its edges straight, as the game draws them
    CHECK_EQ(layerBatches(terrain::build(g, 3, a2, &laid), terrain::kNeighbour), size_t(0));
    CHECK_EQ(layerBatches(terrain::build(g, 3, a2, &laid), terrain::kOwnBand), size_t(0));
}

TEST(terrain_an_own_band_mirrors_its_rim_not_smears_it) {
    // 3.0.4-beta.13 (the user on beta.12: «как будто там замазались стыки однотипной текстурой»): a cell's own band
    // takes its texture from a little inside, mirrored at kBand - beta.12's two rows put one place of the texture on
    // both (inset(1) = inset(0.94) = 0.94), one column of texels stretched over the band at every seam
    const gnd::Ground g = flat();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const terrain::Mesh m = terrain::build(g, 3, atlas);
    int seen = 0, mirrored = 0;  // the grass cell (1, 0)'s band along its seam with the dirt (2, 0) at x = 0
    for (const terrain::Batch& b : m.batches) {
        if (b.layer != terrain::kOwnBand || b.texture != 0) continue;
        for (uint32_t k = b.firstVertex; k < b.firstVertex + b.vertexCount; ++k) {
            const terrain::Vertex& e = m.vertices[k];
            if (e.pos[0] != 0.0f || e.pos[2] <= -10.0f || e.pos[2] >= 0.0f) continue;
            ++seen;
            for (uint32_t q = b.firstVertex; q < b.firstVertex + b.vertexCount; ++q) {
                const terrain::Vertex& o = m.vertices[q];
                const bool inside = o.pos[2] == e.pos[2] && o.pos[0] < 0.0f && o.pos[0] > -1.0f;
                if (inside && std::fabs(o.uv[0] - e.uv[0]) > 0.01f) {
                    ++mirrored;
                    break;
                }
            }
        }
    }
    CHECK(seen > 0 && mirrored == seen);
}

TEST(terrain_water_lies_over_the_cells_under_it) {
    // 3.0.4-beta.20 (the own ground on every map): the map's water as uaRO.exe lays it (0x4e59d0) - a quad over a cell
    // a corner of which lies under the water's highest (its level less the waves' height; up along -y), flat at the
    // level, its texture's coordinates the cell's place modulo four a quarter each; by the ground's chunks
    gnd::Ground g;
    g.width = 10, g.height = 3, g.zoom = 10.0f;
    g.surfaces.resize(1);
    g.cells.resize(30);
    for (gnd::Cell& c : g.cells) {
        c.top = 0;
        for (float& h : c.h) h = 0.0f;
    }
    g.cells[1].h[3] = 8.0f;         // cell (1, 0): its far corner 8 under the ground's 0
    g.cells[5].h[0] = 0.5f;         // cell (5, 0): 0.5 under - in reach of the waves alone
    g.cells[9].h[1] = 3.0f;         // cell (9, 0): the next chunk's
    g.cells[13].h[2] = 2.0f;        // cell (3, 1) - with no top: no water
    g.cells[13].top = -1;
    terrain::Mesh m = terrain::water(g, 1.0f, 0.4f);  // the highest water 0.6
    CHECK_EQ(m.chunks.size(), size_t(2));
    CHECK_EQ(m.vertices.size(), size_t(8));  // cells (1, 0) and (9, 0), four corners each
    CHECK_EQ(m.indices.size(), size_t(12));
    if (m.chunks.size() == 2 && m.vertices.size() == 8) {
        CHECK(m.chunks[0].cellX == 0 && m.chunks[1].cellX == 8);
        CHECK(m.batches.size() == 2 && m.batches[0].vertexCount == 4 && m.batches[1].firstVertex == 4);
        const terrain::Vertex* v = &m.vertices[0];  // cell (1, 0): from x = (1 - 5) * 10, z = (0 - 1.5) * 10
        CHECK(v[0].pos[0] == -40.0f && v[0].pos[2] == -15.0f && v[3].pos[0] == -30.0f && v[3].pos[2] == -5.0f);
        for (int k = 0; k < 4; ++k) CHECK(v[k].pos[1] == 1.0f);  // flat at the level
        CHECK(v[0].uv[0] == 0.25f && v[0].uv[1] == 0.0f && v[3].uv[0] == 0.5f && v[3].uv[1] == 0.25f);
        CHECK(m.vertices[5].uv[0] == 0.5f);  // cell (9, 0): 9 modulo 4 is 1 - its far side at 2 quarters
        CHECK(m.chunks[0].lo[1] == 1.0f && m.chunks[0].hi[1] == 1.0f);
        for (uint32_t i = 0; i < 6; ++i) CHECK(m.indices[i] < 4);  // each batch's own, from its first
    }
    m = terrain::water(g, 1.0f, 1.0f);  // the highest 0: cell (5, 0) too
    CHECK_EQ(m.vertices.size(), size_t(12));
    CHECK(terrain::water(g, 10.0f, 0.0f).vertices.empty());   // the water below every corner (the deepest at 8)
    CHECK_EQ(terrain::water(g, -1.0f, 0.0f).vertices.size(), size_t(29 * 4));  // over all of it: each cell with a top
}

TEST(terrain_knows_the_plain_ground_under_its_smooth_points) {
    // 3.0.4-beta.29 (the user took «Скорость» whole: «плавный переход гладкой земли в дальнюю»): each point of the
    // smooth ground (and its strips) knows the height the plain ground has there - the cell's two triangles as the
    // plain mesh splits them, from (x + 1, y) to (x, y + 1) - so that it is eased onto it before a chunk changes from
    // the one to the other (drawOwnGround): no jump, no crack between them
    gnd::Ground g = flat();
    for (gnd::Cell& c : g.cells) c.h[0] = 0.0f, c.h[1] = -4.0f, c.h[2] = -8.0f, c.h[3] = -6.0f;  // not a plane
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const terrain::Mesh fine = terrain::build(g, 3, atlas);
    int checked = 0;
    for (const terrain::Batch& b : fine.batches) {
        if (b.layer != terrain::kGround) continue;
        for (uint32_t k = b.firstVertex; k < b.firstVertex + b.vertexCount; ++k) {
            const terrain::Vertex& v = fine.vertices[k];
            if (std::fabs(v.normal[1]) < 0.5f) continue;  // a wall: plain as it is
            const float gx = v.pos[0] / g.zoom + float(g.width) / 2.0f, gz = v.pos[2] / g.zoom + float(g.height) / 2.0f;
            const float fu = gx - std::floor(gx + 1e-4f), fv = gz - std::floor(gz + 1e-4f);
            if (fu > 1e-3f && fu < 0.999f && fv > 1e-3f && fv < 0.999f) {  // inside a cell (its edges two cells')
                const float want = fu + fv <= 1.0f ? 0.0f - 4.0f * fu - 8.0f * fv
                                                   : -6.0f - 2.0f * (1.0f - fu) + 2.0f * (1.0f - fv);
                CHECK(std::fabs(v.plain - want) < 1e-3f);
                ++checked;
            }
        }
    }
    CHECK(checked > 0);
    for (const terrain::Vertex& v : terrain::build(g, 1, atlas).vertices)  // the plain ground: itself
        CHECK(v.plain == v.pos[1]);
}


namespace {

// A plateau (up along -y: 10 over the rest) whose last row of corners ends at a slope going down 5 a row - the slope's
// top edge, where the Catmull-Rom patch rises over the plain cells (cmd_fild02's fortress stairs lie on such a slope)
gnd::Ground slopeUnderStairs() {
    gnd::Ground g = flat();
    g.width = 8;
    g.height = 8;
    const auto corner = [](int j) { return j <= 4 ? -10.0f : -10.0f + 5.0f * float(j - 4); };
    g.cells.assign(64, g.cells[0]);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            gnd::Cell& c = g.cells[size_t(y * 8 + x)];
            c.top = 0;
            c.front = c.right = -1;
            c.h[0] = c.h[1] = corner(y);
            c.h[2] = c.h[3] = corner(y + 1);
        }
    return g;
}

}  // namespace

TEST(terrain_no_higher_than_the_plain_cells_where_a_model_rests) {
    // 3.0.6-beta.3 (R-RSK2JR «дирка на сходах в комодо»: cmd_fild02's fortress stairs lie 0.1 over the plain cells of
    // their slope, and the smooth ground rose 0.16-0.29 over their top step - the slope's rock in a round patch over
    // the steps): where a model rests on the ground the smooth ground is no higher than the game's plain cells (up
    // along -y), elsewhere as smooth as before, its points on the cells' shared edges the same from both cells
    const gnd::Ground g = slopeUnderStairs();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const auto above = [&](const terrain::Mesh& m, int row) {  // the most a ground point of a row rises over the plain
        float most = 0.0f;
        for (const terrain::Batch& b : m.batches) {
            if (b.layer != terrain::kGround) continue;
            for (uint32_t k = b.firstVertex; k < b.firstVertex + b.vertexCount; ++k) {
                const terrain::Vertex& v = m.vertices[k];
                const float gz = v.pos[2] / g.zoom + float(g.height) / 2.0f;
                if (gz > float(row) + 1e-3f && gz < float(row + 1) - 1e-3f) most = std::max(most, v.plain - v.pos[1]);
            }
        }
        return most;
    };
    const terrain::Mesh before = terrain::build(g, 3, atlas);
    CHECK(above(before, 4) > 0.2f);  // the slope's top row: the patch over the plain cells
    std::vector<uint8_t> resting(64, 0);
    for (int x = 0; x < 8; ++x) resting[size_t(4 * 8 + x)] = 1;  // the stairs over row 4
    const terrain::Mesh after = terrain::build(g, 3, atlas, nullptr, &resting);
    CHECK(above(after, 4) < 1e-4f);
    CHECK(above(after, 6) == above(before, 6));  // two rows off: as smooth as before
    // no crack of its own: a place of the ground's points that had one height, whichever cell it was of, still has
    // (the smooth cells' edges with the plain ones round the map's border part as before, a few of them no more)
    const auto cracks = [](const terrain::Mesh& m) {
        std::map<std::pair<int, int>, std::vector<float>> at;
        for (const terrain::Batch& b : m.batches) {
            if (b.layer != terrain::kGround) continue;
            for (uint32_t k = b.firstVertex; k < b.firstVertex + b.vertexCount; ++k) {
                const terrain::Vertex& v = m.vertices[k];
                if (std::fabs(v.normal[1]) < 0.5f) continue;  // a wall
                at[{int(std::lround(v.pos[0] * 3.0f)), int(std::lround(v.pos[2] * 3.0f))}].push_back(v.pos[1]);
            }
        }
        std::vector<std::pair<int, int>> out;
        for (const auto& [key, heights] : at)
            for (float h : heights)
                if (std::fabs(h - heights[0]) > 1e-4f) {
                    out.push_back(key);
                    break;
                }
        return out;
    };
    const std::vector<std::pair<int, int>> was = cracks(before), now = cracks(after);
    CHECK(!was.empty());  // the border's, as before
    for (const auto& k : now) CHECK(std::find(was.begin(), was.end(), k) != was.end());  // none new (some gone)
}

TEST(terrain_knows_where_a_model_rests_on_the_ground) {
    // 3.0.6-beta.3: a cell a model's face lies on (from 4 over its plain cells to 1 under them) - not one high over it
    // (a roof, a bridge's top far up) nor one not over it
    const gnd::Ground g = slopeUnderStairs();
    terrain::Mesh models;  // as own_models::place makes them: a batch a quad, its indices from its first vertex
    const auto quad = [&](float x0, float z0, float x1, float z1, float y) {
        terrain::Batch b;
        b.firstVertex = uint32_t(models.vertices.size());
        b.vertexCount = 4;
        b.firstIndex = uint32_t(models.indices.size());
        b.indexCount = 6;
        const float corners[4][2] = {{x0, z0}, {x1, z0}, {x0, z1}, {x1, z1}};
        for (const auto& c : corners) {
            terrain::Vertex v{};
            v.pos[0] = c[0], v.pos[1] = y, v.pos[2] = c[1];
            models.vertices.push_back(v);
        }
        models.indices.insert(models.indices.end(), {0u, 1u, 2u, 1u, 3u, 2u});
        models.batches.push_back(b);
    };
    // cells' places: x from (x - 4) * 10, z from (y - 4) * 10; row 4's plain ground -10 at its top edge to -5
    quad(20.0f, -40.0f, 30.0f, -30.0f, -40.0f);  // a roof 30 over the plateau's cell (6, 0)
    quad(-10.0f, 1.0f, 10.0f, 2.0f, -9.6f);   // a step 0.1 over row 4's plain at z 1..2 (cells 3 and 4 of it)
    const std::vector<uint8_t> r = terrain::restingOn(g, models);
    CHECK_EQ(r.size(), size_t(64));
    CHECK(r[4 * 8 + 3] == 1 && r[4 * 8 + 4] == 1);
    CHECK(r[4 * 8 + 2] == 0 && r[4 * 8 + 5] == 0 && r[3 * 8 + 3] == 0);
    CHECK(r[0 * 8 + 6] == 0);
}
