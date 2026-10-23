// 3.0.1-beta.3 (R-SA3W7Y): the flower beds the maps set as models - their tops are the grass's ground too.
#include <cmath>
#include <vector>

#include "core/grass_beds.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

const char kBed[] = "\xb3\xaa\xb9\xab\xc0\xe2\xc3\xca\xb2\xc9\\\xb5\xd5\xb1\xd9\xb2\xc9\xb9\xe7.rsm";  // 둥근꽃밭
const char kWeed[] = "\xc7\xc1\xb7\xd0\xc5\xd7\xb6\xf3\\weed.bmp";  // 프론테라\weed.bmp

bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

// The round bed as rsm::parse leaves it (its middle over the place, its bottom at 0, up along -y): its stone border
// 3 high and its weed top 0.3 over the bottom
rsm::Model bed() {
    rsm::Model m;
    rsm::Part border, weed;
    border.texture = "\xc7\xc1\xb7\xd0\xc5\xd7\xb6\xf3\\pron-flo-bloc.bmp";
    border.lo[0] = -20, border.lo[1] = -3, border.lo[2] = -20;
    border.hi[0] = 20, border.hi[1] = 0, border.hi[2] = 20;
    weed.texture = kWeed;
    weed.lo[0] = -18, weed.lo[1] = -0.3f, weed.lo[2] = -16;
    weed.hi[0] = 17, weed.hi[1] = -0.3f, weed.hi[2] = 19;
    m.parts = {border, weed};
    return m;
}

// A cell of the ground, side 10, its corners at height y
void cell(std::vector<seams::Triangle>& out, int ix, int iz, float y) {
    seams::Vertex c[4];
    for (int k = 0; k < 4; ++k) {
        c[k].pos[0] = float(ix + (k & 1)) * 10.0f;
        c[k].pos[1] = y;
        c[k].pos[2] = float(iz + (k >> 1)) * 10.0f;
    }
    seams::Triangle a, b;
    a.v[0] = c[0], a.v[1] = c[1], a.v[2] = c[2];
    b.v[0] = c[2], b.v[1] = c[1], b.v[2] = c[3];
    out.push_back(a);
    out.push_back(b);
}

}  // namespace

TEST(grass_beds_know_the_round_bed) {
    CHECK(grass_beds::bedModel(kBed));
    CHECK(grass_beds::bedModel("\xb3\xaa\xb9\xab\xc0\xe2\xc3\xca\xb2\xc9\\\xb5\xd5\xb1\xd9\xb2\xc9\xb9\xe7.RSM"));
    CHECK(!grass_beds::bedModel("\xb3\xaa\xb9\xab\xc0\xe2\xc3\xca\xb2\xc9\\\xb3\xaa\xb9\xab" "01.rsm"));  // a tree
    CHECK(grass_beds::bedTexture(kWeed));
    CHECK(grass_beds::bedTexture("\xc7\xc1\xb7\xd0\xc5\xd7\xb6\xf3\\WEED.BMP"));
    CHECK(!grass_beds::bedTexture("\xc7\xc1\xb7\xd0\xc5\xd7\xb6\xf3\\pron-flo-bloc.bmp"));
}

// Where the map stands the bed - its scale (two of prontera's four mirrored), its turn about y, its place - and then
// the game's world (the axes' signs along the ground)
TEST(grass_beds_stand_where_the_map_places_them) {
    rsw::Model at;
    at.file = kBed;
    at.pos[0] = 30, at.pos[1] = 0, at.pos[2] = -10;
    at.scale[0] = -1;
    std::vector<grass_beds::Box> boxes = grass_beds::place(bed(), at);
    CHECK_EQ(boxes.size(), size_t(1));  // the weed's top, not the border
    // x mirrored: -17..18, then +30; z: -16..19, then -10
    CHECK(near(boxes[0].lo[0], 13) && near(boxes[0].hi[0], 48));
    CHECK(near(boxes[0].lo[2], -26) && near(boxes[0].hi[2], 9));
    CHECK(near(boxes[0].lo[1], -0.3f) && near(boxes[0].hi[1], -0.3f));
    const std::vector<grass_beds::Box> world = grass_beds::inWorld(boxes, map::Axes{1, 1, -1});
    CHECK_EQ(world.size(), size_t(1));
    CHECK(near(world[0].lo[0], 13) && near(world[0].hi[0], 48));
    CHECK(near(world[0].lo[2], -9) && near(world[0].hi[2], 26));  // z turned around
    CHECK(grass_beds::inWorld(boxes, map::Axes{}).empty());  // the axes not known: no guess
    at.scale[0] = 1, at.rot[1] = 90;  // a quarter turn about y: x' = z, z' = -x
    boxes = grass_beds::place(bed(), at);
    CHECK_EQ(boxes.size(), size_t(1));
    CHECK(near(boxes[0].lo[0], 14) && near(boxes[0].hi[0], 49));
    CHECK(near(boxes[0].lo[2], -27) && near(boxes[0].hi[2], 8));
    rsm::Model none;  // a model file not read: nothing
    CHECK(grass_beds::place(none, at).empty());
}

// prontera's fountain: the paving at 1.0, the bed's weed at -0.3 - 1.3 over it, past the grass's 0.75
TEST(grass_beds_tops_are_the_grasss_ground) {
    std::vector<seams::Triangle> ground;
    for (int x = 0; x < 4; ++x) cell(ground, x, 0, 1.0f);
    ground_cells::Grid g = ground_cells::build(ground);
    CHECK(!ground_cells::onGround(g, 15.0f, -0.3f, 5.0f, 0.75f));
    grass_beds::Box b;
    b.lo[0] = 11, b.lo[1] = -0.3f, b.lo[2] = 1;
    b.hi[0] = 28, b.hi[1] = -0.3f, b.hi[2] = 9;
    grass_beds::addTo(g, {b});
    CHECK(ground_cells::onGround(g, 15.0f, -0.3f, 5.0f, 0.75f));  // the bed's top
    CHECK(ground_cells::onGround(g, 25.0f, -0.3f, 5.0f, 0.75f));
    CHECK(ground_cells::onGround(g, 15.0f, 1.0f, 5.0f, 0.75f));    // the paving beside it still
    CHECK(!ground_cells::onGround(g, 35.0f, -0.3f, 5.0f, 0.75f));  // a cell the bed does not reach
    CHECK(!ground_cells::onGround(g, 5.0f, -0.3f, 5.0f, 0.75f));
    ground_cells::Grid empty;  // no grid: nothing to do
    grass_beds::addTo(empty, {b});
    CHECK(empty.span.empty());
}
