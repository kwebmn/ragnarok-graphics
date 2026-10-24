#include <cmath>
#include <string>
#include <vector>

#include "core/own_models.hpp"
#include "core/rsm.hpp"
#include "core/rsw.hpp"
#include "sample_files.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.4-beta.14 (the user chose «Дома и деревья вдали»): the map's models placed as the game places them, in chunks for
// the renderer to draw past the game's far plane
TEST(own_models_stand_where_the_map_puts_them) {
    const std::vector<uint8_t> b = testfiles::rsmWall();
    rsm::Model wall;
    std::string error;
    CHECK(rsm::parse(b.data(), b.size(), wall, error));
    rsw::Model at;
    at.file = "wall.rsm";
    at.pos[0] = 100.0f, at.pos[1] = -3.0f, at.pos[2] = 50.0f;
    at.rot[1] = 90.0f;  // turned about the up axis: its x along the map's z
    std::vector<std::string> textures;
    const terrain::Mesh m = own_models::place({at}, {&wall}, 200, 200, 10.0f, textures);
    CHECK(textures.size() == 1 && textures[0] == "wall.bmp");
    CHECK_EQ(m.chunks.size(), size_t(1));
    CHECK_EQ(m.indices.size(), size_t(6));
    if (m.chunks.empty()) return;
    const terrain::Chunk& c = m.chunks[0];
    // the wall's x -2..2 now along z about 50, at x 100, from its place's height (-3) up to -9
    CHECK(std::fabs(c.lo[0] - 100.0f) < 1e-4f && std::fabs(c.hi[0] - 100.0f) < 1e-4f);
    CHECK(std::fabs(c.lo[2] - 48.0f) < 1e-4f && std::fabs(c.hi[2] - 52.0f) < 1e-4f);
    CHECK(std::fabs(c.hi[1] + 3.0f) < 1e-4f && std::fabs(c.lo[1] + 9.0f) < 1e-4f);
    // its faces' normals across the wall (along x), unit
    for (const terrain::Vertex& v : m.vertices) CHECK(std::fabs(std::fabs(v.normal[0]) - 1.0f) < 1e-4f);
    // its texture's coordinates kept, the batch its texture's
    CHECK(m.batches.size() == 1 && m.batches[0].texture == 0);
    // the corners the two faces share are one vertex each: 4, not 6 (each projected once a picture)
    CHECK_EQ(m.vertices.size(), size_t(4));
}

TEST(own_models_leave_out_what_is_not_read) {
    rsw::Model at;
    std::vector<std::string> textures;
    const terrain::Mesh m = own_models::place({at}, {nullptr}, 200, 200, 10.0f, textures);
    CHECK(m.chunks.empty() && textures.empty());
}

TEST(own_models_learn_the_game_s_colours) {
    // the game's own draws of its models near: a triangle of theirs where one of ours lies gives its corners' colours;
    // the rest lit as the game lights those seen, by how much they face the map's light
    const std::vector<uint8_t> b = testfiles::rsmWall();
    rsm::Model wall;
    std::string error;
    CHECK(rsm::parse(b.data(), b.size(), wall, error));
    rsw::Model at;
    at.pos[0] = 100.0f, at.pos[2] = 50.0f;
    std::vector<std::string> textures;
    const terrain::Mesh m = own_models::place({at}, {&wall}, 200, 200, 10.0f, textures);
    const float toLight[3] = {0.0f, -1.0f, 0.0f};  // straight above
    own_models::Learning l;
    l.reset(m, toLight);
    CHECK(m.vertices.size() == 4 && m.indices.size() == 6);
    if (m.vertices.size() != 4 || m.indices.size() != 6) return;
    // the game's first triangle of the wall, as it reached the screen and back (a hair off)
    float corners[3][3];
    uint32_t colours[3] = {0xFF102030u, 0xFF405060u, 0xFF708090u};
    for (int k = 0; k < 3; ++k)
        for (int a = 0; a < 3; ++a) corners[k][a] = m.vertices[m.indices[k]].pos[a] + (a == 0 ? 0.04f : -0.03f);
    CHECK(l.note(corners, colours));
    for (int k = 0; k < 3; ++k) CHECK_EQ(l.colourOf(m.indices[k]), colours[k]);
    CHECK(l.seen() == 1 && l.matched() == 1);
    // 3.0.4-beta.20: the same triangle again - matched, nothing learnt anew (the renderer's record drawn again then;
    // R-35N5QN: matched() grew every picture, and the record was never drawn again)
    const size_t learnt = l.changes();
    CHECK(learnt == 3);
    CHECK(l.note(corners, colours));
    CHECK(l.matched() == 2 && l.changes() == learnt);
    // one where none of ours lies: not ours, yet lit as the map lights - a flat face (facing up) bright, an upright
    // one dim
    float flat[3][3] = {{0, 0, 0}, {10, 0, 0}, {0, 0, 10}}, upright[3][3] = {{0, 0, 0}, {10, 0, 0}, {0, -10, 0}};
    const uint32_t bright[3] = {0xFFC8C8C8u, 0xFFC8C8C8u, 0xFFC8C8C8u};
    const uint32_t dim[3] = {0xFF505050u, 0xFF505050u, 0xFF505050u};
    for (int i = 0; i < 20; ++i) {
        CHECK(!l.note(flat, bright));
        CHECK(!l.note(upright, dim));
    }
    l.fit();
    // the wall's second triangle's own corner (upright, never seen): as the game lit the upright ones
    uint32_t unseen = 0;
    for (int k = 3; k < 6; ++k)
        if (m.indices[k] != m.indices[0] && m.indices[k] != m.indices[1] && m.indices[k] != m.indices[2])
            unseen = m.indices[k];
    const uint32_t c = l.colourOf(unseen);
    CHECK(std::abs(int(c & 0xFF) - 0x50) <= 2);
}
