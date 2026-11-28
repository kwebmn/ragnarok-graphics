#pragma once
#include <cstdint>

namespace uagfx::keychars {

// 3.0.24-beta.46 (the audit 28.11: a digit chosen in an NPC's menu was typed into the chat line too, and the next
// Enter said it - «TestHD : 2»): Windows makes a press's letter (WM_CHAR) before anyone sees the press, so a press
// our windows took still left its letter for the game's chat field. The game gets a letter only when it also got the
// press that made it - the same scan code (and extended bit), at most kWithinMs after the press.
constexpr uint64_t kWithinMs = 200;
struct Press {
    unsigned key = 0;  // the scan code and the extended bit (lParam bits 16-24); 0 none seen
    uint64_t atMs = 0;
    bool toGame = true;
};
Press pressed(unsigned long long lParam, uint64_t nowMs, bool toGame);
// whether the letter with this lParam is the letter of a press the game did not get
bool keepLetter(const Press& last, unsigned long long lParam, uint64_t nowMs);

}  // namespace uagfx::keychars
