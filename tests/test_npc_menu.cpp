// 3.0.24-beta.27 (docs/superpowers/plans/2026-11-27-other-windows-old-layout.md, task 4; the spec's «Разговор с
// NPC»: «выбор — отдельный список, который ведут мышь (наведение, колесо, клик), клавиатура (↑ ↓, 1–5, Enter, Esc) и
// геймпад»): the choice's own list beside the talk - right of it 8 apart, else left, else under it, in the screen
#include "core/npc_menu.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(npc_menu_place_right) {
    const panel::Box talk{300, 200, 420, 260};
    const panel::Box m = npcmenu::place(talk, 240, 180, 1280, 720);
    CHECK(m.x == 728 && m.y == 200 && m.w == 240 && m.h == 180);
}

TEST(npc_menu_place_left_or_below) {
    const panel::Box atRight{820, 200, 420, 260};  // no room right of it
    const panel::Box left = npcmenu::place(atRight, 240, 180, 1280, 720);
    CHECK(left.x == 572 && left.y == 200);
    const panel::Box wide{100, 100, 700, 260};  // a narrow screen: no room either side
    const panel::Box below = npcmenu::place(wide, 240, 180, 900, 720);
    CHECK(below.y == 368 && below.x >= 0 && below.x + below.w <= 900);
    const panel::Box low{100, 500, 700, 200};  // under it the screen ends: kept inside
    const panel::Box in = npcmenu::place(low, 240, 180, 900, 720);
    CHECK(in.y + in.h <= 720 && in.y >= 0);
}

TEST(npc_menu_keys) {
    CHECK_EQ(npcmenu::byKey('1', 5), 0);
    CHECK_EQ(npcmenu::byKey('5', 5), 4);
    CHECK_EQ(npcmenu::byKey('0', 5), -1);
    CHECK_EQ(npcmenu::byKey('7', 5), -1);
    CHECK_EQ(npcmenu::byKey(0x61 /* VK_NUMPAD1 */, 3), 0);
    CHECK_EQ(npcmenu::byKey('A', 5), -1);
}

// 3.0.24-beta.51 (the audit 28.11, N4: the Kafra's picture over the menu at the talk's right): the side of the NPC's
// picture (the server's cut-in: 0 bottom left, 2 bottom right) kept free - the menu on the other side when there is
// room there; the picture's side the last choice
TEST(npc_menu_place_keeps_off_the_pictures_side) {
    const panel::Box talk{430, 300, 420, 260};
    const panel::Box right = npcmenu::place(talk, 240, 180, 1280, 720, npcmenu::sideOf(2));
    CHECK(right.x + right.w <= talk.x);  // the picture at the bottom right: the menu at the left
    const panel::Box left = npcmenu::place(talk, 240, 180, 1280, 720, npcmenu::sideOf(0));
    CHECK(left.x >= talk.x + talk.w);  // at the bottom left: at the right as before
    const panel::Box none = npcmenu::place(talk, 240, 180, 1280, 720, npcmenu::sideOf(255));
    CHECK(none.x >= talk.x + talk.w);  // closed: as before
    CHECK(npcmenu::sideOf(1) == npcmenu::Side::None && npcmenu::sideOf(3) == npcmenu::Side::None);
    const panel::Box atLeft{20, 300, 420, 260};  // no room at the left: the right after all
    CHECK(npcmenu::place(atLeft, 240, 180, 1280, 720, npcmenu::Side::Right).x >= atLeft.x + atLeft.w);
}
