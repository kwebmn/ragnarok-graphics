#include <cmath>
#include <string>
#include <vector>

#include "core/uaui_mesh.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }
}  // namespace

// 3.0.18-beta.7 (the RmlUi spike, «Концепция: единая библиотека интерфейса»): RmlUi's triangles as the game's
TEST(uaui_mesh_moves_scales_and_expands_the_triangles) {
    // a 10 x 10 quad at (0, 0), moved by (100, 50), at k 2 with the half texel 0.5: game x = (x - 0.5) / 2
    const std::vector<uaui::Point> pts = {{0, 0, 0xFF102030u, 0, 0}, {10, 0, 0xFF102030u, 1, 0},
                                          {0, 10, 0xFF102030u, 0, 1}, {10, 10, 0xFF102030u, 1, 1}};
    const std::vector<int> ix = {0, 1, 2, 1, 3, 2};
    std::vector<glint::Vertex> out;
    uaui::triangles(pts, ix, 100, 50, nullptr, nullptr, 2.0f, 0.5f, out);
    CHECK_EQ(out.size(), size_t(6));
    CHECK(near(out[0].x, 49.75f) && near(out[0].y, 24.75f));
    CHECK(near(out[4].x, 54.75f) && near(out[4].y, 29.75f));
    CHECK(out[0].diffuse == 0xFF102030u && near(out[0].rhw, 1.0f) && near(out[4].u, 1.0f) && near(out[4].v, 1.0f));
}

TEST(uaui_mesh_clips_to_the_box_with_its_uv) {
    // a 10 x 10 quad clipped to x 0..5: half of it, u 0..0.5
    const std::vector<uaui::Point> pts = {{0, 0, 0xFFFFFFFFu, 0, 0}, {10, 0, 0xFFFFFFFFu, 1, 0},
                                          {0, 10, 0xFFFFFFFFu, 0, 1}, {10, 10, 0xFFFFFFFFu, 1, 1}};
    const std::vector<int> ix = {0, 1, 2, 1, 3, 2};
    const uaui::ClipBox clip{0, 0, 5, 100};
    std::vector<glint::Vertex> out;
    uaui::triangles(pts, ix, 0, 0, nullptr, &clip, 1.0f, 0.0f, out);
    CHECK(!out.empty() && out.size() % 3 == 0);
    float maxX = 0, maxU = 0, area = 0;
    for (size_t i = 0; i < out.size(); i += 3) {
        for (size_t j = i; j < i + 3; ++j) maxX = std::max(maxX, out[j].x), maxU = std::max(maxU, out[j].u);
        area += std::fabs((out[i + 1].x - out[i].x) * (out[i + 2].y - out[i].y) -
                          (out[i + 2].x - out[i].x) * (out[i + 1].y - out[i].y)) / 2;
    }
    CHECK(near(maxX, 5.0f) && near(maxU, 0.5f) && near(area, 50.0f));
    // wholly outside: nothing
    const uaui::ClipBox away{20, 20, 30, 30};
    std::vector<glint::Vertex> none;
    uaui::triangles(pts, ix, 0, 0, nullptr, &away, 1.0f, 0.0f, none);
    CHECK(none.empty());
}

TEST(uaui_mesh_applies_the_transform_and_its_colour_halfway) {
    // RmlUi's column-major matrix: a scale of 2 about the origin
    const float m[16] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const std::vector<uaui::Point> pts = {{1, 1, 0xFF000000u, 0, 0}, {3, 1, 0xFFFFFFFFu, 1, 0},
                                          {1, 3, 0xFF000000u, 0, 1}};
    std::vector<glint::Vertex> out;
    uaui::triangles(pts, {0, 1, 2}, 0, 0, m, nullptr, 1.0f, 0.0f, out);
    CHECK_EQ(out.size(), size_t(3));
    CHECK(near(out[1].x, 6.0f) && near(out[1].y, 2.0f));
    // clipped at x 4 (halfway along the edge from 2 to 6): the colour there half way too
    const uaui::ClipBox clip{0, 0, 4, 100};
    std::vector<glint::Vertex> cut;
    uaui::triangles(pts, {0, 1, 2}, 0, 0, m, &clip, 1.0f, 0.0f, cut);
    bool half = false;
    for (const glint::Vertex& v : cut)
        if (near(v.x, 4.0f) && near(v.y, 2.0f)) half = ((v.diffuse >> 16) & 0xFF) >= 0x7E && ((v.diffuse >> 16) & 0xFF) <= 0x81;
    CHECK(half);
}

TEST(uaui_mesh_reads_utf8) {
    const std::u32string s = uaui::codepoints("A\xD0\x86\xE2\x80\x94\xF0\x9F\x99\x82\xFF");  // A І — 🙂 and a bad byte
    CHECK_EQ(s.size(), size_t(5));
    CHECK(s[0] == U'A' && s[1] == 0x406 && s[2] == 0x2014 && s[3] == 0x1F642 && s[4] == 0xFFFD);
}

// 3.0.18-beta.61 (docs/superpowers/plans/2026-11-10-uaui-possibilities.md, stage 5): a linear gradient as the
// vertices' colours - the box cut along each stop between its ends, so three stops are exact
TEST(uaui_mesh_shades_a_linear_gradient_with_its_stops) {
    std::vector<uaui::Point> pts = {{0, 0, 0xFFFFFFFF}, {100, 0, 0xFFFFFFFF}, {100, 10, 0xFFFFFFFF},
                                    {0, 10, 0xFFFFFFFF}};
    std::vector<int> ix = {0, 1, 2, 0, 2, 3};
    uaui::Gradient g;
    g.x0 = 0, g.y0 = 0, g.x1 = 100, g.y1 = 0;  // left to right
    g.stops = {{0, 0xFFFF0000}, {0.5f, 0xFF00FF00}, {1, 0xFF0000FF}};
    uaui::shade(pts, ix, g);
    CHECK(ix.size() % 3 == 0 && ix.size() > 6);  // cut at the middle stop
    bool middle = false;
    for (const uaui::Point& p : pts) {
        if (p.x == 0) CHECK_EQ(p.argb, 0xFFFF0000u);
        if (p.x == 100) CHECK_EQ(p.argb, 0xFF0000FFu);
        if (std::fabs(p.x - 50) < 0.01f) {
            middle = true;
            CHECK_EQ(p.argb, 0xFF00FF00u);
        }
    }
    CHECK(middle);
    // the vertex's own colour multiplies (the element's opacity)
    std::vector<uaui::Point> half = {{0, 0, 0x80808080}, {100, 0, 0x80808080}, {0, 10, 0x80808080}};
    std::vector<int> one = {0, 1, 2};
    g.stops = {{0, 0xFFFFFFFF}, {1, 0xFFFFFFFF}};
    uaui::shade(half, one, g);
    CHECK_EQ(half[0].argb, 0x80808080u);
}

// 3.0.18-beta.61: a rounded clip (overflow hidden + border-radius) - the mask's convex outline cuts the triangles
TEST(uaui_mesh_cuts_by_a_convex_mask) {
    std::vector<uaui::Point> mask = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {5, 5}};  // a square, a point inside
    uaui::ClipBox c{-100, -100, 100, 100};
    c.hull = uaui::hull(mask, 0, 0, nullptr);
    CHECK_EQ(c.hull.size(), size_t(8));  // its four corners only
    std::vector<uaui::Point> tri = {{-5, -5, 0xFFFFFFFF}, {15, -5, 0xFFFFFFFF}, {-5, 15, 0xFFFFFFFF}};
    std::vector<glint::Vertex> out;
    uaui::triangles(tri, {0, 1, 2}, 0, 0, nullptr, &c, 1, 0, out);
    float area = 0;
    for (size_t i = 0; i + 2 < out.size(); i += 3)
        area += std::fabs((out[i + 1].x - out[i].x) * (out[i + 2].y - out[i].y) -
                          (out[i + 2].x - out[i].x) * (out[i + 1].y - out[i].y)) / 2;
    CHECK(std::fabs(area - 50.0f) < 0.01f);  // the square under the triangle's long side (x + y = 10): its half
    for (const glint::Vertex& v : out) CHECK(v.x >= -0.01f && v.x <= 10.01f && v.y >= -0.01f && v.y <= 10.01f);
}

// 3.0.18-beta.61: a soft shadow or glow - a ring round a rounded box, its colour at the edge fading to nothing
TEST(uaui_mesh_makes_a_soft_ring_round_a_box) {
    std::vector<uaui::Point> pts;
    std::vector<int> ix;
    uaui::softRing(0, 0, 100, 40, 8, 12, 0x80000000, pts, ix);
    CHECK(!pts.empty() && ix.size() % 3 == 0);
    float minX = 1e9f, maxX = -1e9f;
    for (const uaui::Point& p : pts) {
        minX = std::min(minX, p.x), maxX = std::max(maxX, p.x);
        CHECK(p.argb == 0x80000000u || p.argb == 0u);
        if (p.argb == 0x80000000u) CHECK(p.x >= -0.01f && p.x <= 100.01f);  // the inner edge on the box's
    }
    CHECK(std::fabs(minX + 12) < 0.01f && std::fabs(maxX - 112) < 0.01f);
}

// 3.0.18-beta.63: a radial gradient - the geometry cut into small triangles (8 px at most a side) and each vertex
// coloured by its distance from the middle over the radius
TEST(uaui_mesh_shades_a_radial_gradient) {
    std::vector<uaui::Point> pts = {{0, 0, 0xFFFFFFFF}, {40, 0, 0xFFFFFFFF}, {40, 40, 0xFFFFFFFF},
                                    {0, 40, 0xFFFFFFFF}};
    std::vector<int> ix = {0, 1, 2, 0, 2, 3};
    uaui::Gradient g;
    g.radial = true;
    g.x0 = 20, g.y0 = 20, g.x1 = 20, g.y1 = 20;  // the middle, the radii
    g.stops = {{0, 0xFFFFFFFF}, {1, 0x00000000}};
    uaui::shade(pts, ix, g);
    bool middle = false;
    for (const uaui::Point& p : pts) {
        CHECK(std::fabs(p.x - 0) < 0.01f || std::fabs(p.x - 40) < 0.01f || p.x > 0);
        if (std::fabs(p.x - 20) < 0.01f && std::fabs(p.y - 20) < 0.01f) middle = true, void();
        if (p.x == 0 && p.y == 0) CHECK_EQ(p.argb, 0u);  // beyond the radius
    }
    for (size_t i = 0; i + 2 < ix.size(); i += 3) {
        const uaui::Point &a = pts[size_t(ix[i])], &b = pts[size_t(ix[i + 1])];
        CHECK(std::hypot(a.x - b.x, a.y - b.y) <= 8.01f);
    }
    CHECK(middle);
}

// 3.0.18-beta.64: a rounded box filled (the glass's shape) - its area the box's less the corners' cut
TEST(uaui_mesh_fills_a_rounded_box) {
    std::vector<uaui::Point> pts;
    std::vector<int> ix;
    uaui::roundedBox(0, 0, 40, 20, 6, 0xFFFFFFFF, pts, ix);
    float area = 0;
    for (size_t i = 0; i + 2 < ix.size(); i += 3) {
        const uaui::Point &a = pts[size_t(ix[i])], &b = pts[size_t(ix[i + 1])], &c = pts[size_t(ix[i + 2])];
        area += std::fabs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) / 2;
    }
    const float want = 40 * 20 - (4 - 3.14159265f) * 36;
    CHECK(std::fabs(area - want) < 0.01f * want);
}

// 3.0.18-beta.76 (R-5AJCUY: the low HP edge 105-135 thousand vertices a frame at 4K): a big radial gradient cut by its
// radius (a twelfth of it at most), not by 8 px
TEST(uaui_mesh_cuts_a_big_radial_gradient_by_its_radius) {
    std::vector<uaui::Point> pts = {{0, 0, 0xFFFFFFFF}, {3840, 0, 0xFFFFFFFF}, {3840, 2160, 0xFFFFFFFF},
                                    {0, 2160, 0xFFFFFFFF}};
    std::vector<int> ix = {0, 1, 2, 0, 2, 3};
    uaui::Gradient g;
    g.radial = true;
    g.x0 = 1920, g.y0 = 1080, g.x1 = 2203, g.y1 = 1239;  // farthest corner
    g.stops = {{0.62f, 0x00000000}, {1, 0x66FF2D23}};
    uaui::shade(pts, ix, g);
    CHECK(ix.size() / 3 <= 6000);  // a few thousand triangles at most, not 131 thousand (its clear middle whole)
    CHECK(ix.size() / 3 > 100);
}
