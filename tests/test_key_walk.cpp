// 3.0.18-beta.207 (the keyboard concept https://claude.ai/artifact/SxvWxL5BM2tHtSWAvYPh9J, stage 1; the testers:
// «кнопками с клавиатуры верх вниз влево вправо управлять»): the arrows walk while the chat line is empty - the held
// arrows as the pad's stick, the rule for when an arrow walks
#include <cmath>
#include <initializer_list>

#include "core/key_walk.hpp"
#include "pad/core/logic/pad.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }

}  // namespace

TEST(key_walk_vectors) {
    using namespace keywalk;
    const Vec up = vectorOf(kUp);
    CHECK(near(up.x, 0) && near(up.y, 1));  // up: the stick pushed up
    const Vec right = vectorOf(kRight);
    CHECK(near(right.x, 1) && near(right.y, 0));
    const Vec upRight = vectorOf(kUp | kRight);  // two: a diagonal, as long as one
    CHECK(near(upRight.x, 0.7071f) && near(upRight.y, 0.7071f));
    const Vec downLeft = vectorOf(kDown | kLeft);
    CHECK(near(downLeft.x, -0.7071f) && near(downLeft.y, -0.7071f));
    const Vec both = vectorOf(kUp | kDown);  // opposite ones cancel
    CHECK(near(both.x, 0) && near(both.y, 0));
    const Vec three = vectorOf(kUp | kDown | kLeft);
    CHECK(near(three.x, -1) && near(three.y, 0));
    const Vec none = vectorOf(0);
    CHECK(near(none.x, 0) && near(none.y, 0));
}

TEST(key_walk_dirs_of_keys) {
    using namespace keywalk;
    CHECK_EQ(int(dirOfVk(0x26)), int(kUp));  // VK_UP
    CHECK_EQ(int(dirOfVk(0x28)), int(kDown));
    CHECK_EQ(int(dirOfVk(0x25)), int(kLeft));
    CHECK_EQ(int(dirOfVk(0x27)), int(kRight));
    CHECK_EQ(int(dirOfVk(0x41)), 0);  // 'A': not an arrow
    CHECK_EQ(int(dirOfVk(0x0D)), 0);
}

TEST(key_walk_gate) {
    using namespace keywalk;
    Gate g;
    g.enabled = g.inGame = g.chatEmpty = true;
    g.modifier = false;
    CHECK(arrowWalks(g));
    Gate off = g;
    off.enabled = false;  // «Ходьба стрілками» off
    CHECK(!arrowWalks(off));
    Gate login = g;
    login.inGame = false;  // the login, the character select: their own arrows
    CHECK(!arrowWalks(login));
    Gate typed = g;
    typed.chatEmpty = false;  // words in the chat line: the arrows move its caret
    CHECK(!arrowWalks(typed));
    Gate ctrl = g;
    ctrl.modifier = true;  // Ctrl, Alt or Shift held: not a walk
    CHECK(!arrowWalks(ctrl));
    Gate field = g;
    field.otherField = true;  // the final review: another of the game's fields typed in (a count, a name) - its caret
    CHECK(!arrowWalks(field));
}

// 3.0.18-beta.214 (R-CX44S5 «кнопка з awakening potion зависла»): a key's pad button in the driver's windows - Space
// none (it was X: «Речі» took the potion up to the bar, and the keys could not put it)
TEST(key_walk_window_buttons) {
    using namespace keywalk;
    CHECK_EQ(int(windowButtonOf(0x26, false)), int(uapad::kPadUp));
    CHECK_EQ(int(windowButtonOf(0x27, false)), int(uapad::kPadRight));
    CHECK_EQ(int(windowButtonOf(0x0D, false)), int(uapad::kPadA));   // Enter
    CHECK_EQ(int(windowButtonOf(0x1B, false)), int(uapad::kPadB));   // Esc
    CHECK_EQ(int(windowButtonOf(0x21, false)), int(uapad::kPadLB));  // PgUp
    CHECK_EQ(int(windowButtonOf(0x22, false)), int(uapad::kPadRB));  // PgDn
    CHECK_EQ(int(windowButtonOf(0x5D, false)), int(uapad::kPadY));   // the menu key
    CHECK_EQ(int(windowButtonOf(0x79, true)), int(uapad::kPadY));    // Shift+F10
    CHECK_EQ(int(windowButtonOf(0x79, false)), 0);                   // F10 alone: the game's
    CHECK_EQ(int(windowButtonOf(0x20, false)), 0);                   // Space: no window button
    CHECK_EQ(int(windowButtonOf(0x41, false)), 0);
}

// 3.0.18-beta.215 (R-8KPDY9 «клавиатура тянула все время в право»): an arrow's let go taken by a hook earlier in the
// chain (the walk mode switched on with the arrow held) left the walk on for 30 s - the keys held are checked against
// Windows' own state of the keys each frame
namespace {
unsigned g_down[4];
int g_downs = 0;
bool fakeDown(unsigned vk) {
    for (int i = 0; i < g_downs; ++i)
        if (g_down[i] == vk) return true;
    return false;
}
void downOnly(std::initializer_list<unsigned> vks) {
    g_downs = 0;
    for (unsigned vk : vks) g_down[g_downs++] = vk;
}
}  // namespace

TEST(key_walk_held_checked_against_windows) {
    using namespace keywalk;
    downOnly({0x26});  // only the arrow up is down
    CHECK_EQ(int(heldNow(uint8_t(kUp | kRight), fakeDown)), int(kUp));  // the right's let go was not seen
    downOnly({});
    CHECK_EQ(int(heldNow(uint8_t(kUp | kRight), fakeDown)), 0);
    downOnly({0x25, 0x28});
    CHECK_EQ(int(heldNow(uint8_t(kLeft | kDown), fakeDown)), int(kLeft | kDown));
    CHECK_EQ(int(heldNow(0, fakeDown)), 0);
}

TEST(key_walk_window_buttons_checked_against_windows) {
    using namespace keywalk;
    downOnly({0x0D});  // Enter down, the right arrow up
    CHECK_EQ(int(windowButtonsNow(uint16_t(uapad::kPadRight | uapad::kPadA), fakeDown)), int(uapad::kPadA));
    downOnly({0x79});  // F10 (Shift+F10 pressed it): Y held
    CHECK_EQ(int(windowButtonsNow(uapad::kPadY, fakeDown)), int(uapad::kPadY));
    downOnly({0x5D});  // the menu key
    CHECK_EQ(int(windowButtonsNow(uapad::kPadY, fakeDown)), int(uapad::kPadY));
    downOnly({});
    CHECK_EQ(int(windowButtonsNow(uint16_t(uapad::kPadY | uapad::kPadLB | uapad::kPadB), fakeDown)), 0);
}
