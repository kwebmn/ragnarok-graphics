// 3.0.11-beta.39: the party's panel (core/party_panel) - its rows, low HP, the members elsewhere and offline
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/party_panel.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool inside(const panel::Box& outer, const panel::Box& inner) {
    return inner.x >= outer.x - 1e-3f && inner.y >= outer.y - 1e-3f && inner.x + inner.w <= outer.x + outer.w + 1e-3f &&
           inner.y + inner.h <= outer.y + outer.h + 1e-3f;
}

std::vector<partypanel::Member> party() {
    std::vector<partypanel::Member> m(4);
    m[0].name = "Tor";
    m[0].leader = true;
    m[0].percent = 96;
    m[1].name = "Mira";
    m[1].percent = 28;
    m[2].name = "Lis";
    m[2].where = "prontera";
    m[3].name = "Ash";
    m[3].online = false;
    return m;
}

}  // namespace

TEST(party_panel_rows) {
    const std::vector<partypanel::Member> m = party();
    CHECK(!partypanel::low(m[0], 30) && partypanel::low(m[1], 30) && !partypanel::low(m[2], 30));
    CHECK(!partypanel::low(m[3], 30));  // offline: not low, not known
    const partypanel::Layout L = partypanel::layout(m.size(), 10, 220, 1.0f);
    CHECK(L.x == 10 && L.y == 220 && L.rows.size() == 4 && L.glass.w >= 240);
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const partypanel::Row& r = L.rows[i];
        CHECK(inside(L.glass, r.box) && inside(r.box, r.name) && inside(r.box, r.value) && inside(r.box, r.bar));
        CHECK(r.bar.h <= 5.01f && r.bar.y >= r.name.y + r.name.h - 1 && r.value.x > r.name.x);
        if (i) CHECK(r.box.y >= L.rows[i - 1].box.y + L.rows[i - 1].box.h);
    }
    CHECK(partypanel::layout(0, 10, 220, 1.0f).rows.empty());
    const partypanel::Layout L2 = partypanel::layout(m.size(), 20, 440, 2.0f);
    CHECK(std::fabs(L2.glass.w - 2 * L.glass.w) < 1.0f);
}

// 3.0.11-beta.80 (the owner: «чтобы видить какие касты стоят у персонажей какимито маленькими иконками»): a member's
// buffs as small icons under its bar, each with its time left under it when known; the row a line taller
TEST(party_panel_member_buffs) {
    std::vector<partypanel::Member> m = party();
    panel::Picture icon;
    icon.width = icon.height = 14;
    icon.px.assign(14 * 14, 0xFF2050E0u);
    m[0].buffs = {{&icon, 0.5f}, {&icon, -1.0f}};
    const partypanel::Layout plain = partypanel::layout(partypanel::rowsOf(party()), 0, 0, 1.0f);
    const partypanel::Layout L = partypanel::layout(partypanel::rowsOf(m), 0, 0, 1.0f);
    CHECK(L.rows[0].box.h > plain.rows[0].box.h && L.rows[1].box.h == plain.rows[1].box.h && L.h > plain.h);
    CHECK(L.rows[0].buffs.size() == 2 && inside(L.rows[0].box, L.rows[0].buffs[1]));
    CHECK(L.rows[0].buffs[0].y >= L.rows[0].bar.y + L.rows[0].bar.h);
    const panel::Box& b = L.rows[0].buffs[0];
    // its icon

    // more than fit: as many as the row holds

    m[0].buffs.assign(40, partypanel::Buff{&icon, 0.3f});
    const partypanel::Layout many = partypanel::layout(partypanel::rowsOf(m), 0, 0, 1.0f);
    CHECK(!many.rows[0].buffs.empty() && many.rows[0].buffs.size() < 40);
    CHECK(inside(many.rows[0].box, many.rows[0].buffs.back()));
}

