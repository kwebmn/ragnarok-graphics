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

TEST(party_panel_picture_and_words) {
    const std::vector<partypanel::Member> m = party();
    const partypanel::Layout L = partypanel::layout(m.size(), 0, 0, 1.0f);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    partypanel::paint(p, L, m, 30);
    const auto at = [&](float x, float y) { return p.px[size_t(int(y)) * size_t(p.width) + size_t(int(x))]; };
    const partypanel::Row& tor = L.rows[0];
    const uint32_t full = at(tor.bar.x + tor.bar.w * 0.5f, tor.bar.y + tor.bar.h / 2);  // 96 %: green
    CHECK(((full >> 8) & 0xFF) > 0xA0 && ((full >> 8) & 0xFF) > ((full >> 16) & 0xFF));
    const partypanel::Row& mira = L.rows[1];
    const uint32_t lowBar = at(mira.bar.x + 3, mira.bar.y + mira.bar.h / 2);  // 28 %: red
    CHECK(((lowBar >> 16) & 0xFF) > 0xC0 && ((lowBar >> 8) & 0xFF) < 0x80);
    const std::vector<partypanel::Words> w = partypanel::words(L, m, 30, "офлайн");
    const auto has = [&](const std::string& text) {
        return std::find_if(w.begin(), w.end(), [&](const partypanel::Words& x) { return x.text == text; });
    };
    CHECK(has("Tor") != w.end() && has("96 %") != w.end() && has("prontera") != w.end() && has("офлайн") != w.end());
    CHECK(has("! Mira") != w.end() && has("28 %") != w.end() && has("28 %")->rgb != has("96 %")->rgb);
}

// 3.0.11-beta.79 (the owner: «плашка с пати её надо иметь возможность сворачивать»): a header - «Група», the members
// online of all, ▾ folding it to the header alone
TEST(party_panel_header_folds) {
    const std::vector<partypanel::Member> m = party();
    const partypanel::Layout L = partypanel::layout(m.size(), 0, 0, 1.0f, false);
    CHECK(L.header.w > 200 && L.header.h > 15 && inside(L.glass, L.header) && inside(L.header, L.fold));
    CHECK(L.rows.size() == 4 && L.rows[0].box.y >= L.header.y + L.header.h - 1e-3f);
    CHECK(partypanel::hitTest(L, L.fold.x + 2, L.fold.y + 2).part == partypanel::Part::Fold);
    CHECK(partypanel::hitTest(L, L.header.x + 4, L.header.y + 4).part == partypanel::Part::Header);
    const partypanel::Hit row = partypanel::hitTest(L, L.rows[2].box.x + 4, L.rows[2].box.y + 4);
    CHECK(row.part == partypanel::Part::Row && row.row == 2);
    CHECK(partypanel::hitTest(L, -2, 2).part == partypanel::Part::None);
    const std::vector<partypanel::Words> w = partypanel::words(L, m, 30, "офлайн", "Група");
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const partypanel::Words& x) { return x.text == text; });
    };
    CHECK(has("Група") && has("3/4") && has("\xE2\x96\xBE"));  // 3 of 4 online; ▾
    const partypanel::Layout F = partypanel::layout(m.size(), 0, 0, 1.0f, true);  // folded: the header alone
    CHECK(F.rows.empty() && F.folded && F.h < L.h / 2 && F.glass.h >= F.header.h);
    const std::vector<partypanel::Words> fw = partypanel::words(F, m, 30, "офлайн", "Група");
    CHECK(std::any_of(fw.begin(), fw.end(), [](const partypanel::Words& x) { return x.text == "\xE2\x96\xB8"; }));
    CHECK(std::none_of(fw.begin(), fw.end(), [](const partypanel::Words& x) { return x.text == "Tor"; }));
}
