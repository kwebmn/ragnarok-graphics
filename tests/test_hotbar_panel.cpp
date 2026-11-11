// 3.0.11-beta.40: the hotbar (core/hotbar_panel) - its rows in both modes, the battle mode's button (3.0.11-beta.147:
// in the pencil's place), the mouse over it, its cells' picture and words, a cell's icon lifted and let go
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "core/hotbar_panel.hpp"
#include "core/pad_glyphs.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool inside(const panel::Box& outer, const panel::Box& inner) {
    return inner.x >= outer.x - 1e-3f && inner.y >= outer.y - 1e-3f && inner.x + inner.w <= outer.x + outer.w + 1e-3f &&
           inner.y + inner.h <= outer.y + outer.h + 1e-3f;
}

hotbar::Texts texts() {
    hotbar::Texts t;
    t.battle = "бойовий режим";
    t.battleHint = "літери — навички й предмети \xC2\xB7 Enter — чат";
    t.battleOn = "увімкнути бойовий режим";
    t.battleOff = "вимкнути бойовий режим";
    t.use = "клік — використати";
    t.noSp = "Немає SP \xC2\xB7 потрібно {1}, у вас {2}";
    t.notHere = "Не тут \xC2\xB7 {1}";
    t.reasons.assign(size_t(hotbar::NotHere::kCount), "");
    t.reasons[size_t(hotbar::NotHere::Teleport)] = "на цій карті телепорт заборонено";
    t.unlearned = "Персонаж не має цієї навички";
    return t;
}

hotbar::Show hovering(int cell) {
    hotbar::Show s;
    s.hover = cell;
    return s;
}

}  // namespace

TEST(hotbar_battle_rows_and_keys) {
    CHECK(hotbar::gameRow(0) == 1 && hotbar::gameRow(1) == 2 && hotbar::gameRow(2) == 0);
    CHECK(std::string(hotbar::battleKey(0, 0)) == "Q" && std::string(hotbar::battleKey(0, 8)) == "O");
    CHECK(std::string(hotbar::battleKey(1, 8)) == "L" && std::string(hotbar::battleKey(2, 0)) == "Z");
    CHECK(std::string(hotbar::battleKey(2, 7)) == "," && std::string(hotbar::battleKey(2, 8)) == ".");
    CHECK(hotbar::iconSide(hotbar::Mode::Normal, 1) == 32 && hotbar::iconSide(hotbar::Mode::Normal, 2) == 64);
}

TEST(hotbar_layout) {
    const hotbar::Layout N = hotbar::layout(hotbar::Mode::Normal, 960, 1070, 1.0f);
    CHECK(N.cells.size() == 9 && N.rowLabels.empty() && N.header.w == 0);
    CHECK(std::abs(N.y + int(N.glass.y + N.glass.h) - 1070) <= 1);              // its bottom where asked
    CHECK(std::abs(N.x + int(N.glass.x + N.glass.w / 2) - 960) <= 1);          // its middle too
    CHECK(N.tipRoom.h > 0 && N.tipRoom.y + N.tipRoom.h <= N.glass.y + 1e-3f);  // the tips over it
    CHECK(N.row.x + N.row.w <= N.cells[0].x && N.toggle.x >= N.cells[8].x + N.cells[8].w);
    for (size_t i = 0; i < N.cells.size(); ++i) {
        CHECK(inside(N.glass, N.cells[i]) && N.cells[i].w == 48 && N.cells[i].h == 48);
        if (i) CHECK(N.cells[i].x > N.cells[i - 1].x + N.cells[i - 1].w && N.cells[i].y == N.cells[0].y);
    }
    const hotbar::Layout B = hotbar::layout(hotbar::Mode::Battle, 960, 1070, 1.0f);
    CHECK(B.cells.size() == 27 && B.rowLabels.size() == 3 && B.header.w > 0 && B.header.y < B.cells[0].y);
    for (int r = 0; r < 3; ++r)
        for (int c = 1; c < 9; ++c) {
            const panel::Box& a = B.cells[size_t(r * 9 + c - 1)];
            const panel::Box& b = B.cells[size_t(r * 9 + c)];
            CHECK(b.x > a.x && b.y == a.y && inside(B.glass, b));
        }
    CHECK(B.cells[9].y > B.cells[0].y && B.cells[18].y > B.cells[9].y);
    const hotbar::Layout N2 = hotbar::layout(hotbar::Mode::Normal, 1920, 2140, 2.0f);
    CHECK(N2.cells[0].w == 96 && std::fabs(N2.glass.w - 2 * N.glass.w) < 1.0f);
}

TEST(hotbar_mouse) {
    const hotbar::Layout N = hotbar::layout(hotbar::Mode::Normal, 960, 1070, 1.0f);
    for (int i = 0; i < 9; ++i) {
        const hotbar::Hit h = hotbar::hitTest(N, N.cells[size_t(i)].x + 24, N.cells[size_t(i)].y + 24);
        CHECK(h.part == hotbar::Part::Cell && h.cell == i);
    }
    CHECK(hotbar::hitTest(N, N.toggle.x + 4, N.toggle.y + 4).part == hotbar::Part::Toggle);
    CHECK(hotbar::hitTest(N, N.row.x + 4, N.row.y + 4).part == hotbar::Part::Body);
    CHECK(hotbar::hitTest(N, N.tipRoom.x + 4, N.tipRoom.y + 4).part == hotbar::Part::None);
    const hotbar::Layout B = hotbar::layout(hotbar::Mode::Battle, 960, 1070, 1.0f);
    const hotbar::Hit h = hotbar::hitTest(B, B.cells[20].x + 3, B.cells[20].y + 3);
    CHECK(h.part == hotbar::Part::Cell && h.cell == 20);
}

// The game's tables and the session's slots (win/hotbar reads them out of the client)
TEST(hotbar_tables_and_slots) {
    const std::map<std::string, std::string> t =
        hotbar::nameTable("//file start\r\n501#Red_Potion#\r\nAC_DOUBLE#Double_Strafe#\n\n#bad#\n601#Fly_Wing#");
    CHECK(t.size() == 3 && t.at("501") == "Red_Potion" && t.at("AC_DOUBLE") == "Double_Strafe");
    CHECK(t.at("601") == "Fly_Wing");
    CHECK(hotbar::spaced("Owl's_Eye") == "Owl's Eye" && hotbar::spaced("") == "");
    CHECK(hotbar::lowered("AC_DOUBLE") == "ac_double");
    const hotbar::Slot skill = hotbar::slotOf("##AC_DOUBLE");
    CHECK(skill.kind == hotbar::Slot::Kind::Skill && skill.id == "AC_DOUBLE");
    const hotbar::Slot item = hotbar::slotOf("501");
    CHECK(item.kind == hotbar::Slot::Kind::Item && item.id == "501");
    CHECK(hotbar::slotOf("").kind == hotbar::Slot::Kind::None);
    CHECK(hotbar::slotOf("nothing").kind == hotbar::Slot::Kind::None);
    CHECK(hotbar::slotOf("##").kind == hotbar::Slot::Kind::None);
}

// The server's rules over the map's flags (uAthena pc_isUseitem, skill_not_ok, db/skill_nocast_db.txt)
TEST(hotbar_not_here_rules) {
    using hotbar::NotHere;
    const auto item = [](const char* n) { return hotbar::slotOf(n); };
    const auto skill = [](const std::string& n) { return hotbar::slotOf("##" + n); };
    const std::vector<std::string> plain, noTp{"noteleport", "nomemo"}, war{"gvg"}, pvp{"pvp"};
    const std::vector<std::string> noReturn{"noreturn"}, noBranch{"nobranch"}, noSkill{"noskill", "nowarp"};
    CHECK(hotbar::notHere(item("601"), plain) == NotHere::No);
    CHECK(hotbar::notHere(item("601"), noTp) == NotHere::Teleport);
    CHECK(hotbar::notHere(item("601"), war) == NotHere::GuildWar);
    CHECK(hotbar::notHere(item("602"), noTp) == NotHere::No);
    CHECK(hotbar::notHere(item("602"), noReturn) == NotHere::Return);
    CHECK(hotbar::notHere(item("604"), noBranch) == NotHere::Branch);
    CHECK(hotbar::notHere(item("12103"), war) == NotHere::GuildWar);
    CHECK(hotbar::notHere(item("605"), war) == NotHere::GuildWar);
    CHECK(hotbar::notHere(item("501"), noSkill) == NotHere::No);
    CHECK(hotbar::notHere(skill("AL_TELEPORT"), noTp) == NotHere::Teleport);
    CHECK(hotbar::notHere(skill("AL_TELEPORT"), war) == NotHere::GuildWar);
    CHECK(hotbar::notHere(skill("AL_TELEPORT"), noSkill) == NotHere::No);  // the server lets it through noskill
    CHECK(hotbar::notHere(skill("AL_WARP"), noSkill) == NotHere::Warp);
    CHECK(hotbar::notHere(skill("WE_CALLPARTNER"), noTp) == NotHere::Call);
    CHECK(hotbar::notHere(skill("MC_VENDING"), noSkill) == NotHere::No);
    CHECK(hotbar::notHere(skill("AC_DOUBLE"), noSkill) == NotHere::Skills);
    CHECK(hotbar::notHere(skill("AC_DOUBLE"), war) == NotHere::No);
    CHECK(hotbar::notHere(skill("CG_HERMODE"), plain) == NotHere::OnlyGuildWar);
    CHECK(hotbar::notHere(skill("CG_HERMODE"), pvp) == NotHere::OnlyGuildWar);
    CHECK(hotbar::notHere(skill("CG_HERMODE"), war) == NotHere::No);
    CHECK(hotbar::notHere(skill("CG_HERMODE"), {"gvg_castle"}) == NotHere::No);  // a castle: its war unknown
    CHECK(hotbar::notHere(hotbar::slotOf(""), noSkill) == NotHere::No);
}

// 3.0.11-beta.117 (R-8YW96B: «при хотьбі по світу, час від часу панель скілів від Ф1 до Ф9 буквально на секунду
// зміщується в право і повертається назад»): a window over the bar's place moves it only once it has stood there 1.5 s
TEST(hotbar_moves_aside_only_for_a_window_that_stays) {
    hotbar::Settle settle;
    CHECK(!settle.settled(true, 10000, 1500));  // a window just came over its place
    CHECK(!settle.settled(true, 11000, 1500));
    CHECK(settle.settled(true, 11500, 1500));   // it stayed: the bar moves aside
    CHECK(!settle.settled(false, 11600, 1500));  // gone: back at once
    CHECK(!settle.settled(true, 11700, 1500));   // again: counted anew
    CHECK(settle.settled(true, 13200, 1500));
}

// 3.0.11-beta.125: when the pill's warning shows - a fall under 50 or to 10 or fewer its words 5 s, none left as long as
// it lasts; more again (a new stack) quiet at once; none worn nothing
TEST(hotbar_ammo_warns_as_it_falls) {
    CHECK(hotbar::ammoLevel(-1) == 0 && hotbar::ammoLevel(50) == 0 && hotbar::ammoLevel(49) == 1);
    CHECK(hotbar::ammoLevel(11) == 1 && hotbar::ammoLevel(10) == 2 && hotbar::ammoLevel(1) == 2);
    CHECK(hotbar::ammoLevel(0) == 3);
    hotbar::AmmoWarn w;
    CHECK(w.now(437, 1000) == 0);
    CHECK(w.now(49, 2000) == 1);   // under 50: told
    CHECK(w.now(45, 6900) == 1);   // its 5 s
    CHECK(w.now(44, 7000) == 0);   // then the count alone
    CHECK(w.now(10, 8000) == 2);   // 10 or fewer: told again
    CHECK(w.now(3, 13100) == 0);
    CHECK(w.now(0, 14000) == 3);   // none left
    CHECK(w.now(0, 40000) == 3);   // as long as it lasts
    CHECK(w.now(500, 41000) == 0); // a new stack: quiet
    CHECK(w.now(-1, 42000) == 0);
    hotbar::AmmoWarn login;
    CHECK(login.now(30, 1000) == 1);  // a few at the start: told too
    CHECK(login.now(400, 2000) == 0);  // more again: quiet at once
}

// 3.0.11-beta.129 (the owner «делай»: a sound when 10 arrows are left and when none are): a new warning's level once
// (`fresh`) - the sound's moment; the count not known a while (a map loading) changes nothing - the same stack low
// again is not told again
TEST(hotbar_ammo_warns_once_across_a_maps_loading) {
    hotbar::AmmoWarn w;
    CHECK(w.now(12, 1000) == 1 && w.fresh == 1);
    CHECK(w.now(11, 2000) == 1 && w.fresh == 0);
    CHECK(w.now(10, 3000) == 2 && w.fresh == 2);
    CHECK(w.now(-1, 4000) == 0 && w.fresh == 0);  // a map loading: not known
    CHECK(w.now(10, 5000) == 2 && w.fresh == 0);   // the same low: its words go on, not told again
    CHECK(w.now(0, 9000) == 3 && w.fresh == 3);
    CHECK(w.now(0, 9500) == 3 && w.fresh == 0);
    CHECK(w.now(300, 10000) == 0 && w.fresh == 0);  // a new stack
}

// 3.0.11-beta.147: the game's slot each shown cell keeps - the normal mode's of the row shown, the battle mode's rows
// Q, A, Z the game's 1, 2, 0 (whatever row the normal mode shows)
TEST(hotbar_cells_slots) {
    CHECK(hotbar::gameSlot(hotbar::Mode::Normal, 0, 0) == 0 && hotbar::gameSlot(hotbar::Mode::Normal, 2, 8) == 26);
    CHECK(hotbar::gameSlot(hotbar::Mode::Normal, 1, 3) == 12);
    CHECK(hotbar::gameSlot(hotbar::Mode::Battle, 2, 0) == 9);   // Q: the game's row 1
    CHECK(hotbar::gameSlot(hotbar::Mode::Battle, 0, 9) == 18);  // A: its row 2
    CHECK(hotbar::gameSlot(hotbar::Mode::Battle, 0, 26) == 8);  // «.»: its row 0
    CHECK(hotbar::gameSlot(hotbar::Mode::Normal, 0, 9) == -1 && hotbar::gameSlot(hotbar::Mode::Battle, 0, 27) == -1);
    CHECK(hotbar::gameSlot(hotbar::Mode::Normal, 0, -1) == -1);
}

// 3.0.11-beta.147 (the owner: «иногда панель сама двигается» - a click whose pointer slipped dragged the whole bar): a
// press on a cell's icon lifts it only held 0.25 s and moved 10 pixels x the scale - a quicker one, wherever it is let
// go, stays the pressed cell's click
TEST(hotbar_lift_click_or_drag) {
    hotbar::Lift l;
    hotbar::press(l, 3, 100, 50, 1000);
    CHECK(!hotbar::moved(l, 130, 50, 1148, 1.0f));  // R-VYV3AG: slipped in 148 ms - still a click
    CHECK(hotbar::moved(l, 130, 50, 1250, 1.0f));   // held on: lifted
    CHECK(hotbar::moved(l, 102, 50, 1300, 1.0f));   // and stays lifted back near
    hotbar::press(l, 4, 100, 50, 2000);
    CHECK(l.cell == 4 && !l.dragging);
    CHECK(!hotbar::moved(l, 109, 50, 2600, 1.0f));  // held still
    CHECK(!hotbar::moved(l, 118, 50, 2600, 2.0f));  // 18 px at scale 2: still
    CHECK(hotbar::moved(l, 121, 50, 2600, 2.0f));
    hotbar::Lift none;
    CHECK(!hotbar::moved(none, 500, 500, 99999, 1.0f));  // nothing pressed
}

// 3.0.11-beta.147: a lifted icon let go - over another cell: the two swapped; well off the bar (24 px x the scale past
// its glass): its slot emptied; over its own cell, the bar's other parts or just past its edge: nothing
TEST(hotbar_lifted_icon_let_go) {
    const hotbar::Layout L = hotbar::layout(hotbar::Mode::Normal, 400, 200, 1.0f);
    const auto at = [&](const hotbar::Layout& Y, int from, const panel::Box& b) {
        return hotbar::letGo(Y, from, b.x + b.w / 2, b.y + b.h / 2);
    };
    hotbar::Release r = at(L, 2, L.cells[6]);
    CHECK(r.what == hotbar::Let::Swap && r.cell == 6);
    CHECK(at(L, 2, L.cells[2]).what == hotbar::Let::Nothing);
    CHECK(at(L, 2, L.toggle).what == hotbar::Let::Nothing && at(L, 2, L.row).what == hotbar::Let::Nothing);
    CHECK(hotbar::letGo(L, 2, L.glass.x + 100, L.glass.y - 10).what == hotbar::Let::Nothing);  // just over its edge
    r = hotbar::letGo(L, 2, L.glass.x + 100, L.glass.y - 30);
    CHECK(r.what == hotbar::Let::Empty && r.cell == 2);
    CHECK(hotbar::letGo(L, 2, -500, -400).what == hotbar::Let::Empty);  // far off, outside its picture
    CHECK(hotbar::away(L, -500, -400) && !hotbar::away(L, L.glass.x + 100, L.glass.y - 10));
    const hotbar::Layout B = hotbar::layout(hotbar::Mode::Battle, 400, 300, 1.0f);
    r = at(B, 0, B.cells[26]);
    CHECK(r.what == hotbar::Let::Swap && r.cell == 26);
    CHECK(hotbar::letGo(L, -1, -500, -400).what == hotbar::Let::Nothing);  // nothing lifted
}

// 3.0.11-beta.147: a lifted icon at the cursor - on a dark plate, a little faded; well off the bar a red badge with a
// minus at its top right (let go there - taken off the bar)
TEST(hotbar_lifted_icon_picture) {
    panel::Picture icon;
    icon.width = icon.height = 32;
    icon.px.assign(32 * 32, 0xFF3060C0u);
    const int side = hotbar::liftedSide(hotbar::Mode::Normal, 1.0f);
    CHECK(side >= 40 && side <= 56);
    const auto paint = [&](bool away) {
        panel::Picture p;
        p.width = p.height = side;
        p.px.assign(size_t(side) * size_t(side), 0u);
        hotbar::paintLifted(p, &icon, away, 1.0f);
        return p;
    };
    const auto red = [](const panel::Picture& q) {
        int n = 0;
        for (const uint32_t px : q.px) n += ((px >> 16) & 0xFF) > 0xA0 && ((px >> 8) & 0xFF) < 0x70;
        return n;
    };
    const panel::Picture near = paint(false), far = paint(true);
    const uint32_t c = near.px[size_t(side / 2) * size_t(side) + size_t(side / 2)];
    CHECK((c >> 24) > 0x80 && (c & 0xFF) > 0x60 && (c & 0xFF) < 0xC0);  // the icon there, a little faded
    CHECK(red(near) == 0 && red(far) > 20);
    CHECK(hotbar::liftedSide(hotbar::Mode::Normal, 2.0f) == 2 * side);
}

