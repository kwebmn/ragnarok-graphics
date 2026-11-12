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
    CHECK(m.key(down('P', 0x19)) == Act::Swallow);  // a letter: not into the chat
    CHECK(m.key(down('0', 0x0B)) == Act::Swallow);
    CHECK(m.key(down(0xBA, 0x27)) == Act::Swallow);  // a sign (; ж)
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

// 3.0.18-beta.209 (stage 3): the walk mode's letters for the quick actions - one key, one action
TEST(walk_mode_actions) {
    using walkmode::Do;
    walkmode::Mode m;
    m.set(true);
    const auto act = [&](unsigned vk, unsigned scan, bool shift = false) {
        Key k = down(vk, scan);
        k.shift = shift;
        return m.key(k);
    };
    CHECK(act(' ', 0x39) == Act::Action && m.action() == Do::Attack);
    CHECK(act('F', 0x21) == Act::Action && m.action() == Do::Pickup);
    CHECK(act('R', 0x13) == Act::Action && m.action() == Do::Talk);
    CHECK(act('C', 0x2E) == Act::Action && m.action() == Do::Sit);
    CHECK(act(0x09, 0x0F) == Act::Action && m.action() == Do::NextTarget);
    CHECK(act(0x09, 0x0F, true) == Act::Action && m.action() == Do::PrevTarget);
    CHECK(act('Q', 0x10) == Act::Action && m.action() == Do::CameraLeft);
    CHECK(act('E', 0x12) == Act::Action && m.action() == Do::CameraRight);
    CHECK(act('1', 0x02) == Act::Action && m.action() == Do::Slot && m.slot() == 0);
    CHECK(act('9', 0x0A) == Act::Action && m.action() == Do::Slot && m.slot() == 8);
    CHECK(act('0', 0x0B) == Act::Swallow);  // no action: just not typed
    CHECK(m.key(up(' ', 0x39)) == Act::Swallow);  // let go: nothing more
    m.key(down(0x0D, 0x1C));  // typing: the same keys type
    CHECK(act('F', 0x21) == Act::Pass);
    CHECK(act(0x09, 0x0F) == Act::Pass);  // Tab: the chat's channel while typing
}
