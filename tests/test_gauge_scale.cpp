#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#include "core/gauge_scale.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.20-beta.84 (R-7WTVBF «когда отдаляю, то полоска хп выглядит огромной»): the bars under the characters (the game's
// gauges, 60 x 9 pixels whatever the camera) smaller once the camera is further out than the game's own views

namespace {

std::vector<uint8_t> quad(float x0, float y0, float x1, float y1) {  // TL vertices, 32 bytes each
    std::vector<uint8_t> v(4 * 32, 0);
    const float xy[4][2] = {{x0, y0}, {x1, y0}, {x0, y1}, {x1, y1}};
    for (int i = 0; i < 4; ++i) std::memcpy(v.data() + size_t(i) * 32, xy[i], sizeof xy[i]);
    return v;
}

float xOf(const std::vector<uint8_t>& v, int i) {
    float x = 0;
    std::memcpy(&x, v.data() + size_t(i) * 32, 4);
    return x;
}

float yOf(const std::vector<uint8_t>& v, int i) {
    float y = 0;
    std::memcpy(&y, v.data() + size_t(i) * 32 + 4, 4);
    return y;
}

}  // namespace

TEST(gauge_scale_the_game_s_size_up_to_its_own_views_then_smaller) {
    CHECK_EQ(gaugescale::scaleAt(300.0f), 1.0f);
    CHECK_EQ(gaugescale::scaleAt(gaugescale::kFullAt), 1.0f);
    CHECK(std::fabs(gaugescale::scaleAt(700.0f) - gaugescale::kFullAt / 700.0f) < 1e-5f);
    CHECK_EQ(gaugescale::scaleAt(1400.0f), gaugescale::kLeast);  // no smaller than this
    CHECK_EQ(gaugescale::scaleAt(0.0f), 1.0f);                    // nonsense: as the game draws it
    CHECK_EQ(gaugescale::scaleAt(std::nanf("")), 1.0f);
}

TEST(gauge_scale_a_bar_shrinks_toward_its_top_middle) {
    std::vector<uint8_t> v = quad(100.0f, 200.0f, 160.0f, 209.0f);
    gaugescale::scaleVertices(v.data(), 4, 32, 130.0f, 200.0f, 0.5f);
    CHECK_EQ(xOf(v, 0), 115.0f);
    CHECK_EQ(yOf(v, 0), 200.0f);
    CHECK_EQ(xOf(v, 3), 145.0f);
    CHECK_EQ(yOf(v, 3), 204.5f);
}

TEST(gauge_scale_a_face_goes_with_the_gauge_whose_box_holds_it) {
    const std::vector<std::array<int, 4>> boxes = {{100, 200, 60, 9}, {300, 250, 60, 5}};
    const std::vector<uint8_t> fill = quad(301.0f, 251.0f, 330.0f, 254.0f);  // the second one's fill
    CHECK_EQ(gaugescale::boxOf(fill.data(), 4, 32, boxes), 1);
    const std::vector<uint8_t> words = quad(500.0f, 100.0f, 560.0f, 112.0f);  // a name elsewhere
    CHECK_EQ(gaugescale::boxOf(words.data(), 4, 32, boxes), -1);
    const std::vector<uint8_t> edge = quad(99.0f, 199.0f, 161.0f, 210.0f);  // its frame, a pixel round it
    CHECK_EQ(gaugescale::boxOf(edge.data(), 4, 32, boxes), 0);
}

TEST(gauge_scale_a_stack_of_gauges_shrinks_toward_its_top) {
    // an actor's HP/SP gauge and its cast's under it: one anchor, the top middle of the first; another actor's apart
    const std::vector<std::array<int, 4>> boxes = {{100, 210, 60, 6}, {300, 250, 60, 5}, {100, 200, 60, 9}};
    const std::vector<std::array<float, 2>> a = gaugescale::anchors(boxes);
    CHECK_EQ(a.size(), size_t(3));
    if (a.size() == 3) {
        CHECK_EQ(a[0][0], 130.0f);
        CHECK_EQ(a[0][1], 200.0f);
        CHECK_EQ(a[2][0], 130.0f);
        CHECK_EQ(a[2][1], 200.0f);
        CHECK_EQ(a[1][0], 330.0f);
        CHECK_EQ(a[1][1], 250.0f);
    }
    CHECK(gaugescale::gaugeClass(0x696D4C) && gaugescale::gaugeClass(0x696C18));
    CHECK(!gaugescale::gaugeClass(0x696068));  // the words over a head: not a gauge
}
