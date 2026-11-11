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

// 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO», stage 2): played with the controller - each cell its
// button's badge in the top left instead of its key, a cell no button uses dimmed
TEST(hotbar_pad_badges) {
    const hotbar::Layout L = hotbar::layout(hotbar::Mode::Normal, 400, 200, 1.0f);
    panel::Picture icon;
    icon.width = icon.height = 32;
    icon.px.assign(32 * 32, 0xFF3060C0u);
    std::vector<hotbar::Cell> cells(9);
    for (int i = 0; i < 9; ++i) {
        cells[size_t(i)].key = "F" + std::to_string(i + 1);
        cells[size_t(i)].name = "Skill";
        cells[size_t(i)].icon = &icon;
    }
    cells[0].pad = 2;  // X
    cells[3].pad = 5;  // RB
    hotbar::Show show;
    show.pad = true;
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    hotbar::paint(p, L, cells, show, texts());
    const auto mid = [&](const panel::Box& b) {
        return p.px[size_t(int(b.y + b.h / 2)) * size_t(p.width) + size_t(int(b.x + b.w / 2))];
    };
    CHECK((mid(L.cells[8]) & 0xFF) < (mid(L.cells[0]) & 0xFF) * 3 / 4);  // F9: no button, dimmed
    uint32_t ring = 0;  // X's blue ring over the cell's corner: its badge 18 px from 4 left and 5 above the cell
    for (int y = int(L.cells[0].y) - 5; y < int(L.cells[0].y) - 1; ++y)
        ring = std::max(ring, p.px[size_t(y) * size_t(p.width) + size_t(int(L.cells[0].x) + 5)] & 0xFFu);
    CHECK(ring > 0xB0);
    const std::vector<hotbar::Words> w = hotbar::words(L, cells, 0, show, texts());
    const auto has = [&](const std::string& text) {
        return std::find_if(w.begin(), w.end(), [&](const hotbar::Words& x) { return x.text == text; }) != w.end();
    };
    CHECK(has("X") && has("RB") && !has("F1") && !has("F9"));
    show.sony = true;
    const std::vector<hotbar::Words> ws = hotbar::words(L, cells, 0, show, texts());
    CHECK(std::find_if(ws.begin(), ws.end(), [](const hotbar::Words& x) { return x.text == "R1"; }) != ws.end());
    // 3.0.18-beta.130 (the owner: «кнопки rt lt джойстика как-то не влезли»): a
    // shoulder's badge as wide as its pill (uaUI's picture of it was square: «RB»/«LT» cut at the right); a face
    // button's square
    const hotbar::View v = hotbar::view(L, cells, 0, show, texts());
    CHECK(std::abs(v.cells[3].padBox.w - padglyph::widthOf(padglyph::Button::RB, 18.0f)) < 0.01f);
    CHECK(std::abs(v.cells[0].padBox.w - 18.0f) < 0.01f);
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
    // 3.0.11-beta.78: a skill the character has not learned - greyed as one not allowed here, its own words
    std::vector<hotbar::Cell> missing = cells;
    missing[6].unlearned = true;
    hotbar::Show m6;
    m6.hover = 6;
    CHECK(hotbar::words(L, missing, 0, m6, texts()).back().text == "Персонаж не має цієї навички");
    panel::Picture mp;
    mp.width = L.w;
    mp.height = L.h;
    mp.px.assign(size_t(L.w) * size_t(L.h), 0u);
    hotbar::paint(mp, L, missing, hotbar::Show{}, texts());
    const auto blue = [&](const panel::Box& b) {  // the cell's blue on the whole: the icon's, dimmed or not
        long sum = 0;
        for (int y = int(b.y); y < int(b.y + b.h); ++y)
            for (int x = int(b.x); x < int(b.x + b.w); ++x)
                sum += long(mp.px[size_t(y) * size_t(mp.width) + size_t(x)] & 0xFF);
        return sum;
    };
    CHECK(blue(L.cells[6]) * 10 < blue(L.cells[5]) * 8);
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

// 3.0.11-beta.110 (the concept «Концепция окон и инвентаря», stage 2): a thing or a skill dragged - the cells lit in
// gold (a drop on one gives it the cell)
TEST(hotbar_lights_its_cells_while_a_thing_is_dragged) {
    const hotbar::Layout L = hotbar::layout(hotbar::Mode::Normal, 960, 1070, 1.0f);
    const std::vector<hotbar::Cell> cells(L.cells.size());
    const auto painted = [&](const hotbar::Show& show) {
        panel::Picture p;
        p.width = L.w;
        p.height = L.h;
        p.px.assign(size_t(L.w) * size_t(L.h), 0u);
        hotbar::paint(p, L, cells, show, texts());
        return p;
    };
    hotbar::Show sh;
    const panel::Picture plain = painted(sh);
    sh.dropping = true;
    const panel::Picture lit = painted(sh);
    const auto edge = [](const panel::Picture& p, const panel::Box& c) {
        return p.px[size_t(int(c.y + c.h / 2)) * size_t(p.width) + size_t(int(c.x))];
    };
    CHECK(((edge(lit, L.cells[0]) >> 16) & 0xFF) > ((edge(plain, L.cells[0]) >> 16) & 0xFF) + 20);
    CHECK(((edge(lit, L.cells[5]) >> 16) & 0xFF) > ((edge(plain, L.cells[5]) >> 16) & 0xFF) + 20);
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

// 3.0.11-beta.125 (the owner: «когда играю арчером, есть стрелы, и не видно сколько осталось ... чтобы словить момент
// когда они заканчиваются»): the ammo worn on a pill over the bar's left end - how many, amber under 50, red at 10 or
// fewer; a warning's words with the count for a while as it falls under 50 or 10 or runs out
TEST(hotbar_shows_the_ammo_left) {
    const hotbar::Layout L = hotbar::layout(hotbar::Mode::Normal, 960, 1070, 1.0f);
    const std::vector<hotbar::Cell> cells(L.cells.size());
    hotbar::Show sh;
    CHECK(hotbar::ammoBox(L, sh).w == 0);  // none worn: no pill
    sh.ammo.count = 437;
    const panel::Box b = hotbar::ammoBox(L, sh);
    CHECK(b.w > 0 && b.y >= 0 && b.y + b.h <= L.glass.y && b.x >= L.glass.x);
    const auto colourOf = [&](const std::string& text) -> uint32_t {
        for (const hotbar::Words& w : hotbar::words(L, cells, 0, sh, texts()))
            if (w.text == text) return w.rgb;
        return 0;
    };
    CHECK(colourOf("437") == 0xECEFF5);
    sh.ammo.count = 37;
    CHECK(colourOf("37") == 0xF0A050);  // under 50: amber
    sh.ammo.count = 8;
    CHECK(colourOf("8") == 0xFF5A4E);   // 10 or fewer: red
    sh.ammo.warning = "Стріли закінчуються";
    CHECK(colourOf("Стріли закінчуються \xC2\xB7 8") == 0xFF5A4E);
    CHECK(hotbar::ammoBox(L, sh).w > b.w);  // wider with its words
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    hotbar::paint(p, L, cells, sh, texts());
    const panel::Box w = hotbar::ammoBox(L, sh);
    CHECK((p.px[size_t(w.y + w.h / 2) * size_t(p.width) + size_t(w.x + w.w / 2)] >> 24) > 0x80);  // its ground
}

// 3.0.16-beta.8: the supplies' pills in a row after the ammo's, drawn; the spheres' wider by their dots
TEST(hotbar_supplies_after_the_ammo) {
    const hotbar::Layout L = hotbar::layout(hotbar::Mode::Normal, 800, 900, 1.0f);
    hotbar::Show sh;
    sh.ammo.count = 120;
    hotbar::Supply gem;
    gem.count = 7;
    hotbar::Supply spheres;
    spheres.dots = 5;
    spheres.count = 3;
    sh.supplies = {gem, spheres};
    const panel::Box a = hotbar::ammoBox(L, sh);
    const std::vector<panel::Box> b = hotbar::supplyBoxes(L, sh);
    CHECK(b.size() == 2);
    CHECK(b[0].x >= a.x + a.w && b[1].x >= b[0].x + b[0].w);
    CHECK(b[0].y == a.y);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    std::vector<hotbar::Cell> cells(9);
    hotbar::paint(p, L, cells, sh, texts());
    for (const panel::Box& w : b)
        CHECK((p.px[size_t(w.y + w.h / 2) * size_t(p.width) + size_t(w.x + w.w - 6)] >> 24) > 0x80);
    sh.ammo.count = -1;  // none worn: from the glass's left end
    CHECK(hotbar::supplyBoxes(L, sh)[0].x == hotbar::layout(hotbar::Mode::Normal, 800, 900, 1.0f).glass.x);
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

// 3.0.11-beta.143 (the owner's concept «маг, прист и хант на геймпаде»): a ground skill's own count in its cell's
// bottom right - Fire Wall's walls standing «2/3», a trap's Traps in the bag «×37» (red when none) - and its edge
// flashing amber when a wall burnt out (re-place it)
TEST(hotbar_ground_skill_counts_and_flash) {
    const hotbar::Layout L = hotbar::layout(hotbar::Mode::Normal, 400, 200, 1.0f);
    std::vector<hotbar::Cell> cells(9);
    cells[0].name = "Fire Wall";
    cells[0].level = 10;
    cells[0].standing = "2/3";
    cells[1].name = "Ankle Snare";
    cells[1].level = 5;
    cells[1].standing = "\xC3\x97" "37";
    cells[2].name = "Land Mine";
    cells[2].standing = "\xC3\x97" "0";
    cells[2].standingOut = true;
    const std::vector<hotbar::Words> w = hotbar::words(L, cells, 0, hotbar::Show{}, texts());
    const auto find = [&](const std::string& text) {
        return std::find_if(w.begin(), w.end(), [&](const hotbar::Words& x) { return x.text == text; });
    };
    CHECK(find("2/3") != w.end() && find("\xC3\x97" "37") != w.end());
    if (find("2/3") != w.end()) {  // the bottom right
        const panel::Box& c = L.cells[0];
        const panel::Box& at = find("2/3")->box;
        CHECK(std::fabs(at.x + at.w - (c.x + c.w - 2)) < 0.5f && at.y > c.y + c.h / 2);
    }
    CHECK(find("\xC3\x97" "0") != w.end() && find("\xC3\x97" "0")->rgb == 0xFF5A4E);
    panel::Picture plain, flashed;
    for (panel::Picture* p : {&plain, &flashed}) {
        p->width = L.w;
        p->height = L.h;
        p->px.assign(size_t(L.w) * size_t(L.h), 0u);
    }
    hotbar::paint(plain, L, cells, hotbar::Show{}, texts());
    cells[0].flash = 1.0f;
    hotbar::paint(flashed, L, cells, hotbar::Show{}, texts());
    const panel::Box& c = L.cells[0];
    const uint32_t before = plain.px[size_t(int(c.y + c.h / 2)) * size_t(L.w) + size_t(int(c.x))];
    const uint32_t after = flashed.px[size_t(int(c.y + c.h / 2)) * size_t(L.w) + size_t(int(c.x))];
    CHECK(((after >> 16) & 0xFF) > ((before >> 16) & 0xFF) + 40);  // its edge amber
}

// 3.0.11-beta.147 (the owner: «предусмотри возможность чтобы можно было батлмод кнопкой на этой панели включать»): the
// pencil gone - in its place the battle mode's button, two crossed swords: red while the mode is on, its tip what a
// click does
TEST(hotbar_battle_button) {
    const hotbar::Layout N = hotbar::layout(hotbar::Mode::Normal, 400, 200, 1.0f);
    const hotbar::Layout B = hotbar::layout(hotbar::Mode::Battle, 400, 300, 1.0f);
    for (const hotbar::Layout* L : {&N, &B}) {
        CHECK(L->toggle.w > 0 && inside(L->glass, L->toggle) && L->toggle.x >= L->cells[8].x + L->cells[8].w);
        const hotbar::Hit h = hotbar::hitTest(*L, L->toggle.x + L->toggle.w / 2, L->toggle.y + L->toggle.h / 2);
        CHECK(h.part == hotbar::Part::Toggle);
    }
    const auto backing = [](const hotbar::Layout& L) {  // left of its swords, inside its rounding
        panel::Picture p;
        p.width = L.w;
        p.height = L.h;
        p.px.assign(size_t(L.w) * size_t(L.h), 0u);
        hotbar::paint(p, L, std::vector<hotbar::Cell>(L.cells.size()), hotbar::Show{}, texts());
        return p.px[size_t(int(L.toggle.y + L.toggle.h / 2)) * size_t(p.width) + size_t(int(L.toggle.x + 3))];
    };
    const uint32_t off = backing(N), on = backing(B);
    CHECK(((on >> 16) & 0xFF) > ((on >> 8) & 0xFF) + 40);    // red while on
    CHECK(((off >> 16) & 0xFF) < ((off >> 8) & 0xFF) + 20);  // not while off
    hotbar::Show sh;
    sh.hoverToggle = true;
    const auto has = [](const std::vector<hotbar::Words>& w, const std::string& text) {
        return std::find_if(w.begin(), w.end(), [&](const hotbar::Words& x) { return x.text == text; }) != w.end();
    };
    CHECK(has(hotbar::words(N, std::vector<hotbar::Cell>(9), 0, sh, texts()), "увімкнути бойовий режим"));
    CHECK(has(hotbar::words(B, std::vector<hotbar::Cell>(27), 0, sh, texts()), "вимкнути бойовий режим"));
    CHECK(!has(hotbar::words(N, std::vector<hotbar::Cell>(9), 0, hotbar::Show{}, texts()), "увімкнути бойовий режим"));
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

// 3.0.11-beta.147: while an icon is lifted its cell shows it faded and the cell under it is outlined in gold
TEST(hotbar_lifted_cell_and_its_place) {
    const hotbar::Layout L = hotbar::layout(hotbar::Mode::Normal, 400, 200, 1.0f);
    panel::Picture icon;
    icon.width = icon.height = 32;
    icon.px.assign(32 * 32, 0xFF3060C0u);
    std::vector<hotbar::Cell> cells(9);
    for (hotbar::Cell& c : cells) {
        c.name = "Fire Bolt";
        c.icon = &icon;
    }
    hotbar::Show show;
    show.lifted = 1;
    show.over = 4;
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    hotbar::paint(p, L, cells, show, texts());
    const auto mid = [&](const panel::Box& b) {
        return p.px[size_t(int(b.y + b.h / 2)) * size_t(p.width) + size_t(int(b.x + b.w / 2))];
    };
    CHECK((mid(L.cells[1]) & 0xFF) < (mid(L.cells[2]) & 0xFF) / 2);  // its icon faded where it was
    const auto edge = [&](const panel::Box& b) {
        return p.px[size_t(int(b.y + b.h / 2)) * size_t(p.width) + size_t(int(b.x))];
    };
    const uint32_t gold = edge(L.cells[4]), plain = edge(L.cells[3]);
    CHECK(((gold >> 16) & 0xFF) > ((plain >> 16) & 0xFF) + 60 && ((gold >> 16) & 0xFF) > (gold & 0xFF) + 40);
}

// 3.0.12-beta.2 (a player's wish: «вікно запасу стріл перемістити вниз від скілів»; the owner: «если плашка навыков
// вверху то стрелы надо вниз, а если внизу то вверху стрелы»): flipped, the glass at the picture's top, the tips and
// the ammo's pill under it, the picture's size the same
TEST(hotbar_flipped_puts_the_ammo_and_tips_under_the_bar) {
    hotbar::Layout L = hotbar::layout(hotbar::Mode::Normal, 960, 300, 1.0f);
    const hotbar::Layout before = L;
    hotbar::flip(L);
    CHECK(L.below && L.glass.y == 0 && L.w == before.w && L.h == before.h);
    CHECK(L.cells[0].y == before.cells[0].y - before.glass.y && L.toggle.y < L.glass.y + L.glass.h);
    CHECK(L.tipRoom.y >= L.glass.y + L.glass.h && L.tipRoom.y + L.tipRoom.h <= float(L.h) + 0.5f);
    hotbar::Show sh;
    sh.ammo.count = 300;
    const panel::Box b = hotbar::ammoBox(L, sh);
    CHECK(b.w > 0 && b.y >= L.glass.y + L.glass.h && b.y + b.h <= float(L.h) + 0.5f);
    hotbar::flip(L);  // twice: no further
    CHECK(L.glass.y == 0 && L.below);
}

// 3.0.18-beta.21 (the RML bar): the view says what the words say, where they say it
TEST(hotbar_view_places_the_words_parts) {
    using namespace uagfx::hotbar;
    const Layout L = layout(Mode::Normal, 600, 700, 1.0f);
    std::vector<Cell> cells(9);
    cells[0].key = "F1";
    cells[0].name = "Heal";
    cells[0].level = 10;
    cells[1].key = "F2";
    cells[1].name = "Red Potion";
    cells[1].count = 7;
    cells[2].key = "F3";
    cells[2].name = "Fire Wall";
    cells[2].level = 10;
    cells[2].spNeed = 40;
    cells[2].spHave = 10;
    Show show;
    Texts t;
    const View v = view(L, cells, 0, show, t);
    const std::vector<Words> w = words(L, cells, 0, show, t);
    const auto has = [&](const Plate& p) {
        for (const Words& x : w)
            if (x.text == p.text && std::fabs(x.box.x - p.box.x) < 0.01f && std::fabs(x.box.y - p.box.y) < 0.01f)
                return true;
        return false;
    };
    CHECK_EQ(v.cells.size(), size_t(9));
    CHECK(has(v.cells[0].key) && has(v.cells[0].level) && has(v.cells[1].count) && has(v.cells[2].sp));
    CHECK(v.cells[2].dry && v.cells[2].grey && v.cells[2].iconOpacity == 0.5f);
    CHECK(has(v.row) && has(v.f12));
    CHECK(v.cells[8].empty);
}
