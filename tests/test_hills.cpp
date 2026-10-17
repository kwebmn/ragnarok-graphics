#include <cmath>

#include "core/hills.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A cell of `side` at (ix, iz) as two triangles, its corners' heights from `height`.
template <typename H>
void cell(std::vector<seams::Triangle>& out, int ix, int iz, float side, H height) {
    seams::Vertex c[4];
    for (int k = 0; k < 4; ++k) {
        c[k].pos[0] = (float(ix) + float(k & 1)) * side + 3.0f;
        c[k].pos[2] = (float(iz) + float(k >> 1)) * side - 7.0f;
        c[k].pos[1] = height(c[k].pos[0], c[k].pos[2]);
    }
    seams::Triangle a, b;
    a.v[0] = c[0], a.v[1] = c[1], a.v[2] = c[2];
    b.v[0] = c[2], b.v[1] = c[1], b.v[2] = c[3];
    out.push_back(a);
    out.push_back(b);
}

}  // namespace

TEST(hills_pack_keeps_a_normal) {
    float x, y, z;
    CHECK(!hills::unpack(0.0f, x, y, z));
    const float v = hills::pack(0.6f, -0.48f);
    CHECK(hills::unpack(v, x, y, z));
    CHECK(std::fabs(x - 0.6f) < 0.002f);
    CHECK(std::fabs(z + 0.48f) < 0.001f);
    CHECK(y < -0.6f);  // upwards
    CHECK(hills::unpack(hills::pack(-1.0f, 1.0f), x, y, z));
    CHECK(float(double(hills::pack(1.0f, 1.0f))) == hills::pack(1.0f, 1.0f));  // a float holds it exactly
}

TEST(hills_flat_ground_points_up) {
    std::vector<seams::Triangle> ground;
    for (int z = 0; z < 4; ++z)
        for (int x = 0; x < 4; ++x) cell(ground, x, z, 10.0f, [](float, float) { return -20.0f; });
    const hills::Grid g = hills::build(ground);
    CHECK(g.side == 10.0f);
    CHECK(g.width == 5 && g.height == 5);
    CHECK(std::fabs(g.originX - 3.0f) < 1e-3f && std::fabs(g.originZ + 7.0f) < 1e-3f);
    float x, y, z;
    CHECK(hills::unpack(g.packed[12], x, y, z));
    CHECK(std::fabs(x) < 0.002f && std::fabs(z) < 0.002f && y < -0.99f);
}

TEST(hills_a_slope_leans_its_normals_the_walls_left_out) {
    std::vector<seams::Triangle> ground;
    // rises along +x (up is -y): the normal leans towards -x
    for (int z = 0; z < 3; ++z)
        for (int x = 0; x < 3; ++x) cell(ground, x, z, 10.0f, [](float px, float) { return -0.3f * px; });
    seams::Triangle wall;  // a cliff's side standing up: no part of the normals
    wall.v[0].pos[0] = 13.0f, wall.v[0].pos[1] = 0.0f, wall.v[0].pos[2] = 3.0f;
    wall.v[1].pos[0] = 13.0f, wall.v[1].pos[1] = -30.0f, wall.v[1].pos[2] = 3.0f;
    wall.v[2].pos[0] = 13.0f, wall.v[2].pos[1] = 0.0f, wall.v[2].pos[2] = 13.0f;
    ground.push_back(wall);
    const hills::Grid g = hills::build(ground);
    float x, y, z;
    CHECK(hills::unpack(g.packed[size_t(1) * size_t(g.width) + 1], x, y, z));
    const float want = 0.3f / std::sqrt(1.09f);
    CHECK(std::fabs(x + want) < 0.01f);
    CHECK(std::fabs(z) < 0.01f);
}
