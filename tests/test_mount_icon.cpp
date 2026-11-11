#include "core/mount_icon.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.116 (the owner chose «2 Значок між стрілками»): the dismount sign - a door's bracket on the
// left, an arrow out of it to the right; premultiplied, see-through round it
TEST(mount_icon_paints_a_bracket_and_an_arrow) {
    const int s = 32;
    std::vector<uint32_t> px(size_t(s * s), 0);
    mounticon::paint(px, s, s, 0xE6CB8A);
    const auto alpha = [&](int x, int y) { return int(px[size_t(y * s + x)] >> 24); };
    CHECK(alpha(0, 0) == 0 && alpha(s - 1, s - 1) == 0);  // the corners clear
    CHECK(alpha(s / 2 + 5, s / 2) > 128);                  // the arrow's shaft right of the middle
    CHECK(alpha(int(s * 0.22f), s / 2) > 128);             // the bracket's upright on the left
    int lit = 0;
    for (uint32_t p : px) lit += (p >> 24) > 0;
    CHECK(lit > 60 && lit < s * s / 2);
    for (uint32_t p : px) {  // premultiplied: no colour over its alpha
        const uint32_t a = p >> 24;
        CHECK(((p >> 16) & 0xFF) <= a && ((p >> 8) & 0xFF) <= a && (p & 0xFF) <= a);
    }
}
