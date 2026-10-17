#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "core/rsm.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

struct Bytes {
    std::vector<uint8_t> b;
    template <typename T>
    void put(T v) {
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        b.insert(b.end(), p, p + sizeof v);
    }
    void name(const std::string& s, size_t width) {
        std::string t = s;
        t.resize(width, '\0');
        b.insert(b.end(), t.begin(), t.end());
    }
};

// A model of version 1.4, one node: a trunk (texture 0, two faces) 2 wide, 10 tall over y 0-10 of the file, and a
// crown (texture 1, one face) 8 wide over y -4 to 2 - the file's y down, as the game's (up along -y).
std::vector<uint8_t> tree() {
    Bytes w;
    w.b = {'G', 'R', 'S', 'M', 1, 4};
    w.put<int32_t>(0);
    w.put<int32_t>(1);
    w.put<uint8_t>(255);
    for (int i = 0; i < 16; ++i) w.put<uint8_t>(0);
    w.put<int32_t>(2);
    w.name("bark.bmp", 40);
    w.name("leaf.bmp", 40);
    w.name("main", 40);
    w.put<int32_t>(1);
    w.name("main", 40);
    w.name("", 40);
    w.put<int32_t>(2);
    w.put<int32_t>(0);
    w.put<int32_t>(1);
    const float mat[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    for (float f : mat) w.put(f);
    for (int i = 0; i < 3; ++i) w.put(0.0f);  // offset
    for (int i = 0; i < 3; ++i) w.put(0.0f);  // pos
    w.put(0.0f);                              // angle
    w.put(0.0f), w.put(1.0f), w.put(0.0f);    // axis
    w.put(1.0f), w.put(1.0f), w.put(1.0f);    // scale
    const float v[][3] = {{-1, 10, 0}, {1, 10, 0}, {1, 0, 0}, {-1, 0, 0}, {-4, 2, -4}, {4, 2, 4}, {0, -4, 0}};
    w.put<int32_t>(7);
    for (const auto& p : v)
        for (float f : p) w.put(f);
    w.put<int32_t>(0);  // texture coordinates
    w.put<int32_t>(3);
    const uint16_t faces[][4] = {{0, 1, 2, 0}, {0, 2, 3, 0}, {4, 5, 6, 1}};
    for (const auto& f : faces) {
        w.put(f[0]), w.put(f[1]), w.put(f[2]);
        w.put<uint16_t>(0), w.put<uint16_t>(0), w.put<uint16_t>(0);
        w.put(f[3]);
        w.put<uint16_t>(0);
        w.put<int32_t>(0);
        w.put<int32_t>(0);
    }
    w.put<int32_t>(0);  // rotation keys
    return w.b;
}

}  // namespace

TEST(rsm_parts_stand_on_the_ground) {
    const std::vector<uint8_t> b = tree();
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
