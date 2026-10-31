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

// 3.0.11-beta.80 (the owner: «иконки можно сделать как лица персонажей?»): a member's face in its round, no letter
TEST(party_panel_faces) {
    std::vector<partypanel::Member> m = party();
    panel::Picture face;
    face.width = face.height = 20;
    face.px.assign(400, 0xFFC08040u);  // premultiplied, opaque
    m[0].face = &face;
    const partypanel::Layout L = partypanel::layout(m.size(), 0, 0, 1.0f);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    partypanel::paint(p, L, m, 30);
    const partypanel::Row& r = L.rows[0];
    const uint32_t middle =
        p.px[size_t(int(r.round.y + r.round.h / 2)) * size_t(p.width) + size_t(int(r.round.x + r.round.w / 2))];
    CHECK(((middle >> 16) & 0xFF) > 0xA0 && ((middle >> 8) & 0xFF) > 0x60 && (middle & 0xFF) < 0x60);
    const uint32_t corner = p.px[size_t(int(r.round.y + 1)) * size_t(p.width) + size_t(int(r.round.x + 1))];
    CHECK(((corner >> 16) & 0xFF) < 0x80);  // the round clips it: its corner not the face's
    const std::vector<partypanel::Words> w = partypanel::words(L, m, 30, "офлайн", "Група");
    CHECK(std::none_of(w.begin(), w.end(), [](const partypanel::Words& x) { return x.text == "T"; }));
    CHECK(std::any_of(w.begin(), w.end(), [](const partypanel::Words& x) { return x.text == "M"; }));  // no face: M
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
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    partypanel::paint(p, L, m, 30);
    const panel::Box& b = L.rows[0].buffs[0];
    const uint32_t c = p.px[size_t(int(b.y + b.h / 2)) * size_t(p.width) + size_t(int(b.x + b.w / 2))];
    CHECK((c & 0xFF) > 0xC0 && ((c >> 16) & 0xFF) < 0x60);  // its icon
    // more than fit: as many as the row holds
    m[0].buffs.assign(40, partypanel::Buff{&icon, 0.3f});
    const partypanel::Layout many = partypanel::layout(partypanel::rowsOf(m), 0, 0, 1.0f);
    CHECK(!many.rows[0].buffs.empty() && many.rows[0].buffs.size() < 40);
    CHECK(inside(many.rows[0].box, many.rows[0].buffs.back()));
}

// 3.0.11-beta.81 (the owner: «1-8 делай»): a member's class and level after its name, its cast (the skill's words in
// their place and a line under its bar), dead (its row dark red, «мертвий»), a buff of one's own about to end glowing
TEST(party_panel_class_cast_dead_glow) {
    std::vector<partypanel::Member> m = party();
    m[0].job = "Priest 85";
    m[1].casting = "Heal";
    m[1].castPart = 0.5f;
    m[2].where.clear();  // here
    m[2].dead = true;
    panel::Picture icon;
    icon.width = icon.height = 14;
    icon.px.assign(14 * 14, 0xFF2050E0u);
    m[0].buffs = {{&icon, 0.05f, 1.0f}};
    const partypanel::Layout L = partypanel::layout(partypanel::rowsOf(m), 0, 0, 1.0f);
    const std::vector<partypanel::Words> w = partypanel::words(L, m, 30, "офлайн", "Група", "мертвий");
    const auto find = [&](const std::string& text) -> const partypanel::Words* {
        for (const partypanel::Words& x : w)
            if (x.text == text) return &x;
        return nullptr;
    };
    const partypanel::Words* job = find("Priest 85");
    CHECK(job && job->box.x > L.rows[0].name.x + 10 && job->rgb != find("Tor")->rgb);
    CHECK(find("Heal") != nullptr);
    CHECK(find("мертвий") != nullptr && find("мертвий")->rgb != find("96 %")->rgb);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    partypanel::paint(p, L, m, 30);
    const partypanel::Row& r1 = L.rows[1];  // the cast's line under the bar: blue
    const uint32_t cast = p.px[size_t(int(r1.bar.y + r1.bar.h + 2.5f)) * size_t(p.width) + size_t(int(r1.bar.x + 3))];
    CHECK((cast & 0xFF) > 0xA0 && (cast & 0xFF) > ((cast >> 8) & 0xFF));
    const partypanel::Row& r2 = L.rows[2];  // dead: its row dark red
    const uint32_t dead =
        p.px[size_t(int(r2.box.y + r2.box.h / 2)) * size_t(p.width) + size_t(int(r2.box.x + r2.box.w * 0.6f))];
    CHECK(((dead >> 16) & 0xFF) > ((dead >> 8) & 0xFF) + 10);
    const panel::Box& b = L.rows[0].buffs[0];  // the glow: gold round the icon
    const uint32_t edge = p.px[size_t(int(b.y + b.h / 2)) * size_t(p.width) + size_t(int(b.x - 1))];
    CHECK(((edge >> 16) & 0xFF) > 0x90 && ((edge >> 8) & 0xFF) > 0x70);
}

// 3.0.11-beta.82 (the owner's 2 and 4): a member here - an arrow its way (as the screen turns) and its cells away at
// the end of its bar's line; elsewhere - a route mark by its map's name; a skill waiting for a target - the rows
// edged gold (a click on one casts it there)
TEST(party_panel_arrows_routes_targeting) {
    std::vector<partypanel::Member> m = party();
    m[0].cells = 12;
    m[0].dirX = 1;  // to the right
    m[0].dirY = 0;
    partypanel::Layout L = partypanel::layout(partypanel::rowsOf(m), 0, 0, 1.0f);
    const partypanel::Row& r = L.rows[0];
    CHECK(r.where.w > 20 && r.where.x >= r.bar.x + r.bar.w && inside(r.box, r.where));
    const std::vector<partypanel::Words> w = partypanel::words(L, m, 30, "офлайн", "Група", "мертвий");
    CHECK(std::any_of(w.begin(), w.end(), [](const partypanel::Words& x) { return x.text == "12"; }));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    partypanel::paint(p, L, m, 30);
    int lit = 0;  // the arrow drawn in the distance's box
    for (int y = int(r.where.y); y < int(r.where.y + r.where.h); ++y)
        for (int x = int(r.where.x); x < int(r.where.x + r.where.h); ++x)
            lit += ((p.px[size_t(y) * size_t(p.width) + size_t(x)] >> 16) & 0xFF) > 0x80;
    CHECK(lit > 4);
    L.targeting = true;  // a skill waits: the rows here edged gold
    panel::Picture t;
    t.width = L.w;
    t.height = L.h;
    t.px.assign(size_t(L.w) * size_t(L.h), 0u);
    partypanel::paint(t, L, m, 30);
    const uint32_t edge = t.px[size_t(int(r.box.y + r.box.h / 2)) * size_t(t.width) + size_t(int(r.box.x))];
    CHECK(((edge >> 16) & 0xFF) > 0x90 && ((edge >> 8) & 0xFF) > 0x70 && (edge & 0xFF) < ((edge >> 16) & 0xFF));
}

// 3.0.11-beta.85 (the pad's «Выбор цели навыка»): the member the pad's support skill would go on - its row filled gold
// and edged stronger than the others waiting
TEST(party_panel_pad_choice) {
    std::vector<partypanel::Member> m = party();
    m[1].chosen = true;
    partypanel::Layout L = partypanel::layout(partypanel::rowsOf(m), 0, 0, 1.0f);
    L.targeting = true;
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    partypanel::paint(p, L, m, 30);
    auto at = [&](const partypanel::Row& r, float fx, float fy) {
        return p.px[size_t(int(r.box.y + r.box.h * fy)) * size_t(p.width) + size_t(int(r.box.x + r.box.w * fx))];
    };
    const uint32_t chosenEdge = at(L.rows[1], 0, 0.5f), otherEdge = at(L.rows[0], 0, 0.5f);
    CHECK(((chosenEdge >> 16) & 0xFF) > ((otherEdge >> 16) & 0xFF));  // the chosen edge brighter
    const uint32_t chosenInside = at(L.rows[1], 0.5f, 0.15f), otherInside = at(L.rows[0], 0.5f, 0.15f);
    CHECK(((chosenInside >> 16) & 0xFF) > ((otherInside >> 16) & 0xFF) + 8);  // and filled gold
}

// 3.0.11-beta.84 (the owner's 8: «компактный вид — одни полоски HP в сетку»): a view button in the header; compact -
// two columns of small cells (a face, a name, an HP bar), a click on one as on its row
TEST(party_panel_compact_grid) {
    std::vector<partypanel::Member> m = party();
    m.push_back(m[0]);
    m.push_back(m[1]);  // six
    const partypanel::Layout list = partypanel::layout(partypanel::rowsOf(m), 0, 0, 1.0f, false, false);
    const partypanel::Layout L = partypanel::layout(partypanel::rowsOf(m), 0, 0, 1.0f, false, true);
    CHECK(L.compact && L.rows.size() == 6 && L.h < list.h * 0.6f);
    CHECK(L.rows[1].box.x > L.rows[0].box.x + L.rows[0].box.w - 1);
    CHECK(std::fabs(L.rows[1].box.y - L.rows[0].box.y) < 0.01f);
    CHECK(L.rows[2].box.y >= L.rows[0].box.y + L.rows[0].box.h - 0.01f && L.rows[2].box.x == L.rows[0].box.x);
    for (const partypanel::Row& r : L.rows) CHECK(inside(L.glass, r.box) && inside(r.box, r.bar) && r.buffs.empty());
    CHECK(L.view.w > 0 && inside(L.header, L.view));
    CHECK(partypanel::hitTest(L, L.view.x + 2, L.view.y + 2).part == partypanel::Part::View);
    const partypanel::Hit h = partypanel::hitTest(L, L.rows[3].box.x + 3, L.rows[3].box.y + 3);
    CHECK(h.part == partypanel::Part::Row && h.row == 3);
    const std::vector<partypanel::Words> w = partypanel::words(L, m, 30, "офлайн", "Група", "мертвий");
    CHECK(std::any_of(w.begin(), w.end(), [](const partypanel::Words& x) { return x.text == "Tor"; }));
    CHECK(std::none_of(w.begin(), w.end(), [](const partypanel::Words& x) { return x.text == "96 %"; }));
    CHECK(std::any_of(w.begin(), w.end(), [](const partypanel::Words& x) { return x.text == "\xE2\x96\xA4"; }));  // ▤
    const std::vector<partypanel::Words> lw = partypanel::words(list, m, 30, "офлайн", "Група", "мертвий");
    CHECK(std::any_of(lw.begin(), lw.end(), [](const partypanel::Words& x) { return x.text == "\xE2\x96\xA6"; }));  // ▦
}
