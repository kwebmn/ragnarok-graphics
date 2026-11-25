// 3.0.23-beta.8 (the owner: «стены комнат на Direct3D 11»): Direct3D 11 has no own ground - the room walls' mesh
// (core/pit_walls) placed by the processor each picture as the game's one-texture vertices, flat in the walls' colour
// lit by their side, cut at the near plane
#include <vector>

#include "core/flat_walls.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
terrain::Mesh quadAt(float z) {  // a wall 10 wide and 20 high, z ahead of the eye, facing it
    terrain::Mesh m;
    const float pos[4][3] = {{-5, 0, z}, {5, 0, z}, {-5, -20, z}, {5, -20, z}};
    for (const auto& p : pos) {
        terrain::Vertex v{};
        v.pos[0] = p[0], v.pos[1] = p[1], v.pos[2] = p[2];
        v.normal[2] = -1.0f;
        m.vertices.push_back(v);
    }
    m.indices = {0, 1, 2, 1, 3, 2};
    return m;
}
own_ground::View lookingAlongZ() {
    own_ground::View v{};
    v.r[0] = 1, v.r[4] = 1, v.r[8] = 1;  // view = world
    return v;
}
}  // namespace

TEST(flat_walls_placed_and_cut_at_the_near_plane) {
    const camera::Projection p = camera::projectionFor(1920, 1080);
    const own_ground::View v = lookingAlongZ();
    flatwalls::Look look;
    look.rgb[0] = 0.5f, look.rgb[1] = 0.4f, look.rgb[2] = 0.3f;
    std::vector<own_ground::Tl1> out;
    flatwalls::place(quadAt(50), v, p, map::Axes{1, 1, 1}, look, out);
    CHECK_EQ(out.size(), size_t(6));  // two triangles ahead
    for (const own_ground::Tl1& t : out) CHECK(t.rhw > 0 && t.z > 0 && t.z < 1);
    const uint32_t c = out[0].diffuse;
    CHECK((c >> 24) == 0xFF && ((c >> 16) & 0xFF) > (c & 0xFF));  // the walls' colour: red over blue
    out.clear();
    flatwalls::place(quadAt(-50), v, p, map::Axes{1, 1, 1}, look, out);
    CHECK(out.empty());  // behind the eye
    out.clear();
    terrain::Mesh across = quadAt(50);
    across.vertices[0].pos[2] = -50, across.vertices[2].pos[2] = -50;  // half of it behind: cut, not dropped
    flatwalls::place(across, v, p, map::Axes{1, 1, 1}, look, out);
    CHECK(!out.empty() && out.size() % 3 == 0);
    for (const own_ground::Tl1& t : out) CHECK(t.rhw > 0);
}
