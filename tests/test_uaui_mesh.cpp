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
