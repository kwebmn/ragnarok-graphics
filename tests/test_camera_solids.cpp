// 3.0.20-beta.70 (the owner's pick 1 of the camera's options, «упираться в настоящие стены и модели»): the behind
// camera's way back from the head, and the free camera's step, tried against the models' own triangles as the game
// drew them - not their boxes, which stop the eye at a gate's opening, under a roof's overhang and beside a house's
// corner, and let it through any model whose box the character stands in
#include <cmath>
#include <vector>

#include "core/camera_solids.hpp"
#include "core/view3d.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// corners of an upright wall in the plane x = `x`, z from z0 to z1, y (down) from y0 to y1; two triangles
void wall(std::vector<float>& v, std::vector<solids::Face>& f, float x, float z0, float z1, float y0, float y1,
          bool open = false) {
    const uint16_t base = uint16_t(v.size() / 3);
    const float corners[4][3] = {{x, y0, z0}, {x, y0, z1}, {x, y1, z0}, {x, y1, z1}};
    for (const auto& c : corners) v.insert(v.end(), c, c + 3);
    f.push_back(solids::Face{{base, uint16_t(base + 1), uint16_t(base + 2)}, open});
    f.push_back(solids::Face{{uint16_t(base + 1), uint16_t(base + 3), uint16_t(base + 2)}, open});
}

solids::Set oneWall(float x = 10.0f) {
    std::vector<float> v;
    std::vector<solids::Face> f;
    wall(v, f, x, -20.0f, 20.0f, -40.0f, 0.0f);
    solids::Set s;
    s.add(1, v.data(), v.size() / 3, f.data(), f.size());
    return s;
}

}  // namespace

TEST(solids_a_way_meets_a_wall_from_either_side) {
    const solids::Set s = oneWall();
    CHECK_EQ(s.parts(), size_t(1));
    CHECK_EQ(s.triangles(), size_t(2));
    CHECK(s.has(1) && !s.has(2));
    const float head[3] = {0, -14, 0}, eye[3] = {40, -20, 0};
    const float share = s.meets(head, eye);
    CHECK(share > 0.24f && share < 0.26f);  // the wall a quarter of the way
    CHECK(std::fabs(s.meets(eye, head) - 0.75f) < 0.01f);  // from behind it: a wall has no side for the camera
    const float beside[3] = {40, -20, 200};  // past the wall's end (50 aside where the wall stands)
    CHECK(s.meets(head, beside) < 0.0f);
    const float over[3] = {40, -200, 0};  // over its top (60 up where the wall stands)
    CHECK(s.meets(head, over) < 0.0f);
    const float shortOf[3] = {8, -14, 0};  // the way ends before it
    CHECK(s.meets(head, shortOf) < 0.0f);
    solids::Set none;
    CHECK(none.meets(head, eye) < 0.0f);
}

// A gate: two posts and a lintel - its box is whole, its opening is not. The way through the opening is free.
TEST(solids_a_gates_opening_lets_the_way_through) {
    std::vector<float> v;
    std::vector<solids::Face> f;
    wall(v, f, 10.0f, -20.0f, -8.0f, -40.0f, 0.0f);  // the left post
    wall(v, f, 10.0f, 8.0f, 20.0f, -40.0f, 0.0f);    // the right post
    wall(v, f, 10.0f, -8.0f, 8.0f, -40.0f, -28.0f);  // the lintel over the opening (y down: -28 is 28 up)
    solids::Set s;
    s.add(7, v.data(), v.size() / 3, f.data(), f.size());
    const float head[3] = {0, -14, 0}, through[3] = {40, -16, 0}, post[3] = {40, -16, -60}, lintel[3] = {40, -90, 0};
    CHECK(s.meets(head, through) < 0.0f);
    CHECK(s.meets(head, post) > 0.0f);
    CHECK(s.meets(head, lintel) > 0.0f);
    // the model's box holds the opening: the old way stopped there
    view3d::Box box;
    box.centre[0] = 10, box.centre[1] = -20, box.centre[2] = 0;
    box.half[0] = 1, box.half[1] = 20, box.half[2] = 20;
    CHECK(view3d::entersAt(box, head, through) > 0.0f);
}

TEST(solids_the_eye_stops_a_step_before_a_wall) {
    const solids::Set s = oneWall(-30.0f);  // behind the character, who looks along +x
    const float target[3] = {0, -14, 0};
    const float eye[3] = {-45, -14, 0};  // 45 back
    const float free = solids::freeOfSolids(eye, target, 45.0f, s, 0.0f);
    CHECK(std::fabs(free - (30.0f - view3d::kPullStep)) < 0.01f);
    // nothing on the way: as far as asked
    const float up[3] = {0, -100, 0};
    CHECK_EQ(solids::freeOfSolids(up, target, 45.0f, s, 0.0f), 45.0f);
    // a wall at the head's back: never nearer than the nearest the camera comes
    const solids::Set close = oneWall(-3.0f);
    CHECK_EQ(solids::freeOfSolids(eye, target, 45.0f, close, 0.0f), view3d::kPullNearest);
    // the eye has a body: a wall's end two units beside the way stops it too
    std::vector<float> v;
    std::vector<solids::Face> f;
    wall(v, f, -30.0f, 1.5f, 40.0f, -40.0f, 0.0f);  // starts a unit and a half to the side of the way
    solids::Set edge;
    edge.add(3, v.data(), v.size() / 3, f.data(), f.size());
    CHECK_EQ(solids::freeOfSolids(eye, target, 45.0f, edge, 0.0f), 45.0f);  // a bare line passes
    CHECK(solids::freeOfSolids(eye, target, 45.0f, edge, 3.0f) < 30.0f);    // three units wide does not
}

// Leaves, a fence, a grate: faces whose picture has holes. The camera goes through them (a crown's card over the head
// would pull it in at every step under a tree)
TEST(solids_leaves_do_not_stop_the_camera) {
    std::vector<float> v;
    std::vector<solids::Face> f;
    wall(v, f, 10.0f, -20.0f, 20.0f, -40.0f, 0.0f, true);  // a leaf card
    wall(v, f, 20.0f, -15.0f, 15.0f, -40.0f, 0.0f);        // a house's wall behind it
    solids::Set s;
    s.add(5, v.data(), v.size() / 3, f.data(), f.size());
    CHECK_EQ(s.triangles(), size_t(2));
    CHECK_EQ(s.leftOut(), size_t(2));
    const float head[3] = {0, -14, 0}, eye[3] = {40, -14, 0};
    CHECK(std::fabs(s.meets(head, eye) - 0.5f) < 0.01f);  // the wall, not the leaves
    // a node of leaves alone is known all the same: its box must not stand in for it
    std::vector<float> lv;
    std::vector<solids::Face> lf;
    wall(lv, lf, 10.0f, -20.0f, 20.0f, -40.0f, 0.0f, true);
    solids::Set crown;
    crown.add(9, lv.data(), lv.size() / 3, lf.data(), lf.size());
    CHECK(crown.has(9));
    CHECK_EQ(crown.triangles(), size_t(0));
}

TEST(solids_the_set_keeps_each_node_once_and_within_its_bound) {
    std::vector<float> v;
    std::vector<solids::Face> f;
    wall(v, f, 10.0f, -20.0f, 20.0f, -40.0f, 0.0f);
    solids::Set s;
    s.add(1, v.data(), v.size() / 3, f.data(), f.size());
    s.add(1, v.data(), v.size() / 3, f.data(), f.size());  // told again: as it was
    CHECK_EQ(s.triangles(), size_t(2));
    // a face of a corner past the vertices, of a size never seen: left out, the rest kept
    std::vector<solids::Face> bad = f;
    bad.push_back(solids::Face{{0, 1, 99}, false});
    s.add(2, v.data(), v.size() / 3, bad.data(), bad.size());
    CHECK_EQ(s.triangles(), size_t(4));
    const float huge[9] = {0, 0, 0, 1e7f, 0, 0, 0, 0, 1e7f};
    const solids::Face one{{0, 1, 2}, false};
    s.add(3, huge, 3, &one, 1);
    CHECK_EQ(s.triangles(), size_t(4));
    // the bound: no more than kMostTriangles in all - a node that would pass it is not taken (its box stands for it)
    solids::Set full;
    std::vector<float> many;
    std::vector<solids::Face> faces;
    for (int i = 0; i < 400; ++i) wall(many, faces, float(i), -1.0f, 1.0f, -1.0f, 0.0f);
    uint32_t id = 1;
    while (full.triangles() + faces.size() <= solids::kMostTriangles) full.add(id++, many.data(), many.size() / 3,
                                                                              faces.data(), faces.size());
    const size_t before = full.triangles();
    full.add(id, many.data(), many.size() / 3, faces.data(), faces.size());
    CHECK_EQ(full.triangles(), before);
    CHECK(!full.has(id));
    full.clear();
    CHECK_EQ(full.triangles(), size_t(0));
    CHECK(!full.has(1));
}

// The free camera: a step into a wall stops kFreeGap before it, along the wall it slides, through a gate it flies
TEST(solids_the_free_camera_keeps_off_walls_and_flies_through_a_gate) {
    const solids::Set s = oneWall(30.0f);
    const float from[3] = {0, -20, 0};
    float to[3] = {28, -20, 5};  // towards the wall and a little along it
    solids::keepOffSolids(from, to, s);
    CHECK(std::fabs(to[0] - (30.0f - view3d::kFreeGap)) < 0.01f);  // stopped the gap before the wall
    CHECK_EQ(to[2], 5.0f);                                          // and slid along it
    float away[3] = {-10, -20, 0};
    solids::keepOffSolids(from, away, s);
    CHECK_EQ(away[0], -10.0f);
    // a gate ahead: through its opening
    std::vector<float> v;
    std::vector<solids::Face> f;
    wall(v, f, 30.0f, -40.0f, -12.0f, -60.0f, 0.0f);
    wall(v, f, 30.0f, 12.0f, 40.0f, -60.0f, 0.0f);
    solids::Set gate;
    gate.add(4, v.data(), v.size() / 3, f.data(), f.size());
    float through[3] = {26, -20, 0};
    solids::keepOffSolids(from, through, gate);
    CHECK_EQ(through[0], 26.0f);
    const float inside[3] = {26, -20, 0};
    float past[3] = {40, -20, 0};
    solids::keepOffSolids(inside, past, gate);
    CHECK_EQ(past[0], 40.0f);
}

// A lamp post, a tree's trunk, a sign: thin things between the character and the eye. In a town they come at every
// step - the camera goes past them, as it did while their small boxes were left out
TEST(solids_thin_things_do_not_stop_the_camera) {
    std::vector<float> v;
    std::vector<solids::Face> f;
    wall(v, f, 10.0f, -1.5f, 1.5f, -40.0f, 0.0f);  // a post three units wide, forty tall
    solids::Set s;
    s.add(1, v.data(), v.size() / 3, f.data(), f.size());
    CHECK(s.has(1));  // known: its box does not stand in for it either
    CHECK_EQ(s.triangles(), size_t(0));
    CHECK_EQ(s.thin(), size_t(1));
    const float head[3] = {0, -14, 0}, eye[3] = {40, -14, 0};
    CHECK(s.meets(head, eye) < 0.0f);
    // a wall's piece as long as the bound is a wall
    std::vector<float> wv;
    std::vector<solids::Face> wf;
    wall(wv, wf, 10.0f, -solids::kThinUnder / 2, solids::kThinUnder / 2, -40.0f, 0.0f);
    s.add(2, wv.data(), wv.size() / 3, wf.data(), wf.size());
    CHECK_EQ(s.triangles(), size_t(2));
    CHECK(s.meets(head, eye) > 0.0f);
    // long one way on the ground is enough (a fence's rail); tall alone is not
    std::vector<float> rv;
    std::vector<solids::Face> rf;
    wall(rv, rf, 20.0f, -30.0f, 30.0f, -3.0f, 0.0f);  // a low rail sixty long
    s.add(3, rv.data(), rv.size() / 3, rf.data(), rf.size());
    CHECK_EQ(s.triangles(), size_t(4));
}
