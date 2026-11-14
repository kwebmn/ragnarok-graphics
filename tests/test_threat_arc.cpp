// 3.0.19-beta.36 (the 3D view's concept: «Индикатор угрозы: красная дуга у края экрана с той стороны, откуда бьют»;
// «Слева красная дуга: Fabre бьёт из-за спины, вне поля зрения»): who hit the character lately, from which side of
// the view, and the arc at the screen's edge on that side
#include <cmath>

#include "core/threat_arc.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
constexpr float kPi = 3.14159265f;
bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }
}  // namespace

TEST(threat_arc_bearing_against_the_view) {
    // the view looking north (+z), its right to the east (+x)
    CHECK(near(threatarc::bearingOf(0, 0, 0, 10, 0, 1, 1, 0), 0.0f));
    CHECK(near(threatarc::bearingOf(0, 0, 10, 0, 0, 1, 1, 0), kPi / 2));
    CHECK(near(threatarc::bearingOf(0, 0, -10, 0, 0, 1, 1, 0), -kPi / 2));
    CHECK(near(std::fabs(threatarc::bearingOf(0, 0, 0, -10, 0, 1, 1, 0)), kPi));  // behind
    CHECK(near(threatarc::bearingOf(5, 5, 15, 15, 0, 1, 1, 0), kPi / 4));         // from where the character is
}

TEST(threat_arc_at_the_edge_on_its_side) {
    std::vector<own_ground::Tl1> v;
    threatarc::arc(1000, 600, kPi / 2, 1.0f, v);  // to the right
    CHECK(v.size() == size_t(threatarc::kSegments) * 6u);
    bool right = !v.empty();
    for (const own_ground::Tl1& p : v) {
        right = right && p.x > 500.0f + 200.0f;
        const float r = std::hypot(p.x - 500.0f, p.y - 300.0f);
        const float outer = threatarc::kRadius * 600.0f, inner = (threatarc::kRadius - threatarc::kThick) * 600.0f;
        right = right && r <= outer + 0.5f && r >= inner - 0.5f;
    }
    CHECK(right);
    CHECK((v[0].diffuse >> 24) == 0xD9u && (v[0].diffuse & 0xFFFFFFu) == (threatarc::kColour & 0xFFFFFFu));
    v.clear();
    threatarc::arc(1000, 600, 0.0f, 0.5f, v);  // ahead: the top
    bool top = !v.empty();
    for (const own_ground::Tl1& p : v) top = top && p.y < 300.0f;
    CHECK(top && (v[0].diffuse >> 24) < 0x80u);
    v.clear();
    threatarc::arc(1000, 600, kPi, 1.0f, v);  // behind: the bottom
    bool bottom = !v.empty();
    for (const own_ground::Tl1& p : v) bottom = bottom && p.y > 300.0f;
    CHECK(bottom);
}

TEST(threat_arc_fades_after_its_hit) {
    threatarc::Hits h;
    h.hit(7, 1000);
    CHECK(near(h.alphaOf(7, 1000), 1.0f));
    const float half = h.alphaOf(7, 1000 + threatarc::kHoldMs / 2);
    CHECK(half > 0.4f && half < 0.6f);
    CHECK(near(h.alphaOf(7, 1000 + threatarc::kHoldMs), 0.0f));
    CHECK(near(h.alphaOf(8, 1000), 0.0f));  // never hit
    h.hit(7, 3000);                         // again: whole again
    CHECK(near(h.alphaOf(7, 3000), 1.0f));
    h.forget(3000 + threatarc::kHoldMs + 1);
    CHECK(h.last.empty());
    h.hit(9, 100);
    h.gone(9);
    CHECK(h.last.empty());
}

// 3.0.19-beta.37 (the owner: «нет, но наверное надо писать растояние пускай клетка будет метр»): the attacker's
// distance by the arc, a walk cell (5 of the world's units) a metre, inside the arc at its middle
TEST(threat_arc_distance_by_it) {
    CHECK(threatarc::cellsOf(0.0f) == 0 && threatarc::cellsOf(24.0f) == 5 && threatarc::cellsOf(26.0f) == 5);
    CHECK(threatarc::cellsOf(28.0f) == 6 && threatarc::cellsOf(3.0f) == 1);  // at least a metre once apart
    CHECK(threatarc::metres(7, false) == "7 м" && threatarc::metres(12, true) == "12 m");
    float x = 0, y = 0;
    threatarc::labelAt(1000, 600, kPi / 2, x, y);  // to the right: inside the arc, at its height
    CHECK(x > 500.0f && x < 500.0f + (threatarc::kRadius - threatarc::kThick) * 600.0f && std::fabs(y - 300.0f) < 1.0f);
    threatarc::labelAt(1000, 600, kPi, x, y);  // behind: under the middle
    CHECK(std::fabs(x - 500.0f) < 1.0f && y > 300.0f);
}
