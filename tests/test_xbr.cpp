#include <cstdint>
#include <vector>

#include "core/xbr.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

constexpr uint32_t kWhite = 0xFFFFFFFFu, kBlack = 0xFF000000u, kRed = 0xFFFF0000u;

struct Picture {
    int w, h;
    std::vector<uint32_t> px;
    Picture(int width, int height, uint32_t fill) : w(width), h(height), px(size_t(width) * size_t(height), fill) {}
    void set(int x, int y, uint32_t c) { px[size_t(y) * size_t(w) + size_t(x)] = c; }
};

uint32_t at(const std::vector<uint32_t>& big, int w, int x, int y) {
    return big[size_t(y) * size_t(4 * w) + size_t(x)];
}

int green(uint32_t c) { return int((c >> 8) & 255); }

}  // namespace

TEST(xbr_four_times_larger_and_flat_stays_flat) {
    // 0.41: the windows' buttons, frames and labels - four times larger, a flat colour as it was.
    const Picture p(3, 2, 0xFF336699u);
    const std::vector<uint32_t> big = xbr::scale4(p.px.data(), p.w, p.h);
    CHECK_EQ(int(big.size()), 12 * 8);
    bool flat = true;
    for (uint32_t c : big) flat = flat && c == 0xFF336699u;
    CHECK(flat);
}

TEST(xbr_keeps_straight_lines_and_right_angles_sharp) {
    // A stroke one pixel thick, upright, stays four pixels wide and pure; a box keeps its square corners.
    Picture line(5, 5, kWhite);
    for (int y = 0; y < 5; ++y) line.set(2, y, kBlack);
    const std::vector<uint32_t> a = xbr::scale4(line.px.data(), 5, 5);
    bool pure = true;
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 20; ++x) pure = pure && at(a, 5, x, y) == (x >= 8 && x < 12 ? kBlack : kWhite);
    CHECK(pure);
    Picture box(7, 7, kWhite);
    for (int y = 2; y < 5; ++y)
        for (int x = 2; x < 5; ++x) box.set(x, y, kRed);
    const std::vector<uint32_t> b = xbr::scale4(box.px.data(), 7, 7);
    CHECK_EQ(at(b, 7, 8, 8), kRed);    // the box's corners, inside
    CHECK_EQ(at(b, 7, 19, 8), kRed);
    CHECK_EQ(at(b, 7, 8, 19), kRed);
    CHECK_EQ(at(b, 7, 19, 19), kRed);
    CHECK_EQ(at(b, 7, 7, 7), kWhite);  // and outside
    CHECK_EQ(at(b, 7, 20, 20), kWhite);
}

TEST(xbr_smooths_a_diagonal_and_keeps_it_whole) {
    // A stroke one pixel thick going down to the right: each step's outer corners give way to the white, the inner
    // ones of the white take the black - the stroke stays whole along its middle.
    Picture p(8, 8, kWhite);
    for (int k = 0; k < 8; ++k) p.set(k, k, kBlack);
    const std::vector<uint32_t> big = xbr::scale4(p.px.data(), 8, 8);
    bool whole = true;
    for (int k = 2; k < 6; ++k) whole = whole && green(at(big, 8, 4 * k + 2, 4 * k + 2)) < 64;
    CHECK(whole);
    CHECK(green(at(big, 8, 4 * 3 + 3, 4 * 3)) > 128);  // the step's upper right corner: white now
    CHECK(green(at(big, 8, 4 * 3, 4 * 3 + 3)) > 128);  // and its lower left
    bool between = false;  // somewhere a pixel between the two: smooth, not stepped
    for (uint32_t c : big) between = between || (green(c) > 32 && green(c) < 224);
    CHECK(between);
}

TEST(xbr_keeps_a_dot_and_the_transparency) {
    // A dot alone keeps its middle; what is transparent stays so, the edge of what is not is smoothed into it.
    Picture dot(5, 5, kWhite);
    dot.set(2, 2, kBlack);
    const std::vector<uint32_t> a = xbr::scale4(dot.px.data(), 5, 5);
    CHECK_EQ(at(a, 5, 9, 9), kBlack);
    CHECK_EQ(at(a, 5, 10, 10), kBlack);
    Picture shape(6, 6, 0u);  // a triangle on nothing
    for (int y = 1; y < 5; ++y)
        for (int x = 1; x <= y; ++x) shape.set(x, y, kRed);
    const std::vector<uint32_t> b = xbr::scale4(shape.px.data(), 6, 6);
    CHECK_EQ(at(b, 6, 0, 0) >> 24, 0u);
    CHECK_EQ(at(b, 6, 23, 0) >> 24, 0u);
    CHECK_EQ(at(b, 6, 6, 18), kRed);
    bool soft = false;  // the slanted edge: partly opaque, still red
    for (uint32_t c : b) soft = soft || ((c >> 24) > 16 && (c >> 24) < 240 && ((c >> 16) & 255) > 200);
    CHECK(soft);
}
