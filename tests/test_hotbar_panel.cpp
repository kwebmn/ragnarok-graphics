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
    return hotbar::Texts{"бойовий режим", "літери — навички й предмети \xC2\xB7 Enter — чат", "редагувати панель гри",
                         "готово", "клік — використати"};
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
    hotbar::paint(p, L, cells, 0, false, texts());
    const auto at = [&](const panel::Box& b) {
        return p.px[size_t(int(b.y + b.h / 2)) * size_t(p.width) + size_t(int(b.x + b.w / 2))];
    };
    const uint32_t lit = at(L.cells[1]), dim = at(L.cells[2]);
    CHECK((lit & 0xFF) > 0xA0 && (dim & 0xFF) < (lit & 0xFF) / 2);  // the icon there, dimmed with none in the bag
    const panel::Box& c0 = L.cells[0];
    const uint32_t edge = p.px[size_t(int(c0.y + c0.h / 2)) * size_t(p.width) + size_t(int(c0.x))];  // pointed at
    CHECK(((edge >> 16) & 0xFF) > 0x90 && ((edge >> 8) & 0xFF) > 0x70);
    const std::vector<hotbar::Words> w = hotbar::words(L, cells, 0, 0, false, texts());
    const auto has = [&](const std::string& text) {
        return std::find_if(w.begin(), w.end(), [&](const hotbar::Words& x) { return x.text == text; }) != w.end();
    };
    CHECK(has("F1") && has("F9") && has("38") && has("0") && has("10") && has("1") && has("F12"));
    CHECK(has("Double Strafe \xC2\xB7 Lv 10") && has("клік — використати"));
    const std::vector<hotbar::Words> b = hotbar::words(hotbar::layout(hotbar::Mode::Battle, 400, 300, 1.0f),
                                                       std::vector<hotbar::Cell>(27), 0, -1, false, texts());
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
