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

TEST(rsm_refuses_what_is_not_a_model) {
    rsm::Model m;
    std::string error;
    const uint8_t junk[8] = {'G', 'R', 'S', 'W', 1, 4, 0, 0};
    CHECK(!rsm::parse(junk, sizeof junk, m, error));
}
