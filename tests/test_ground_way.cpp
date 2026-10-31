// 3.0.11-beta.31: the way followed drawn on the ground (core/ground_way) - the .gat's heights, the way's points along
// the ground a cell apart at the most, the strip placed on the screen as the game places its own vertices
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "core/ground_way.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A .gat of w x h cells, each its corners' heights from `height(x, y)` at the corner (x, y in cells)
template <typename F>
std::string gat(int w, int h, F height) {
    std::string out = "GRAT\x01\x02";
    out.append(reinterpret_cast<const char*>(&w), 4);
    out.append(reinterpret_cast<const char*>(&h), 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float c[4] = {height(x, y), height(x + 1, y), height(x, y + 1), height(x + 1, y + 1)};
            out.append(reinterpret_cast<const char*>(c), 16);
            const uint32_t type = 0;
            out.append(reinterpret_cast<const char*>(&type), 4);
        }
    return out;
}

}  // namespace

TEST(ground_way_gat_heights) {
    groundway::Heights g;
    CHECK(!groundway::parseGat("GRAT", g));
    CHECK(!groundway::parseGat(gat(2, 2, [](int, int) { return 0.0f; }).substr(0, 30), g));  // cut short
    CHECK(groundway::parseGat(gat(4, 3, [](int x, int y) { return float(x * 10 + y); }), g));
    CHECK(g.w == 4 && g.h == 3);
    float h = 0;
    CHECK(g.at(0, 0, h) && h == 0.0f);                    // the south-west corner
    CHECK(g.at(2.5f, 1.5f, h) && std::fabs(h - 26.5f) < 1e-4f);  // between a cell's corners
    CHECK(!g.at(4.0f, 0, h) && !g.at(-0.1f, 0, h));      // off the map
    float wx = 0, wz = 0;
    groundway::cellToWorld(156, 196, 312, 392, 5, wx, wz);  // the middle cell of prontera: half a cell from 0
    CHECK(wx == 2.5f && wz == 2.5f);
}

TEST(ground_way_points_along_the_ground) {
    groundway::Heights g;
    CHECK(groundway::parseGat(gat(10, 10, [](int x, int) { return -float(x); }), g));  // rising east (up: -y)
    const std::vector<groundway::Point> pts = groundway::along({{0, 5}, {3, 5}}, g, 5, 1.0f, 1.0f);
    CHECK_EQ(pts.size(), size_t(4));  // a point a cell
    CHECK(std::fabs(pts[0].x - (0.5f - 5) * 5) < 1e-4f && std::fabs(pts[0].z - (5.5f - 5) * 5) < 1e-4f);
    CHECK(std::fabs(pts[0].y - (-0.5f - 1.0f)) < 1e-4f);  // the ground at x 0.5 is -0.5, a unit over it: -1.5
    CHECK(std::fabs(pts[3].y - (-3.5f - 1.0f)) < 1e-4f);
    const std::vector<groundway::Point> flipped = groundway::along({{0, 5}, {3, 5}}, g, 5, 1.0f, -1.0f);
    CHECK(std::fabs(flipped[3].y - (3.5f - 1.0f)) < 1e-4f);
    CHECK(groundway::along({{20, 20}}, g, 5, 1.0f, 1.0f).empty());  // off the map
}

TEST(ground_way_strip_on_the_screen) {
    // a camera at the world's origin looking along +z (view = world): x right, y down, as the game's view space
    const float view[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    camera::Projection p;
    p.f0 = p.f1 = 100;
    p.cx = 320;
    p.cy = 240;
    p.nearZ = 10;
    p.farZ = 1500;
    const std::vector<groundway::Point> pts{{0, 10, 100}, {0, 10, 200}, {50, 10, 200}};  // ahead, then right
    std::vector<groundway::Vertex> v;
    CHECK_EQ(groundway::strip(pts, view, p, 2.0f, 0xE6F2C14Eu, 0.0f, v), size_t(12));  // two legs, 6 corners each
    CHECK(v[0].diffuse == 0xE6F2C14Eu && v[0].rhw > 0);
    // the first leg's near end: 1 unit each side of x 0 at z 100 - 1 pixel each side of the middle
    CHECK(std::fabs(std::min(v[0].x, v[1].x) - 319) < 0.01f && std::fabs(std::max(v[0].x, v[1].x) - 321) < 0.01f);
    CHECK(std::fabs(v[0].y - 250) < 0.01f);  // y 10 at z 100: 10 pixels under the middle
    // the bend joins the legs: the second leg starts where the first ends
    CHECK(std::fabs(v[2].x - v[6].x) < 1e-3f && std::fabs(v[2].y - v[6].y) < 1e-3f);
    // the depth as of 5 units nearer along the ray, the screen place kept
    std::vector<groundway::Vertex> biased;
    groundway::strip(pts, view, p, 2.0f, 0xFFFFFFFFu, 5.0f, biased);
    CHECK(biased[0].z < v[0].z && biased[0].x == v[0].x && biased[0].y == v[0].y && biased[0].rhw == v[0].rhw);
    const float side = biased[0].x > 320 ? 1.0f : -1.0f;  // the corner's world x (at y 10, z 100)
    const float len = std::sqrt(1.0f + 100.0f + 10000.0f), k = 1.0f - 5.0f / len;
    const float c[3] = {side * k, 10.0f * k, 100.0f * k};
    own_ground::Tl2 nearer{};
    CHECK(own_ground::placeView(p, c, nearer) && std::fabs(nearer.z - biased[0].z) < 1e-6f);
    // a point behind the near plane: its legs left out
    std::vector<groundway::Vertex> behind;
    CHECK_EQ(groundway::strip({{0, 10, 5}, {0, 10, 100}, {0, 10, 200}}, view, p, 2.0f, 0xFFFFFFFFu, 0, behind),
             size_t(6));
}

// 3.0.11-beta.33 (the owner: «полоска с маршрутом может лучше бы была выполнена такими типа следами типа шажков»): the
// way as footprints - left and right in turn a stride apart, each to its side, its toes along the way.
// 3.0.11-beta.34 (the owner: «каждый раз перестраивается линия, можно так чтобы она не очень дергалась»): counted back
// from the way's end - a print stays where it is on the ground while the player walks the way
TEST(ground_way_prints_along_the_way) {
    const std::vector<groundway::Point> line{{0, 0, 0}, {20, 0, 0}, {20, 0, 20}};  // east 20, then north 20
    const std::vector<groundway::Print> p = groundway::prints(line, 2.5f, 0.5f);
    CHECK_EQ(p.size(), size_t(17));  // at 0, 2.5 ... 40: the way's 40 from its end back
    CHECK(std::fabs(p[0].at) < 1e-4f && std::fabs(p[1].at - 2.5f) < 1e-4f && std::fabs(p.back().at - 40) < 1e-4f);
    CHECK(p[0].left != p[1].left && p[1].left != p[2].left);  // in turn
    // the first: at the way's start, its toes east, half a unit to its side
    CHECK(std::fabs(p[0].fx - 1) < 1e-3f && std::fabs(p[0].fz) < 1e-3f);
    CHECK(std::fabs(p[0].x) < 1e-3f && std::fabs(std::fabs(p[0].z) - 0.5f) < 1e-3f);
    CHECK((p[0].z > 0) == p[0].left);  // the left of a way east is north (x east, z north)
    // on the second leg the toes go north
    const groundway::Print& late = p.back();
    CHECK(late.at > 30 && std::fabs(late.fz - 1) < 1e-3f && std::fabs(std::fabs(late.x - 20) - 0.5f) < 1e-3f);
    CHECK(groundway::prints({{0, 0, 0}}, 2.5f, 0.5f).empty());
    // the way's tail (the player 7.3 along it): the very same prints from there on - each where it was, the same foot
    const std::vector<groundway::Print> tail = groundway::prints(groundway::cut(line, 7.3f), 2.5f, 0.5f);
    CHECK_EQ(tail.size(), size_t(14));  // 7.5, 10 ... 40 of the whole way
    for (const groundway::Print& t : tail) {
        const groundway::Print& q = p[size_t(std::lround((t.at + 7.3f) / 2.5f))];
        CHECK(std::fabs(t.x - q.x) < 1e-3f && std::fabs(t.z - q.z) < 1e-3f && t.left == q.left);
        CHECK(std::fabs(t.fx - q.fx) < 1e-3f && std::fabs(t.fz - q.fz) < 1e-3f);
    }
}

TEST(ground_way_cut_at_the_player) {
    const std::vector<groundway::Point> line{{0, -1, 0}, {20, -3, 0}, {20, -3, 20}};
    const std::vector<groundway::Point> c = groundway::cut(line, 7.5f);
    CHECK_EQ(c.size(), size_t(3));
    CHECK(std::fabs(c[0].x - 7.5f) < 1e-4f && std::fabs(c[0].z) < 1e-4f && std::fabs(c[0].y + 1.75f) < 1e-4f);
    CHECK(c[1].x == 20 && c[2].z == 20);
    CHECK_EQ(groundway::cut(line, 0).size(), size_t(3));
    CHECK_EQ(groundway::cut(line, 25).size(), size_t(2));  // on the second leg
    CHECK_EQ(groundway::cut(line, 99).size(), size_t(1));  // past its end: the end alone
    CHECK(groundway::cut({}, 3).empty());
}

TEST(ground_way_prints_ahead_of_the_player) {
    const std::vector<groundway::Point> line{{0, 0, 0}, {100, 0, 0}};
    const std::vector<groundway::Print> all = groundway::prints(line, 2.5f, 0.5f);  // at 0, 2.5 ... 100
    // the player 10 along: none nearer than 4.5 ahead, none past 30 ahead; they fade in over 2 at both ends
    const std::vector<groundway::Print> a = groundway::ahead(all, 10, 4.5f, 30, 2);
    CHECK(!a.empty() && a.front().at >= 14.5f - 1e-4f && a.back().at <= 40 + 1e-4f);
    CHECK_EQ(a.size(), size_t(10));  // 15, 17.5 ... 37.5
    CHECK(std::fabs(a.front().alpha - 0.25f) < 1e-4f);  // 15: 5 ahead, half a unit into the fade
    CHECK(std::fabs(a[1].alpha - 1) < 1e-4f && std::fabs(a.back().alpha - 1) < 1e-4f);
    CHECK(std::fabs(groundway::ahead(all, 10, 4.5f, 29, 2).back().alpha - 0.75f) < 1e-4f);  // 37.5 of 39: 1.5 left
    CHECK(groundway::ahead(all, 99, 4.5f, 30, 2).empty());
}

TEST(ground_way_print_shape) {
    groundway::Print east;
    east.x = 10;
    east.z = 20;
    const std::vector<groundway::Point> s = groundway::printShape(east, 0.0f);
    CHECK_EQ(s.size(), size_t(groundway::kSoleSides + groundway::kHeelSides));
    float soleX = 0, heelX = 0, most = 0;
    for (int i = 0; i < groundway::kSoleSides; ++i) soleX += s[size_t(i)].x / groundway::kSoleSides;
    for (int i = groundway::kSoleSides; i < int(s.size()); ++i) heelX += s[size_t(i)].x / groundway::kHeelSides;
    for (const groundway::Point& q : s) most = std::max(most, std::fabs(q.z - 20));
    CHECK(soleX > 10 && heelX < 10);  // the sole ahead (east), the heel behind
    CHECK(most < 0.5f);               // narrower than a cell's tenth each side (a cell is 5)
    const std::vector<groundway::Point> grown = groundway::printShape(east, 0.2f);
    float wider = 0;
    for (const groundway::Point& q : grown) wider = std::max(wider, std::fabs(q.z - 20));
    CHECK(std::fabs(wider - most - 0.2f) < 0.02f);  // the edge 0.2 all round
}

TEST(ground_way_footprints_on_the_screen) {
    groundway::Heights g;
    CHECK(groundway::parseGat(gat(20, 20, [](int, int) { return -2.0f; }), g));  // flat, 2 over 0 (up: -y)
    const float view[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};  // as the strip's test: x right, y down, z ahead
    camera::Projection p;
    p.f0 = p.f1 = 100;
    p.cx = 320;
    p.cy = 240;
    p.nearZ = 10;
    p.farZ = 1500;
    groundway::Print ahead;  // a print 30 ahead (cell 5: the map's middle cell 10 at z 0 - this one 6 cells north)
    ahead.z = 30;
    ahead.fz = 1;
    ahead.fx = 0;
    std::vector<groundway::Vertex> v;
    const size_t n = groundway::footprints({ahead}, g, 5, 1.0f, 0.1f, 0.0f, view, p, 0xE6F2C14Eu, 0.0f, v);
    CHECK_EQ(n, size_t((groundway::kSoleSides + groundway::kHeelSides) * 3));  // each oval a fan round its middle
    // on the ground: y -2.1 at z ~30 - under the middle of the screen by 100 * 2.1 / 30
    float y = 0;
    for (const groundway::Vertex& q : v) y += q.y / float(v.size());
    CHECK(std::fabs(y - (240 - 100 * 2.1f / 30)) < 0.6f && v[0].diffuse == 0xE6F2C14Eu);
    groundway::Print off = ahead;  // off the map: nothing
    off.z = 500;
    CHECK_EQ(groundway::footprints({off}, g, 5, 1.0f, 0.1f, 0.0f, view, p, 0xE6F2C14Eu, 0.0f, v), size_t(0));
}

TEST(ground_way_footprints_fade) {
    groundway::Heights g;
    CHECK(groundway::parseGat(gat(20, 20, [](int, int) { return -2.0f; }), g));
    const float view[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    camera::Projection p;
    p.f0 = p.f1 = 100;
    p.cx = 320;
    p.cy = 240;
    p.nearZ = 10;
    p.farZ = 1500;
    groundway::Print half;
    half.z = 30;
    half.fz = 1;
    half.fx = 0;
    half.alpha = 0.5f;
    std::vector<groundway::Vertex> v;
    CHECK(groundway::footprints({half}, g, 5, 1.0f, 0.1f, 0.0f, view, p, 0xE65AC8FAu, 0.0f, v) > 0);
    CHECK_EQ(v[0].diffuse, 0x735AC8FAu);  // 0xE6 halved
    groundway::Print gone = half;  // faded out: nothing
    gone.alpha = 0;
    CHECK_EQ(groundway::footprints({gone}, g, 5, 1.0f, 0.1f, 0.0f, view, p, 0xE65AC8FAu, 0.0f, v), size_t(0));
    CHECK_EQ(groundway::faded(0xE65AC8FAu, 0.25f), 0x3A5AC8FAu);
    CHECK_EQ(groundway::faded(0xE65AC8FAu, 2.0f), 0xE65AC8FAu);
}

// 3.0.11-beta.87 (the concept «Контроллер в интерфейсе uaRO», stage 3): marks on the ground under an actor - a ring
// round its feet (whole, or dashed for the next target), four notches out of it (a friend), and a chevron over its head
// in the screen's pixels
TEST(ground_way_rings_and_chevron) {
    const float view[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};  // as the strip's test: x right, y down, z ahead
    camera::Projection p;
    p.f0 = p.f1 = 100;
    p.cx = 320;
    p.cy = 240;
    p.nearZ = 10;
    p.farZ = 1500;
    std::vector<groundway::Vertex> v;
    CHECK_EQ(groundway::ring(0, 10, 100, 10, 1.0f, 24, false, view, p, 0xF0E6CB8Au, 0.0f, v), size_t(24 * 6));
    float left = 1e9f, right = -1e9f;
    for (const groundway::Vertex& q : v) {
        left = std::min(left, q.x);
        right = std::max(right, q.x);
    }
    CHECK(std::fabs(left - (320 - 10.5f * 100 / 100)) < 1.2f && std::fabs(right - (320 + 10.5f * 100 / 100)) < 1.2f);
    CHECK(v[0].diffuse == 0xF0E6CB8Au);
    std::vector<groundway::Vertex> dashed;
    CHECK_EQ(groundway::ring(0, 10, 100, 10, 1.0f, 24, true, view, p, 0xFFFFFFFFu, 0.0f, dashed), size_t(12 * 6));
    std::vector<groundway::Vertex> notches;
    CHECK_EQ(groundway::notches(0, 10, 100, 10, 1.0f, view, p, 0xFF5BD36Cu, 0.0f, notches), size_t(4 * 6));
    std::vector<groundway::Vertex> c;
    CHECK_EQ(groundway::chevron(400, 120, 20, 14, 0xFFE6CB8Au, c), size_t(3));
    CHECK(c[2].x == 400 && c[2].y == 120 && c[0].y == 106 && c[0].x == 390 && c[1].x == 410);
    CHECK(c[0].z == 0 && c[0].rhw == 1 && c[0].diffuse == 0xFFE6CB8Au);
}

// 3.0.11-beta.88 (the concept's stage 4): the walk without the mouse's arrow on the ground - a shaft from near the
// character's feet and a head at the cell it walks to
TEST(ground_way_arrow) {
    const float view[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    camera::Projection p;
    p.f0 = p.f1 = 100;
    p.cx = 320;
    p.cy = 240;
    p.nearZ = 10;
    p.farZ = 1500;
    std::vector<groundway::Vertex> v;
    // from (0, 100) to (0, 130) on the ground at y 10, 1 wide, its head 4 long and 3 wide: a quad and a triangle
    CHECK_EQ(groundway::arrow(0, 100, 0, 130, 10, 1.0f, 4.0f, 3.0f, view, p, 0xC8FFFFFFu, 0.0f, v), size_t(9));
    float mostZ = 0;  // the head's tip the farthest: its rhw the smallest
    for (const groundway::Vertex& q : v) mostZ = std::max(mostZ, 1.0f / q.rhw);
    CHECK(std::fabs(mostZ - 130) < 0.5f);
    std::vector<groundway::Vertex> none;
    CHECK_EQ(groundway::arrow(0, 100, 0, 101, 10, 1.0f, 4.0f, 3.0f, view, p, 0xC8FFFFFFu, 0.0f, none), size_t(3));
}

// 3.0.11-beta.102 (the owner: «чтобы стрелочка показывалась как в концепции»): the stick's arrow as the concept draws
// it - over the ground's heights from near the character's feet to the near edge of the cell asked for, the head gold
// edged under its white, the cell's square outlined in gold
TEST(ground_way_walk_arrow_on_the_ground) {
    groundway::Heights g;
    CHECK(groundway::parseGat(gat(20, 20, [](int x, int) { return -float(x); }), g));  // rising east (up: -y)
    const int cs = 5;
    float x0 = 0, z0 = 0;  // the character at the cell (4, 10)'s middle, the cell (9, 10) asked for
    groundway::cellToWorld(4, 10, 20, 20, cs, x0, z0);
    const std::vector<groundway::Tri> t = groundway::walkArrowShape(x0, z0, 9, 10, g, cs, 1.0f, 0.5f);
    CHECK(!t.empty());
    bool onGround = true;
    float tipX = -1e9f, shaftFrom = 1e9f, cellFrom = 1e9f, cellTo = -1e9f;
    int firstEdge = -1, firstHead = -1;
    for (size_t i = 0; i < t.size(); ++i) {
        for (const groundway::Point& c : {t[i].a, t[i].b, t[i].c}) {
            float h = 0;
            onGround = onGround && g.at(c.x / float(cs) + 10, c.z / float(cs) + 10, h) && std::fabs(c.y - (h - 0.5f)) <
                                                                                             1e-3f;
            if (t[i].argb == groundway::kWalkHead) tipX = std::max(tipX, c.x);
            if (t[i].argb == groundway::kWalkShaft) shaftFrom = std::min(shaftFrom, c.x);
            if (t[i].argb == groundway::kWalkCellLine) {
                cellFrom = std::min(cellFrom, c.x);
                cellTo = std::max(cellTo, c.x);
            }
        }
        if (t[i].argb == groundway::kWalkEdge && firstEdge < 0) firstEdge = int(i);
        if (t[i].argb == groundway::kWalkHead && firstHead < 0) firstHead = int(i);
    }
    CHECK(onGround);  // every corner on the ground (a unit over it)
    const float cellMiddle = (9.5f - 10) * float(cs);
    CHECK(std::fabs(tipX - (cellMiddle - 0.55f * float(cs))) < 0.01f);  // the tip at the cell's near edge
    CHECK(std::fabs(shaftFrom - (x0 + 0.45f * float(cs))) < 0.01f);      // the shaft from near the feet
    CHECK(cellFrom < cellMiddle - 2.4f && cellTo > cellMiddle + 2.4f);   // the cell's square round its middle
    CHECK(firstEdge >= 0 && firstHead > firstEdge);                      // the gold edge under the white head
    CHECK(!groundway::walkArrowShape(x0, z0, 5, 10, g, cs, 1.0f, 0.5f).empty());  // the next cell: a head and a cell
    CHECK(groundway::walkArrowShape(x0, z0, 4, 10, g, cs, 1.0f, 0.5f).empty());   // where it stands: nothing
    const float view[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 100};  // the ground 100 ahead
    camera::Projection p;
    p.f0 = p.f1 = 100;
    p.cx = 320;
    p.cy = 240;
    p.nearZ = 10;
    p.farZ = 1500;
    std::vector<groundway::Vertex> v;
    CHECK_EQ(groundway::placeTris(t, view, p, 0.0f, v), t.size() * 3);  // three corners a triangle on the screen
    CHECK(v[0].diffuse == t[0].argb);
}

// 3.0.11-beta.103 (the owner: «можешь сделать настройку, чтобы можно было выбрать эту стрелку в конфигураторе»):
// every look of the stick's arrow ([interface] walk_arrow) on the ground's heights - each part where the doc draws it,
// a dark shade under the light ones first, the chevrons running on with the phase, a wall's bar, faded by its alpha
TEST(ground_way_walk_looks) {
    groundway::Heights g;
    CHECK(groundway::parseGat(gat(20, 20, [](int x, int) { return -float(x); }), g));
    const int cs = 5;
    float x0 = 0, z0 = 0;
    groundway::cellToWorld(4, 10, 20, 20, cs, x0, z0);
    groundway::WalkArrow a;
    a.x0 = x0;
    a.z0 = z0;
    a.wayX = 1;
    a.wayZ = 0;
    a.len = 5.0f * cs;  // 3.0.11-beta.104: its end along the way (it was the cell (9, 10))
    const auto on = [&](const std::vector<groundway::Tri>& t) {
        bool all = !t.empty();
        for (const groundway::Tri& q : t)
            for (const groundway::Point& c : {q.a, q.b, q.c}) {
                float h = 0;
                all = all && g.at(c.x / float(cs) + 10, c.z / float(cs) + 10, h) && std::fabs(c.y - (h - 0.5f)) < 1e-3f;
            }
        return all;
    };
    const auto parts = [](const std::vector<groundway::Tri>& t, uint8_t part) {
        size_t n = 0;
        for (const groundway::Tri& q : t) n += q.part == part;
        return n;
    };
    for (const WalkLook look : {WalkLook::Chevrons, WalkLook::Slim, WalkLook::Concept, WalkLook::Ring, WalkLook::Dots,
                                WalkLook::Cell})
        CHECK(on(groundway::walkShape(look, a, g, cs, 1.0f, 0.5f)));
    CHECK(groundway::walkShape(WalkLook::Off, a, g, cs, 1.0f, 0.5f).empty());
    // the chevrons: three, a shade under them first, a gold diamond in the cell; they run on with the phase
    std::vector<groundway::Tri> c = groundway::walkShape(WalkLook::Chevrons, a, g, cs, 1.0f, 0.5f);
    CHECK(parts(c, groundway::kPartChevron) > 0 && parts(c, groundway::kPartMarker) > 0);
    CHECK(!c.empty() && c[0].part == groundway::kPartShade);
    const auto nearest = [&](const std::vector<groundway::Tri>& t) {
        float x = 1e9f;
        for (const groundway::Tri& q : t)
            if (q.part == groundway::kPartChevron) x = std::min({x, q.a.x, q.b.x, q.c.x});
        return x;
    };
    a.phase = 0.4f;
    CHECK(nearest(groundway::walkShape(WalkLook::Chevrons, a, g, cs, 1.0f, 0.5f)) > nearest(c));
    a.phase = 0;
    // a short way (a wall two cells on): one chevron, still, before the diamond; next to the feet the diamond alone
    a.len = 2.0f * cs;
    CHECK(parts(groundway::walkShape(WalkLook::Chevrons, a, g, cs, 1.0f, 0.5f), groundway::kPartChevron) == 4);
    a.len = 1.0f * cs;
    CHECK(parts(groundway::walkShape(WalkLook::Chevrons, a, g, cs, 1.0f, 0.5f), groundway::kPartChevron) == 0);
    a.len = 5.0f * cs;
    // the ring: round the feet, a cell and a twentieth out; its gold pointer further on along the way
    const std::vector<groundway::Tri> r = groundway::walkShape(WalkLook::Ring, a, g, cs, 1.0f, 0.5f);
    bool round = parts(r, groundway::kPartRing) > 0, ahead = parts(r, groundway::kPartPointer) > 0;
    for (const groundway::Tri& q : r)
        for (const groundway::Point& p : {q.a, q.b, q.c}) {
            const float d = std::hypot(p.x - x0, p.z - z0);
            if (q.part == groundway::kPartRing) round = round && d > 0.95f * cs && d < 1.15f * cs;
            if (q.part == groundway::kPartPointer) ahead = ahead && p.x > x0 + 0.9f * cs;
        }
    CHECK(round && ahead);
    // the dots: one a half cell from 0.6 of a cell up to the cell's near edge
    const std::vector<groundway::Tri> d = groundway::walkShape(WalkLook::Dots, a, g, cs, 1.0f, 0.5f);
    CHECK(parts(d, groundway::kPartDot) >= 6 * 8 && parts(d, groundway::kPartMarker) > 0);
    // the cell alone: its frame and a wedge at the feet, no line between them
    const std::vector<groundway::Tri> e = groundway::walkShape(WalkLook::Cell, a, g, cs, 1.0f, 0.5f);
    CHECK(parts(e, groundway::kPartCellLine) > 0 && parts(e, groundway::kPartPointer) > 0);
    CHECK(parts(e, groundway::kPartShaft) == 0 && parts(e, groundway::kPartChevron) == 0);
    // a wall ahead: a short reddish bar across the way at its end
    a.blocked = true;
    CHECK(parts(groundway::walkShape(WalkLook::Chevrons, a, g, cs, 1.0f, 0.5f), groundway::kPartWall) > 0);
    a.blocked = false;
    a.alpha = 0.5f;  // fading: every part at half its alpha at the most
    bool faded = true;
    for (const groundway::Tri& q : groundway::walkShape(WalkLook::Slim, a, g, cs, 1.0f, 0.5f))
        faded = faded && (q.argb >> 24) <= 0x80u;
    CHECK(faded);
}

// 3.0.11-beta.104 (the owner: «стрелочки очень дергаются при движении»): the arrow glides with the character - along
// the way the stick walks, as long as the lead; the asked cell (the game's cell + the lead, hopping a cell on each time
// the character's cell changes) left out while the way is open; a wall: up to that cell, which stays put; eased
TEST(ground_way_walk_glide_holds_still_against_the_feet) {
    const float cs = 5;
    groundway::WalkGlide gl;
    groundway::WalkAim aim;
    aim.wayX = 1;
    aim.wayZ = 0;
    aim.lead = 5 * cs;
    float worst = 0, before = 0;
    for (int f = 0; f < 120; ++f) {  // east at 150 ms a cell, 16 ms frames
        aim.x0 = float(f) * 16.0f / 150.0f * cs;
        aim.cellX = (std::round(aim.x0 / cs) + 5) * cs;  // the asked cell hops on
        groundway::glide(gl, aim, 16);
        const float end = std::cos(gl.angle) * gl.len;  // against the feet
        if (f > 0) worst = std::max(worst, std::fabs(end - before));
        before = end;
    }
    CHECK(worst < 0.01f * cs);
    CHECK(std::fabs(gl.len - 5 * cs) < 0.01f && std::fabs(gl.angle) < 1e-4f);
    // a wall four cells on: eased down to the asked cell, then its end stays on it as the feet come up
    aim.cut = true;
    aim.cellX = aim.x0 + 4 * cs;
    const float wallCell = aim.cellX;
    float firstStep = 0;
    for (int f = 0; f < 25; ++f) {  // 2.7 cells on, the wall still ahead
        aim.x0 += 16.0f / 150.0f * cs;
        const float was = gl.len;
        groundway::glide(gl, aim, 16);
        if (f == 0) firstStep = was - gl.len;
    }
    CHECK(firstStep > 0 && firstStep < 0.5f * cs);  // no jump at once
    CHECK(std::fabs(aim.x0 + gl.len - wallCell) < 0.05f * cs);
    // the stick turned a quarter: about two thirds round in 80 ms, all of it soon
    aim.cut = false;
    aim.wayX = 0;
    aim.wayZ = 1;
    for (int f = 0; f < 5; ++f) groundway::glide(gl, aim, 16);
    CHECK(gl.angle > 0.55f && gl.angle < 1.2f);
    for (int f = 0; f < 40; ++f) groundway::glide(gl, aim, 16);
    CHECK(std::fabs(gl.angle - 1.5707963f) < 0.01f);
    groundway::WalkGlide fresh;  // a new arrow: its way and length at once
    groundway::glide(fresh, aim, 0);
    CHECK(std::fabs(fresh.angle - 1.5707963f) < 1e-4f && std::fabs(fresh.len - 5 * cs) < 1e-4f);
    aim.cut = true;  // the feet put elsewhere at once (a warp): taken anew, no easing from the old place
    aim.x0 += 40 * cs;
    aim.cellX = aim.x0;
    aim.cellZ = aim.z0 + 3 * cs;
    groundway::glide(gl, aim, 16);
    CHECK(std::fabs(gl.len - 3 * cs) < 1e-3f);
}

// 3.0.11-beta.105 (the owner: «а можно клетку куда идти над стрелкой не рисовать?»): without the end's mark each look
// is its arrow alone - no diamond, no cell's corners, frame or fill, no small ring (nor their shade); «Лише клітинка»
// keeps its cell (the look itself)
TEST(ground_way_walk_looks_without_the_mark) {
    groundway::Heights g;
    CHECK(groundway::parseGat(gat(20, 20, [](int, int) { return 0.0f; }), g));
    const int cs = 5;
    groundway::WalkArrow a;
    groundway::cellToWorld(4, 10, 20, 20, cs, a.x0, a.z0);
    a.len = 5.0f * cs;
    const auto marks = [](const std::vector<groundway::Tri>& t) {
        size_t n = 0;
        for (const groundway::Tri& q : t)
            n += q.part == groundway::kPartMarker || q.part == groundway::kPartCellLine ||
                 q.part == groundway::kPartCellFill;
        return n;
    };
    for (const WalkLook look :
         {WalkLook::Chevrons, WalkLook::Slim, WalkLook::Concept, WalkLook::Ring, WalkLook::Dots}) {
        a.mark = true;
        const std::vector<groundway::Tri> with = groundway::walkShape(look, a, g, cs, 1.0f, 0.5f);
        a.mark = false;
        const std::vector<groundway::Tri> without = groundway::walkShape(look, a, g, cs, 1.0f, 0.5f);
        CHECK(marks(with) > 0 && marks(without) == 0);
        CHECK(!without.empty() && without.size() < with.size());  // the arrow stays, its mark's shade gone too
    }
    a.mark = false;
    CHECK(marks(groundway::walkShape(WalkLook::Cell, a, g, cs, 1.0f, 0.5f)) > 0);
}

