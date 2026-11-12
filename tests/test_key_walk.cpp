// 3.0.18-beta.207 (the keyboard concept https://claude.ai/artifact/SxvWxL5BM2tHtSWAvYPh9J, stage 1; the testers:
// «кнопками с клавиатуры верх вниз влево вправо управлять»): the arrows walk while the chat line is empty - the held
// arrows as the pad's stick, the rule for when an arrow walks
#include <cmath>

#include "core/key_walk.hpp"
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
