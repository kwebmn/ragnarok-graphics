#include "core/key_chars.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
// a key's lParam as Windows gives it: the scan code in bits 16-23, the extended key's bit 24
unsigned long long lParamOf(unsigned scan, bool extended = false) {
    return 1ull | (static_cast<unsigned long long>(scan) << 16) | (extended ? (1ull << 24) : 0ull);
}
}  // namespace

// 3.0.24-beta.46 (the audit 28.11: a digit chosen in an NPC's menu was typed into the chat line too and said by
// the next Enter - «TestHD : 2»): Windows makes a press's letter before anyone sees the press; the game's chat field
// gets a letter only when the game got the press that made it
TEST(key_chars_a_press_ours_keeps_its_letter_from_the_game) {
    const keychars::Press ours = keychars::pressed(lParamOf(0x03), 1000, false);  // «2», taken by the NPC menu
    CHECK(keychars::keepLetter(ours, lParamOf(0x03), 1001));
    const keychars::Press games = keychars::pressed(lParamOf(0x03), 1000, true);  // went on to the game
    CHECK(!keychars::keepLetter(games, lParamOf(0x03), 1001));
}

TEST(key_chars_only_that_press_letter_and_only_at_once) {
    const keychars::Press ours = keychars::pressed(lParamOf(0x03), 1000, false);
    CHECK(!keychars::keepLetter(ours, lParamOf(0x04), 1001));             // another key's letter
    CHECK(!keychars::keepLetter(ours, lParamOf(0x03), 1000 + 1000));      // a second later: not that press's
    CHECK(!keychars::keepLetter(ours, 0, 1001));                          // a letter with no key (typed by a tool)
    CHECK(!keychars::keepLetter(keychars::Press{}, lParamOf(0x03), 1001));  // no press seen yet
    CHECK(!keychars::keepLetter(ours, lParamOf(0x03, true), 1001));       // the same scan code, an extended key
}
