// 3.0.18-beta.208 (the keyboard concept, stage 2; the owner: «а что если мы специальный режим сделаем для ходьбы на
// клавиатуре?»): the walk mode - ` (Ё) switches it, the letters stop typing, W A S D and the numpad walk, Enter opens
// the chat and sends, Esc closes it
#include "core/key_walk.hpp"
#include "core/walk_mode.hpp"
#include "test.hpp"

using namespace uagfx;
using walkmode::Act;
using walkmode::Key;

namespace {

Key down(unsigned vk, unsigned scan, bool chatEmpty = true) {
    Key k;
    k.vk = vk;
    k.scan = scan;
    k.down = true;
    k.chatEmpty = chatEmpty;
    return k;
}

Key up(unsigned vk, unsigned scan) {
    Key k = down(vk, scan);
    k.down = false;
    return k;
}

constexpr unsigned kTilde = 0xC0, kTildeScan = 0x29;

}  // namespace

TEST(walk_mode_toggle) {
    walkmode::Mode m;
    CHECK(!m.on());
    CHECK(m.key(down(kTilde, kTildeScan, false)) == Act::Pass);  // words in the chat line: ` types there
    CHECK(!m.on());
    Key ctrl = down(kTilde, kTildeScan);
    ctrl.ctrl = true;  // Ctrl+`: the effects console
    CHECK(m.key(ctrl) == Act::Pass);
    CHECK(m.key(down(kTilde, kTildeScan)) == Act::Toggle);
    CHECK(m.on() && !m.typing());
    CHECK(m.key(down(kTilde, kTildeScan, false)) == Act::Toggle);  // on: off whatever the line holds
    CHECK(!m.on());
}

TEST(walk_mode_keys_while_on) {
    walkmode::Mode m;
    m.set(true);
    CHECK(m.key(down('Q', 0x10)) == Act::Swallow);  // a letter: not into the chat
    CHECK(m.key(down('7', 0x08)) == Act::Swallow);
    CHECK(m.key(down(' ', 0x39)) == Act::Swallow);
    Key altE = down('E', 0x12);
    altE.alt = true;  // Alt+E: the game's window
    CHECK(m.key(altE) == Act::Pass);
    CHECK(m.key(down(0x70, 0x3B)) == Act::Pass);  // F1: the game's slot
    CHECK(m.key(down(0x1B, 0x01)) == Act::Pass);  // Esc: the game's (windows, the menu)
    CHECK(m.key(down(0x2D, 0x52, true)) == Act::Pass);  // Insert: sit, the game's
}

TEST(walk_mode_wasd_and_the_numpad) {
    using namespace keywalk;
    walkmode::Mode m;
    m.set(true);
    CHECK(m.key(down('W', 0x11)) == Act::Swallow);
    CHECK_EQ(int(m.dirs()), int(kUp));
    CHECK(m.key(down('D', 0x20)) == Act::Swallow);
    CHECK_EQ(int(m.dirs()), int(kUp | kRight));
    CHECK(m.key(up('W', 0x11)) == Act::Swallow);
    CHECK_EQ(int(m.dirs()), int(kRight));
    CHECK(m.key(up('D', 0x20)) == Act::Swallow);
    CHECK_EQ(int(m.dirs()), 0);
    CHECK(m.key(down(0x26, 0x48)) == Act::Swallow);  // the arrow up (the numpad's 8 has the same scan code)
    CHECK_EQ(int(m.dirs()), int(kUp));
    CHECK(m.key(up(0x26, 0x48)) == Act::Swallow);
    CHECK(m.key(down(0x69, 0x49)) == Act::Swallow);  // the numpad's 9: up and right at once
    CHECK_EQ(int(m.dirs()), int(kUp | kRight));
    CHECK(m.key(down(0x65, 0x4C)) == Act::Swallow);  // its 5: stop
    CHECK_EQ(int(m.dirs()), 0);
    Key home = down(0x24, 0x47);
    home.extended = true;  // Home's own key (the numpad's 7 without the extended bit): the game's
    CHECK(m.key(home) == Act::Pass);
    CHECK_EQ(int(m.dirs()), 0);
}

TEST(walk_mode_chat) {
    walkmode::Mode m;
    m.set(true);
    m.key(down('W', 0x11));
    CHECK(m.key(down(0x0D, 0x1C)) == Act::OpenChat);  // Enter: the chat line - the walk stops
    CHECK(m.typing());
    CHECK_EQ(int(m.dirs()), 0);
    CHECK(m.key(down('H', 0x23)) == Act::Pass);  // typing
    CHECK(m.key(down(kTilde, kTildeScan, false)) == Act::Pass);  // ` types too
    CHECK(m.key(down(0x0D, 0x1C, false)) == Act::Send);  // Enter: the game sends it, back to walking
    CHECK(!m.typing() && m.on());
    CHECK(m.key(down(0x0D, 0x1C)) == Act::OpenChat);
    CHECK(m.key(down(0x1B, 0x01, false)) == Act::CancelChat);  // Esc: the line cleared, back to walking
    CHECK(!m.typing() && m.on());
}

TEST(walk_mode_set_and_lost) {
    walkmode::Mode m;
    m.set(true);
    m.key(down('A', 0x1E));
    m.lost();  // the window lost the keys
    CHECK_EQ(int(m.dirs()), 0);
    m.key(down(0x0D, 0x1C));
    m.set(false);
    CHECK(!m.on() && !m.typing());
    CHECK(m.key(down('A', 0x1E)) == Act::Pass);  // off: letters type
}
