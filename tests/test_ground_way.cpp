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
// way as footprints - left and right in turn a stride apart, each to its side, its toes along the way
TEST(ground_way_prints_along_the_way) {
    const std::vector<groundway::Point> line{{0, 0, 0}, {20, 0, 0}, {20, 0, 20}};  // east 20, then north 20
    const std::vector<groundway::Print> p = groundway::prints(line, 2.5f, 0.5f, 3.0f, 100.0f);
    CHECK_EQ(p.size(), size_t(15));  // at 3, 5.5 ... 38 of the way's 40
    CHECK(std::fabs(p[0].at - 3.0f) < 1e-4f && std::fabs(p[1].at - 5.5f) < 1e-4f);
    CHECK(p[0].left != p[1].left && p[1].left != p[2].left);  // in turn
    // the first: 3 along the first leg, its toes east, half a unit to its side
    CHECK(std::fabs(p[0].fx - 1) < 1e-3f && std::fabs(p[0].fz) < 1e-3f);
    CHECK(std::fabs(p[0].x - 3) < 1e-3f && std::fabs(std::fabs(p[0].z) - 0.5f) < 1e-3f);
    CHECK((p[0].z > 0) == p[0].left);  // the left of a way east is north (x east, z north)
    // on the second leg the toes go north
    const groundway::Print& late = p.back();
    CHECK(late.at > 30 && std::fabs(late.fz - 1) < 1e-3f && std::fabs(std::fabs(late.x - 20) - 0.5f) < 1e-3f);
    // none past `most`, none on a way shorter than `skip`
    CHECK_EQ(groundway::prints(line, 2.5f, 0.5f, 3.0f, 10.0f).size(), size_t(3));  // 3, 5.5, 8
    CHECK(groundway::prints({{0, 0, 0}, {2, 0, 0}}, 2.5f, 0.5f, 3.0f, 100.0f).empty());
    CHECK(groundway::prints({{0, 0, 0}}, 2.5f, 0.5f, 0.0f, 100.0f).empty());
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
