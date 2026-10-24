#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "core/rsm.hpp"
#include "sample_files.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(rsm_parts_stand_on_the_ground) {
    const std::vector<uint8_t> b = testfiles::rsm();
    rsm::Model m;
    std::string error;
    CHECK(rsm::parse(b.data(), b.size(), m, error));
    CHECK(m.parts.size() == 2);
    CHECK(m.parts[0].texture == "bark.bmp" && m.parts[0].faces == 2);
    // the bottom (the file's largest y, 10) on the ground: the trunk from 0 up to -10, the crown -8 to -14
    CHECK(std::fabs(m.parts[0].hi[1]) < 1e-4f && std::fabs(m.parts[0].lo[1] + 10.0f) < 1e-4f);
    CHECK(std::fabs(m.parts[1].lo[1] + 14.0f) < 1e-4f && std::fabs(m.parts[1].hi[1] + 8.0f) < 1e-4f);
    CHECK(std::fabs(m.parts[1].hi[0] - m.parts[1].lo[0] - 8.0f) < 1e-4f);
    CHECK(std::fabs((m.lo[0] + m.hi[0]) * 0.5f) < 1e-4f);  // the middle over the place
}

TEST(rsm_faces_whole_as_the_game_stands_the_model) {
    // 3.0.4-beta.14 (the user chose «Дома и деревья вдали»): the faces themselves - their corners where the parts'
    // boxes are (its middle over the place, its bottom on the ground), their texture's coordinates, both sides or one
    const std::vector<uint8_t> b = testfiles::rsmWall();
    rsm::Model m;
    std::string error;
    CHECK(rsm::parse(b.data(), b.size(), m, error));
    CHECK(m.textures.size() == 1 && m.textures[0] == "wall.bmp");
    CHECK_EQ(m.faces.size(), size_t(2));
    if (m.faces.size() != 2) return;
    const rsm::Face& f = m.faces[0];
    CHECK(f.texture == 0 && !f.twoSided && m.faces[1].twoSided);
    // x 1..5 stood over its middle: -2..2; the bottom (y 0) on the ground, the top at -6
    CHECK(std::fabs(f.pos[0][0] + 2.0f) < 1e-5f && std::fabs(f.pos[1][0] - 2.0f) < 1e-5f);
    CHECK(std::fabs(f.pos[0][1]) < 1e-5f && std::fabs(f.pos[2][1] + 6.0f) < 1e-5f);
    CHECK(f.uv[0][0] == 0.0f && f.uv[0][1] == 1.0f && f.uv[2][0] == 1.0f && f.uv[2][1] == 0.0f);
}

TEST(rsm_refuses_what_is_not_a_model) {
    rsm::Model m;
    std::string error;
    const uint8_t junk[8] = {'G', 'R', 'S', 'W', 1, 4, 0, 0};
    CHECK(!rsm::parse(junk, sizeof junk, m, error));
}
