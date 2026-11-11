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

namespace {

// 3.0.5-beta.13: what the card holds of a Store - each size's slots, as seam_vs reads them
struct Card {
    std::vector<seams::CardVertex> vertices[2];
    std::vector<uint16_t> indices[2];
    void take(const seams::Store& s) {
        for (uint32_t size = 0; size < 2; ++size) {
            vertices[size].resize(size_t(s.slotsUsed(size)) * seams::kSlotVertices[size]);
            indices[size].resize(size_t(s.slotsUsed(size)) * seams::kSlotIndices[size]);
            const std::vector<uint32_t>& slots = s.freshSlots(size);
            for (size_t n = 0; n < slots.size(); ++n) {
                std::copy_n(s.freshVertices(size).begin() + std::ptrdiff_t(n * seams::kSlotVertices[size]),
                            seams::kSlotVertices[size],
                            vertices[size].begin() + std::ptrdiff_t(size_t(slots[n]) * seams::kSlotVertices[size]));
                std::copy_n(s.freshIndices(size).begin() + std::ptrdiff_t(n * seams::kSlotIndices[size]),
                            seams::kSlotIndices[size],
                            indices[size].begin() + std::ptrdiff_t(size_t(slots[n]) * seams::kSlotIndices[size]));
            }
        }
    }
};

// The view's cells numbered anew in the order they come, as the game's draws are each frame (Strip's groups), and
// each one's texture
void numberAnew(std::vector<seams::Triangle>& view, const std::vector<uint32_t>& worldTextures,
                std::vector<uint32_t>& textures) {
    textures.clear();
    for (size_t t = 0; t < view.size(); ++t) {
        if (t % 2 == 0) textures.push_back(worldTextures[view[t].group]);
        view[t].group = uint32_t(textures.size() - 1);
    }
}

// The store's pieces in view against the strips built afresh: the same triangles of each strip, vertex by vertex -
// the world's places within `far`, the colours, coordinates and flags the same, the fog of the cell's corners by
// their shares within 1; every strip's triangles shown, none twice. The pieces' count.
size_t sameAsAfresh(const seams::Store& store, const Card& card, const std::vector<seams::Strip>& strips, float far) {
    size_t pieces = 0, bad = 0;
    std::vector<size_t> at(strips.size(), 0);
    for (const seams::Shown& s : store.shown()) {
        size_t n = 0;
        while (n < strips.size() && !(strips[n].inside == s.inside && strips[n].shows == s.shows)) ++n;
        if (n == strips.size() || at[n] + s.indices > strips[n].indices.size()) {
            ++bad;
            continue;
        }
        for (uint32_t k = 0; k < s.indices; ++k) {
            const seams::Vertex& v = strips[n].vertices[strips[n].indices[at[n] + k]];
            const uint16_t local = card.indices[s.size][size_t(s.slot) * seams::kSlotIndices[s.size] + k];
            const seams::CardVertex& c = card.vertices[s.size][size_t(s.slot) * seams::kSlotVertices[s.size] + local];
            float fog = 0.0f;
            for (int corner = 0; corner < 4; ++corner)
                fog += float(c.share[corner]) / 65535.0f * float((s.fog >> (8 * corner)) & 255u);
            const bool same = std::fabs(c.pos[0] - v.pos[0]) <= far && std::fabs(c.pos[1] - v.pos[1]) <= far &&
                              std::fabs(c.pos[2] - v.pos[2]) <= far && c.diffuse == v.diffuse &&
                              c.uv0[0] == v.uv0[0] && c.uv0[1] == v.uv0[1] && c.uv1[0] == v.uv1[0] &&
                              c.uv1[1] == v.uv1[1] && c.specular == (v.specular & 0x00FFFFFFu) &&
                              std::fabs(std::floor(fog + 0.5f) - float(v.specular >> 24)) <= 1.0f;
            bad += same ? 0 : 1;
        }
        at[n] += s.indices;
        ++pieces;
    }
    for (size_t n = 0; n < strips.size(); ++n) bad += at[n] == strips[n].indices.size() ? 0 : 1;
    CHECK_EQ(bad, size_t(0));
    return pieces;
}

const float kFarAhead[4] = {0.0f, 0.0f, 0.0f, 1000.0f};  // every place 1000 ahead of the camera

}  // namespace

// The same ground again (the camera standing): nothing made, the same pieces shown
TEST(seams_store_makes_nothing_for_the_same_ground) {
    std::vector<seams::Triangle> world;
    std::vector<uint32_t> textures;
    varied(world, textures, 12);
    seams::Store store;
    store.build(world, textures, 0.5f, kFarAhead, 10.0f);
    const size_t made = store.made();
    const size_t shown = store.shown().size();
    store.build(world, textures, 0.5f, kFarAhead, 10.0f);
    CHECK_EQ(store.made(), made);
    CHECK_EQ(store.shown().size(), shown);
    CHECK(store.freshSlots(seams::kPlain).empty() && store.freshSlots(seams::kCut).empty());
}

// An edge of a cell that reaches the camera's near plane is not shown (applySeams left out such a strip)
TEST(seams_store_shows_no_edge_whose_cell_reaches_the_near_plane) {
    std::vector<seams::Triangle> world;
    std::vector<uint32_t> textures;
    varied(world, textures, 12);
    seams::Store store;
    store.build(world, textures, 0.5f, kFarAhead, 10.0f);
    const size_t all = store.shown().size();
    const float depth[4] = {1.0f, 0.0f, 0.0f, 0.0f};  // the depth is x: the cells with x <= 10 too near
    store.build(world, textures, 0.5f, depth, 10.0f);
    CHECK(store.shown().size() < all);
    CHECK(!store.shown().empty());
    CHECK_EQ(store.made(), all);  // nothing made anew for it
}

// Edges long out of view give their slots up for the new ones: the card's room stays as much as a few views need,
// however far the camera goes; clear() makes everything anew
TEST(seams_store_gives_up_the_slots_of_edges_long_out_of_view) {
    const int side = 96;
    std::vector<seams::Triangle> world;
    std::vector<uint32_t> textures;
    varied(world, textures, side);
    seams::Store store;
    uint32_t most = 0;
    for (int f = 0; f < 2400; ++f) {
        const int step = f / 4, x = step % 160 < 80 ? step % 80 : 80 - step % 80;  // along the world and back
        std::vector<seams::Triangle> view = viewOf(world, side, x, (step / 10) % 80, 14, uint32_t(f), 0.0f, 0.0f);
        std::vector<uint32_t> viewTextures;
        numberAnew(view, textures, viewTextures);
        store.build(view, viewTextures, 0.5f, kFarAhead, 10.0f);
        most = std::max(most, store.slotsUsed(seams::kPlain) + store.slotsUsed(seams::kCut));
    }
    std::printf("  seams store: at most %u slots over 2400 frames, %zu pieces made\n", most, store.made());
    CHECK(most < 12000);  // the whole world's pieces: 9216 cells, about 5 each
    store.clear();
    std::vector<seams::Triangle> view = viewOf(world, side, 0, 0, 14, 1u, 0.0f, 0.0f);
    std::vector<uint32_t> viewTextures;
    numberAnew(view, textures, viewTextures);
    store.build(view, viewTextures, 0.5f, kFarAhead, 10.0f);
    CHECK_EQ(store.freshSlots(seams::kPlain).size() + store.freshSlots(seams::kCut).size(), store.shown().size());
}

