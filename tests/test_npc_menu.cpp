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
