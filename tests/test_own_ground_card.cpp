// 3.0.20-beta.23 (the owner's «берем», item 1 - «Своя земля» on the card): a map's own ground kept on the card once -
// the smooth mesh's vertices, then the plain one's, the colours and the strips' blends after them; a picture sends
// only its chunks' indices, by layer and texture, on each side of the squeeze
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <vector>

#include "core/own_ground_card.hpp"
#include "test.hpp"

using namespace uagfx;
namespace card = uagfx::own_ground::card;

namespace {

uint32_t bitsOf(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

terrain::Vertex vertexOf(float x, float plain, uint32_t blend) {
    terrain::Vertex v{};
    v.pos[0] = x, v.pos[1] = -2.0f * x, v.pos[2] = 3.0f * x;
    v.uv[0] = x + 0.25f, v.uv[1] = x + 0.5f;
    v.lm[0] = x + 0.75f, v.lm[1] = x + 1.0f;
    v.blend = blend;
    v.plain = plain;
    return v;
}

terrain::Batch batchOf(int texture, uint8_t layer, uint32_t firstVertex, uint32_t vertexCount, uint32_t firstIndex,
                       uint32_t indexCount) {
    terrain::Batch b;
    b.texture = texture;
    b.layer = layer;
    b.firstVertex = firstVertex, b.vertexCount = vertexCount;
    b.firstIndex = firstIndex, b.indexCount = indexCount;
    return b;
}

terrain::Chunk chunkOf(uint32_t firstVertex, uint32_t vertexCount, uint32_t firstBatch, uint32_t batchCount) {
    terrain::Chunk c;
    c.firstVertex = firstVertex, c.vertexCount = vertexCount;
    c.firstBatch = firstBatch, c.batchCount = batchCount;
    return c;
}

// the smooth mesh: chunk 0 - a ground of texture 1 (vertices 0-2), a neighbour's strip of texture 0 (3-5); chunk 1 - a
// ground of a texture the map has none of (6-8)
terrain::Mesh fineMesh() {
    terrain::Mesh m;
    for (int i = 0; i < 9; ++i) m.vertices.push_back(vertexOf(float(i), float(i) + 0.5f, 0x00A0B0C0u + uint32_t(i)));
    m.indices = {0, 1, 2, 2, 1, 0, 0, 2, 1, 0, 1, 2};
    m.batches = {batchOf(1, terrain::kGround, 0, 3, 0, 6), batchOf(0, terrain::kNeighbour, 3, 3, 6, 3),
                 batchOf(7, terrain::kGround, 6, 3, 9, 3)};
    m.chunks = {chunkOf(0, 6, 0, 2), chunkOf(6, 3, 2, 1)};
    return m;
}

// the plain mesh: chunk 0 and chunk 1 - a ground of texture 1 each (vertices 0-2, 3-5)
terrain::Mesh plainMesh() {
    terrain::Mesh m;
    for (int i = 0; i < 6; ++i) m.vertices.push_back(vertexOf(100.0f + float(i), 0.0f, 0x00010203u * uint32_t(i)));
    m.indices = {0, 1, 2, 2, 0, 1};
    m.batches = {batchOf(1, terrain::kGround, 0, 3, 0, 3), batchOf(1, terrain::kGround, 3, 3, 3, 3)};
    m.chunks = {chunkOf(0, 3, 0, 1), chunkOf(3, 3, 1, 1)};
    return m;
}

const card::ParallelFor kBackwards = [](size_t count, const std::function<void(size_t)>& body) {
    for (size_t i = count; i-- > 0;) body(i);
};

}  // namespace

TEST(own_ground_card_lays_the_vertices_then_the_colours_and_the_blends) {
    const terrain::Mesh fine = fineMesh(), plain = plainMesh();
    const card::Layout l = card::layoutOf(card::Meshes{&fine, &plain, nullptr});
    CHECK(l.fine == 9 && l.all == 15 && l.baseOf(1) == 9 && l.baseOf(2) == 15);
    CHECK(l.coloursAt() == 30 && l.blendsAt() == 34 && l.size() == 38);  // 15 colours: four uint4
    const terrain::Mesh empty;
    const card::Layout none = card::layoutOf(card::Meshes{&empty, &empty, nullptr});
    CHECK(none.all == 0 && none.size() == 0);
    // 3.0.20-beta.30: the outskirts (core/outskirts.hpp) a third mesh after the two
    const card::Layout three = card::layoutOf(card::Meshes{&fine, &plain, &plain});
    CHECK(three.all == 21 && three.baseOf(2) == 15);
}

TEST(own_ground_card_keeps_a_vertex_as_two_uint4) {
    const terrain::Mesh fine = fineMesh(), plain = plainMesh();
    std::vector<uint32_t> w(2 * card::kVertexWords, 0u);
    card::vertexWords(card::Meshes{&fine, &plain, nullptr}, 8, 2, w.data());  // the smooth mesh's last, the plain's first
    CHECK(w[0] == bitsOf(8.0f) && w[1] == bitsOf(-16.0f) && w[2] == bitsOf(24.0f) && w[3] == bitsOf(8.5f));
    CHECK(w[4] == bitsOf(8.25f) && w[5] == bitsOf(8.5f) && w[6] == bitsOf(8.75f) && w[7] == bitsOf(9.0f));
    CHECK(w[8] == bitsOf(100.0f) && w[11] == bitsOf(0.0f) && w[12] == bitsOf(100.25f));
    std::vector<uint32_t> b(15, 0u);
    card::blendWords(card::Meshes{&fine, &plain, nullptr}, 0, 15, b.data());
    CHECK(b[0] == 0x00A0B0C0u && b[8] == 0x00A0B0C8u && b[10] == 0x00010203u && b[14] == 0x00010203u * 5u);
    uint32_t piece[2] = {0, 0};
    card::blendWords(card::Meshes{&fine, &plain, nullptr}, 8, 2, piece);  // a piece of them, as the card's are put up
    CHECK(piece[0] == 0x00A0B0C8u && piece[1] == 0u);
}

TEST(own_ground_card_sides_of_the_squeeze_with_a_margin) {
    CHECK(card::hasNear(1400.0f, 1490.0f) && card::hasNear(1495.0f, 1490.0f) && !card::hasNear(1500.0f, 1490.0f));
    CHECK(card::hasPast(1600.0f, 1490.0f) && card::hasPast(1485.0f, 1490.0f) && !card::hasPast(1480.0f, 1490.0f));
}

// a picture's draws: the ground's textures, then the strips', each texture's near side before its past one, a draw's
// batches in the chunks' order; the indices the vertices' numbers on the card (the plain mesh's after the smooth one's)
TEST(own_ground_card_lists_a_pictures_chunks_by_layer_texture_and_side) {
    const terrain::Mesh fine = fineMesh(), plain = plainMesh();
    card::Lists lists;
    lists.begin(2, true);  // the map's two textures; the strips too
    lists.add(fine, 0, 0, true, false);
    lists.add(plain, 1, 1, true, true);  // across the squeeze: on both sides
    lists.add(fine, 0, 1, false, true);
    CHECK(lists.chunks() == 3);
    const std::vector<card::Draw>& d = lists.plan();
    CHECK(d.size() == 4);
    CHECK(d[0].layer == terrain::kGround && d[0].texture == 1 && !d[0].past && d[0].count == 9);  // fine 0, plain 1
    CHECK(d[1].layer == terrain::kGround && d[1].texture == 1 && d[1].past && d[1].count == 3);   // plain 1
    CHECK(d[2].layer == terrain::kGround && d[2].texture == 2 && d[2].past && d[2].count == 3);   // none: white
    CHECK(d[3].layer == terrain::kNeighbour && d[3].texture == 0 && !d[3].past && d[3].count == 3);
    std::vector<std::vector<uint32_t>> into(d.size());
    for (size_t k = 0; k < d.size(); ++k) into[k].assign(d[k].count, 999u);
    lists.fill(card::layoutOf(card::Meshes{&fine, &plain, nullptr}), [&into](size_t k) { return into[k].data(); },
               kBackwards);
    CHECK(into[0] == (std::vector<uint32_t>{0, 1, 2, 2, 1, 0, 14, 12, 13}));  // plain chunk 1: 9 + 3 + its own
    CHECK(into[1] == (std::vector<uint32_t>{14, 12, 13}));
    CHECK(into[2] == (std::vector<uint32_t>{6, 7, 8}));
    CHECK(into[3] == (std::vector<uint32_t>{3, 5, 4}));
}

TEST(own_ground_card_lists_leave_the_strips_out_without_the_blends) {
    const terrain::Mesh fine = fineMesh(), plain = plainMesh();
    card::Lists lists;
    lists.begin(2, false);
    lists.add(fine, 0, 0, true, true);
    const std::vector<card::Draw>& d = lists.plan();
    CHECK(d.size() == 2 && d[0].layer == terrain::kGround && !d[0].past && d[1].past && d[1].count == 6);
    lists.begin(2, true);
    CHECK(lists.chunks() == 0 && lists.plan().empty());
}

namespace {

// the camera 300 above the ground looking down along the world's y at 45 degrees, as the game's own view
card::Putting puttingOf() {
    card::Putting pu;
    const float c = 0.70710678f;
    const float viewToWorld[12] = {1, 0, 0, 0, c, -c, 0, c, c, 50, -300, -40};  // its x, y (down), z (ahead)
    CHECK(own_ground::viewOf(viewToWorld, pu.view));
    pu.p.f0 = pu.p.f1 = 900.0f;
    pu.p.cx = 400.0f, pu.p.cy = 300.0f;
    pu.axes = map::Axes{1, -1, 1};
    pu.morphFrom = 400.0f, pu.fineDepth = 600.0f;
    pu.fogA = 250.0f, pu.fogB = -0.1f, pu.fogRgb = 0x00203040u;
    return pu;
}

uint32_t lcg(uint32_t& s) { return s = s * 1664525u + 1013904223u; }
float unit(uint32_t& s) { return float(lcg(s) >> 8) / 16777216.0f; }

}  // namespace

// the processor's way of putting a vertex (ownChunk's, now in core): where the game would put it, the smooth ground
// eased onto the plain one past morphFrom, the strips' depth a little nearer, the fog by the line
TEST(own_ground_card_puts_a_vertex_as_the_processor_did) {
    const card::Putting pu = puttingOf();
    terrain::Vertex v = vertexOf(0.0f, 0.0f, 0x00112233u);
    v.pos[0] = 60.0f, v.pos[1] = 10.0f, v.pos[2] = 200.0f, v.plain = 10.0f;
    own_ground::Tl2 t{}, want{};
    CHECK(card::putVertex(pu, v, true, terrain::kGround, t));
    CHECK(own_ground::place(pu.view, pu.p, pu.axes, v.pos, want));
    CHECK(t.x == want.x && t.y == want.y && t.z == want.z && t.rhw == want.rhw);
    CHECK(t.u0 == v.uv[0] && t.v1 == v.lm[1]);
    const float zd = 1.0f / want.rhw;
    const int fog = std::clamp(int(std::lround(250.0f - 0.1f * zd)), 0, 255);
    CHECK(t.specular == (uint32_t(fog) << 24 | 0x00203040u));
    own_ground::Tl2 strip{};
    CHECK(card::putVertex(pu, v, true, terrain::kNeighbour, strip));
    CHECK(strip.x == want.x && strip.z == own_ground::depthOf(pu.p, zd - 0.05f));
    CHECK(strip.specular == (uint32_t(fog) << 24 | 0x00112233u));
    // far off (past fineDepth): on the plain height - the plain mesh's own never eased
    terrain::Vertex far = v;
    far.pos[2] = 2000.0f, far.pos[1] = 10.0f, far.plain = 30.0f;
    own_ground::Tl2 flat{};
    CHECK(card::putVertex(pu, far, false, terrain::kGround, flat));  // the plain mesh's: never eased
    CHECK(own_ground::place(pu.view, pu.p, pu.axes, far.pos, want) && flat.y == want.y);
    CHECK(card::putVertex(pu, far, true, terrain::kGround, flat));
    CHECK(flat.y != want.y);
    terrain::Vertex behind = v;
    behind.pos[2] = -500.0f;
    CHECK(!card::putVertex(pu, behind, true, terrain::kGround, t));
}

// the card's way (ground_vs, the same sums): the game's screen place and depth times the view's depth, w the view's
// depth - the same place, depth and fog as the processor's for any vertex in front, on its side of the squeeze; the
// cut's distances: the squeeze's side and the near plane, kept where not below 0
TEST(own_ground_card_clips_a_vertex_where_the_processor_puts_it) {
    const card::Putting pu = puttingOf();
    const float from = own_ground::ownFrom(pu.p);
    uint32_t s = 7;
    int checked = 0, past = 0;
    for (int n = 0; n < 4000; ++n) {
        terrain::Vertex v = vertexOf(0.0f, 0.0f, lcg(s) & 0xFFFFFFu);
        v.pos[0] = unit(s) * 2400.0f - 1200.0f;
        v.pos[1] = unit(s) * 60.0f - 30.0f;
        v.pos[2] = unit(s) * 3200.0f - 200.0f;
        v.plain = v.pos[1] + unit(s) * 8.0f - 4.0f;
        const bool smooth = (n & 1) != 0;
        const uint8_t layer = uint8_t(n % 3);
        own_ground::Tl2 t{};
        if (!card::putVertex(pu, v, smooth, layer, t)) continue;
        const float zd = 1.0f / t.rhw;
        if (std::fabs(zd - from) < 1.0f || zd - 0.2f < pu.p.nearZ) continue;  // on the cut, or by the near plane
        const bool beyond = zd > from;
        const card::Clip c = card::clipOf(pu, v, smooth, layer, beyond);
        CHECK(std::fabs(c.w - zd) <= 1e-3f * zd);
        CHECK(std::fabs(c.x / c.w - t.x) < 0.02f && std::fabs(c.y / c.w - t.y) < 0.02f);
        CHECK(std::fabs(c.z / c.w - t.z) < 2e-6f);
        // the fog's alpha by a step at most (the card's depth not the processor's 1 / rhw), its colour the same
        CHECK(std::abs(int(c.specular >> 24) - int(t.specular >> 24)) <= 1);
        CHECK((c.specular & 0xFFFFFFu) == (t.specular & 0xFFFFFFu));
        CHECK(c.cut[0] > 0.0f && c.cut[1] > 0.0f);
        const card::Clip other = card::clipOf(pu, v, smooth, layer, !beyond);
        CHECK(other.cut[0] < 0.0f);  // the other side's draw cuts it away
        ++checked;
        past += beyond ? 1 : 0;
    }
    CHECK(checked > 1500 && past > 300);
    terrain::Vertex behind = vertexOf(0.0f, 0.0f, 0u);
    behind.pos[2] = -500.0f;
    CHECK(card::clipOf(pu, behind, true, terrain::kGround, false).cut[1] < 0.0f);
}

// 3.0.20-beta.30 (the owner's «6,7,8,9», 6: the land past a map's edges, core/outskirts.hpp): a third mesh's chunks
// drawn with the others, their vertices' numbers after the smooth and the plain ones'
TEST(own_ground_card_lists_take_the_outskirts_after_the_two) {
    const terrain::Mesh fine = fineMesh(), plain = plainMesh();
    const terrain::Mesh outskirts = plainMesh();
    card::Lists lists;
    lists.begin(2, true);
    lists.add(outskirts, 2, 0, true, false);
    const std::vector<card::Draw>& d = lists.plan();
    CHECK(d.size() == 1 && d[0].texture == 1 && d[0].count == 3);
    std::vector<uint32_t> into(3, 999u);
    lists.fill(card::layoutOf(card::Meshes{&fine, &plain, &outskirts}), [&into](size_t) { return into.data(); },
               kBackwards);
    CHECK(into == (std::vector<uint32_t>{15, 16, 17}));  // 9 + 6 + its own 0 1 2
    std::vector<uint32_t> w(card::kVertexWords, 0u);
    card::vertexWords(card::Meshes{&fine, &plain, &outskirts}, 15, 1, w.data());  // the outskirts' first
    CHECK(w[0] == bitsOf(100.0f));
}
