#include <cmath>

#include "core/forest.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// 나무01 as its file stands: a trunk 6 across, 37.6 high; a crown 42 across from 14.6 to 45.6 up (up along -y)
rsm::Model tree01() {
    rsm::Model m;
    m.lo[0] = -21.1f, m.lo[1] = -45.6f, m.lo[2] = -21.1f;
    m.hi[0] = 21.1f, m.hi[1] = 0.0f, m.hi[2] = 21.1f;
    rsm::Part trunk{"bark", {-2.7f, -37.6f, -2.8f}, {3.1f, 0.0f, 3.0f}, 16};
    rsm::Part crown{"leaf", {-21.1f, -45.6f, -21.1f}, {21.1f, -14.6f, 21.1f}, 48};
    m.parts = {trunk, crown};
    return m;
}

}  // namespace

TEST(forest_kinds_by_the_model_name) {
    const char* tree01 = "\xb3\xaa\xb9\xab\xc0\xe2\xc3\xca\xb2\xc9\\\xb3\xaa\xb9\xab""01.rsm";  // 나무잡초꽃\나무01
    CHECK(forest::kindOf(tree01) == forest::Kind::Broadleaf);
    CHECK(forest::kindOf("x\\\xbe\xdf\xc0\xda\xbc\xf6""02.rsm") == forest::Kind::Palm);           // a palm
    CHECK(forest::kindOf("x\\\xb4\xfd\xba\xd2""01.rsm") == forest::Kind::Bush);                   // a bush
    CHECK(forest::kindOf("x\\\xb3\xaa\xb9\xab\xb1\xe2\xb5\xd5""01.rsm") == forest::Kind::None);   // a wooden post
    CHECK(forest::kindOf("x\\\xb4\xeb\xb3\xaa\xb9\xab""01.rsm") == forest::Kind::None);           // bamboo
    CHECK(forest::kindOf("x\\rock.rsm") == forest::Kind::None);
}

TEST(forest_grows_a_tree_of_the_models_size) {
    forest::Shape s;
    CHECK(forest::shapeOf(forest::Kind::Broadleaf, tree01(), s));
    CHECK(s.kind == forest::Kind::Broadleaf && s.trunkRadius > 0.3f && s.trunkTop > 14.0f && s.trunkTop < 41.0f);
    const forest::Mesh a = forest::grow(s, 7), b = forest::grow(s, 7), c = forest::grow(s, 8);
    CHECK(a.verts.size() > 400 && a.indices.size() % 3 == 0);
    CHECK(a.verts.size() == b.verts.size() && a.verts[100].pos[0] == b.verts[100].pos[0]);  // the same seed
    CHECK(a.verts.size() != c.verts.size() || a.verts[100].pos[0] != c.verts[100].pos[0]);  // its own
    bool inside = true, leaves = false;
    for (const forest::Vertex& v : a.verts) {
        inside = inside && v.pos[1] <= 0.5f && v.pos[1] >= -55.0f && std::fabs(v.pos[0]) < 32.0f;
        leaves = leaves || v.part == 1.0f;
    }
    CHECK(inside && leaves);
    for (uint32_t i : a.indices) CHECK(i < a.verts.size());
    CHECK(a.leafStart > 0 && a.leafStart < a.indices.size() && a.leafStart % 6 == 0);  // 2.1: the trunk's first
}

TEST(forest_a_tree_without_a_trunk_is_a_bush_and_a_palm_has_fronds) {
    rsm::Model m = tree01();
    m.parts.erase(m.parts.begin());
    forest::Shape s;
    CHECK(forest::shapeOf(forest::Kind::Broadleaf, m, s) && s.kind == forest::Kind::Bush);
    forest::Shape p;
    CHECK(forest::shapeOf(forest::Kind::Palm, tree01(), p));
    bool fronds = false;
    for (const forest::Vertex& v : forest::grow(p, 3).verts) fronds = fronds || v.part == 2.0f;
    CHECK(fronds);
}

TEST(forest_average_colour_of_the_opaque_pixels) {
    const uint32_t px[4] = {0xff00ff00u, 0xff0000ffu, 0x00ff0000u, 0xff000000u};  // green, blue, a hole, black
    float c[3];
    CHECK(forest::averageColour(px, 2, 2, c));
    CHECK(std::fabs(c[0]) < 1e-4f && std::fabs(c[1] - 1.0f / 3.0f) < 1e-3f && std::fabs(c[2] - 1.0f / 3.0f) < 1e-3f);
    const uint32_t none[1] = {0x00ffffffu};
    CHECK(!forest::averageColour(none, 1, 1, c));
}

TEST(forest_sakura_and_its_blossom) {
    CHECK(forest::kindOf("x\\\xb9\xfe\xb3\xaa\xb9\xab""01.rsm") == forest::Kind::Sakura);             // 벗나무01
    CHECK(forest::kindOf("x\\\xba\xd0\xc8\xab\xb9\xfe\xb2\xc9.rsm") == forest::Kind::Blossom);        // 분홍벗꽃
    rsm::Model cloud;  // 분홍벗꽃 as its file stands: 34.6 across, 8.8 high
    cloud.lo[0] = -17.3f, cloud.lo[1] = -8.8f, cloud.lo[2] = -11.6f;
    cloud.hi[0] = 17.3f, cloud.hi[1] = 0.0f, cloud.hi[2] = 11.6f;
    cloud.parts = {rsm::Part{"sakura", {-17.3f, -8.8f, -11.6f}, {17.3f, 0.0f, 11.6f}, 24}};
    forest::Shape s;
    CHECK(forest::shapeOf(forest::Kind::Blossom, cloud, s) && s.kind == forest::Kind::Blossom);
    bool petals = false, high = true;
    for (const forest::Vertex& v : forest::grow(s, 5).verts) {
        petals = petals || v.part == 4.0f;
        high = high && v.pos[1] < 3.0f;  // about its own box (up along -y), not pulled down to a bush's ground
    }
    CHECK(petals && high);
}
