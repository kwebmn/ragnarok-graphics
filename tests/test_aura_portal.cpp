// 3.0.13-beta.15 (the owner's pick 8 of «50 идей»): an aura's piece over the last frame's portal - its middle in the
// portal's box; a portal's draws gathered into one box; the boxes a frame late
#include <cstring>
#include <vector>

#include "core/aura_portal.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
std::vector<uint8_t> quad(float x0, float y0, float x1, float y1) {
    const float xy[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    std::vector<uint8_t> v(4 * 32);
    for (int i = 0; i < 4; ++i) std::memcpy(v.data() + i * 32, xy[i], 8);
    return v;
}
}  // namespace

TEST(aura_over_a_portal_known_a_frame_late) {
    auraportal::Portals p;
    const auto portal = quad(100, 100, 200, 160), portal2 = quad(150, 150, 220, 200);
    const auto aura = quad(170, 140, 190, 170), away = quad(400, 400, 420, 420);
    p.portal(portal.data(), 4, 32);
    p.portal(portal2.data(), 4, 32);  // one box
    CHECK(!p.covers(aura.data(), 4, 32));  // this frame's boxes not asked yet
    p.frame();
    CHECK(p.known() == 1);
    CHECK(p.covers(aura.data(), 4, 32) && !p.covers(away.data(), 4, 32));
    p.frame();  // a frame with no portal
    CHECK(!p.covers(aura.data(), 4, 32));
}
