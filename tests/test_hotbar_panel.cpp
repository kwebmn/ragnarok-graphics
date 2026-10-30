// 3.0.11-beta.40: the hotbar (core/hotbar_panel) - its rows in both modes, the editing button, the mouse over it,
// its cells' picture and words
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "core/hotbar_panel.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool inside(const panel::Box& outer, const panel::Box& inner) {
    return inner.x >= outer.x - 1e-3f && inner.y >= outer.y - 1e-3f && inner.x + inner.w <= outer.x + outer.w + 1e-3f &&
           inner.y + inner.h <= outer.y + outer.h + 1e-3f;
}

hotbar::Texts texts() {
    hotbar::Texts t{"бойовий режим", "літери — навички й предмети \xC2\xB7 Enter — чат", "редагувати панель гри",
                    "готово", "клік — використати"};
    t.noSp = "Немає SP \xC2\xB7 потрібно {1}, у вас {2}";
    t.notHere = "Не тут \xC2\xB7 {1}";
    t.reasons.assign(size_t(hotbar::NotHere::kCount), "");
    t.reasons[size_t(hotbar::NotHere::Teleport)] = "на цій карті телепорт заборонено";
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
    CHECK(N.row.x + N.row.w <= N.cells[0].x && N.edit.x >= N.cells[8].x + N.cells[8].w);
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
    const hotbar::Layout E = hotbar::layout(hotbar::Mode::Editing, 960, 1070, 1.0f);
    CHECK(E.cells.empty() && E.edit.w > 0 && inside(E.glass, E.edit));
    const hotbar::Layout N2 = hotbar::layout(hotbar::Mode::Normal, 1920, 2140, 2.0f);
    CHECK(N2.cells[0].w == 96 && std::fabs(N2.glass.w - 2 * N.glass.w) < 1.0f);
}

TEST(hotbar_mouse) {
    const hotbar::Layout N = hotbar::layout(hotbar::Mode::Normal, 960, 1070, 1.0f);
    for (int i = 0; i < 9; ++i) {
        const hotbar::Hit h = hotbar::hitTest(N, N.cells[size_t(i)].x + 24, N.cells[size_t(i)].y + 24);
        CHECK(h.part == hotbar::Part::Cell && h.cell == i);
    }
    CHECK(hotbar::hitTest(N, N.edit.x + 4, N.edit.y + 4).part == hotbar::Part::Edit);
    CHECK(hotbar::hitTest(N, N.row.x + 4, N.row.y + 4).part == hotbar::Part::Body);
    CHECK(hotbar::hitTest(N, N.tipRoom.x + 4, N.tipRoom.y + 4).part == hotbar::Part::None);
    const hotbar::Layout B = hotbar::layout(hotbar::Mode::Battle, 960, 1070, 1.0f);
    const hotbar::Hit h = hotbar::hitTest(B, B.cells[20].x + 3, B.cells[20].y + 3);
    CHECK(h.part == hotbar::Part::Cell && h.cell == 20);
}

TEST(hotbar_picture_and_words) {
    const hotbar::Layout L = hotbar::layout(hotbar::Mode::Normal, 400, 200, 1.0f);
    panel::Picture icon;
    icon.width = icon.height = 32;
    icon.px.assign(32 * 32, 0xFF3060C0u);
    std::vector<hotbar::Cell> cells(9);
    for (int i = 0; i < 9; ++i) cells[size_t(i)].key = "F" + std::to_string(i + 1);
    cells[0].name = "Double Strafe";
    cells[0].level = 10;
    cells[0].icon = &icon;
    cells[1].name = "Red Potion";
    cells[1].count = 38;
    cells[1].icon = &icon;
    cells[2].name = "Fly Wing";
    cells[2].count = 0;  // none in the bag: dimmed
    cells[2].icon = &icon;
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    hotbar::paint(p, L, cells, hovering(0), texts());
    const auto at = [&](const panel::Box& b) {
        return p.px[size_t(int(b.y + b.h / 2)) * size_t(p.width) + size_t(int(b.x + b.w / 2))];
    };
    const uint32_t lit = at(L.cells[1]), dim = at(L.cells[2]);
    CHECK((lit & 0xFF) > 0xA0 && (dim & 0xFF) < (lit & 0xFF) / 2);  // the icon there, dimmed with none in the bag
    const auto edgeOf = [&](const panel::Picture& pic, const panel::Box& c) {
        return pic.px[size_t(int(c.y + c.h / 2)) * size_t(pic.width) + size_t(int(c.x))];
    };
    const uint32_t hot = edgeOf(p, L.cells[0]), plain = edgeOf(p, L.cells[3]);  // pointed at: a lighter edge
    CHECK(((hot >> 8) & 0xFF) > ((plain >> 8) & 0xFF) + 8);
    const std::vector<hotbar::Words> w = hotbar::words(L, cells, 0, hovering(0), texts());
    const auto has = [&](const std::string& text) {
        return std::find_if(w.begin(), w.end(), [&](const hotbar::Words& x) { return x.text == text; }) != w.end();
    };
    CHECK(has("F1") && has("F9") && has("38") && has("0") && has("10") && has("1") && has("F12"));
    CHECK(has("Double Strafe \xC2\xB7 Lv 10") && has("клік — використати"));
    const std::vector<hotbar::Words> b = hotbar::words(hotbar::layout(hotbar::Mode::Battle, 400, 300, 1.0f),
                                                       std::vector<hotbar::Cell>(27), 0, hovering(-1), texts());
    const auto inB = [&](const std::string& text) {
        return std::find_if(b.begin(), b.end(), [&](const hotbar::Words& x) { return x.text == text; }) != b.end();
    };
    CHECK(inB("бойовий режим") && inB("2") && inB("3") && inB("1"));
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

// 3.0.11-beta.45: the concept's cell states - pressed, chosen (a skill waits for its target), no SP, «не тут»; the
// tip after its while only
TEST(hotbar_cell_states) {
    const hotbar::Layout L = hotbar::layout(hotbar::Mode::Normal, 400, 200, 1.0f);
    panel::Picture icon;
    icon.width = icon.height = 32;
    icon.px.assign(32 * 32, 0xFF3060C0u);
    std::vector<hotbar::Cell> cells(9);
    for (int i = 0; i < 9; ++i) {
        cells[size_t(i)].key = "F" + std::to_string(i + 1);
        cells[size_t(i)].name = "Skill " + std::to_string(i + 1);
        cells[size_t(i)].level = 5;
        cells[size_t(i)].icon = &icon;
    }
    cells[1].selected = true;
    cells[2].spNeed = 24;
    cells[2].spHave = 12;
    cells[3].notHere = hotbar::NotHere::Teleport;
    cells[4].spNeed = 10;
    cells[4].spHave = 30;  // enough
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    hotbar::Show show;
    show.pressed = 0;
    hotbar::paint(p, L, cells, show, texts());
    const auto px = [&](float x, float y) { return p.px[size_t(int(y)) * size_t(p.width) + size_t(int(x))]; };
    const panel::Box& c1 = L.cells[1];
    const uint32_t gold = px(c1.x + 0.5f, c1.y + c1.h / 2);  // chosen: the gold outline
    CHECK(((gold >> 16) & 0xFF) > 0x90 && ((gold >> 8) & 0xFF) > 0x70 && (gold & 0xFF) < ((gold >> 16) & 0xFF));
    const uint32_t full = px(L.cells[5].x + 24, L.cells[5].y + 24), dry = px(L.cells[2].x + 24, L.cells[2].y + 24);
    const uint32_t enough = px(L.cells[4].x + 24, L.cells[4].y + 24);
    CHECK((dry & 0xFF) < (full & 0xFF) && ((dry >> 16) & 0xFF) > ((dry & 0xFF) + 1) / 2);  // dimmed and grey
    CHECK(enough == full);  // SP enough: as any other
    const std::vector<hotbar::Words> w = hotbar::words(L, cells, 0, show, texts());
    const auto find = [&](const std::string& text, size_t from = 0) -> const hotbar::Words* {
        for (size_t i = from; i < w.size(); ++i)
            if (w[i].text == text) return &w[i];
        return nullptr;
    };
    const hotbar::Words* f1 = find("F1");
    const hotbar::Words* f2 = find("F2");
    CHECK(f1 && f2 && std::fabs(f1->box.y - f2->box.y - 1.0f) < 1e-3f);  // pressed: 1 px lower
    int sps = 0;
    for (const hotbar::Words& x : w) sps += x.text == "SP";
    CHECK(sps == 1);  // the «SP» plate on the cell with too little only
    // the tips: why it cannot be used, else how
    const auto tipOf = [&](int cell) {
        hotbar::Show h;
        h.hover = cell;
        const std::vector<hotbar::Words> t = hotbar::words(L, cells, 0, h, texts());
        return t.back().text;
    };
    CHECK(tipOf(2) == "Немає SP \xC2\xB7 потрібно 24, у вас 12");
    CHECK(tipOf(3) == "Не тут \xC2\xB7 на цій карті телепорт заборонено");
    CHECK(tipOf(4) == "клік — використати");
    hotbar::Show early;  // pointed at, its while not over: no tip
    early.hover = 2;
    early.tip = false;
    const std::vector<hotbar::Words> none = hotbar::words(L, cells, 0, early, texts());
    const auto useLine = [](const hotbar::Words& x) { return x.text == "клік — використати"; };
    CHECK(std::none_of(none.begin(), none.end(), useLine));
    CHECK(none.size() + 2 == hotbar::words(L, cells, 0, hovering(2), texts()).size());
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
