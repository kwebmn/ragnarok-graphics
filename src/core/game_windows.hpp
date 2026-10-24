#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/char_status.hpp"
#include "core/config.hpp"

namespace uagfx::gamewindows {

// 3.0.3-beta.6 (the user: switch between the game windows from inside the game, by a key and by a click): the running
// games tell each other of their windows in a block they share (Local\uaro-graphics-windows) - each game its own
// entry, the one of its log slot (1-8, the first started 1): the window's number for the keys and the panel.
constexpr int kMost = 8;
constexpr size_t kName = 32;  // UTF-8 with its end: the server's 23 bytes of a name in any code page fit

struct Entry {
    uint32_t seq;      // odd while its game writes it
    uint32_t pid;      // 0: free
    uint32_t window;   // the game's window (its handle)
    uint32_t number;   // 1-8, its place's
    uint64_t frontAt;  // GetTickCount64 when it was last the window in front; 0 never
    char name[kName];  // the character's, "" none yet (the login, the character select, another build of the exe)
    int16_t baseLevel, jobLevel;  // 0 not known
    int8_t hp, sp;                // percent, -1 not known
    uint8_t spare[2];
};
static_assert(sizeof(Entry) == 64, "the shared block's layout is every game's");
struct Table {
    Entry entries[kMost];
};

// The name into its entry, cut at a whole UTF-8 letter
void putName(Entry& e, const std::string& utf8);
// The character's levels and HP/SP (percent, a little left 1, not none) into its entry; nullptr or a number the game
// has not had: not known
void putStatus(Entry& e, const charstatus::Status* s);

struct Window {  // a live one, as the keys and the panel see it
    uint32_t pid = 0, window = 0;
    int number = 0;
    uint64_t frontAt = 0;
    std::string name;
    int baseLevel = 0, jobLevel = 0, hp = -1, sp = -1;
};
using Alive = std::function<bool(const Entry&)>;  // its game still runs and the window is still its own
// The live windows by number: an entry in its own place (number = place + 1) whose game `alive` says runs
std::vector<Window> windows(const Table& t, const Alive& alive);
// Ctrl+Tab's: the window in front last before this game's (self: its process); none in front ever - the next number
// after this one's; nullptr with no other window
const Window* previous(const std::vector<Window>& all, uint32_t selfPid);
const Window* byNumber(const std::vector<Window>& all, int number);

// Windows' key codes the keys use (core knows no windows.h)
constexpr unsigned kTab = 0x09, kPad0 = 0x60;
enum class Action { None, Back, Window, Panel };
struct KeyAction {
    Action action = Action::None;
    int number = 0;  // Window's
};
// A key going down in the game window: Ctrl+Tab back (with Shift too; not with Alt - Windows' own Ctrl+Alt+Tab),
// Ctrl+Alt+1-8 (and the number pad's) window N, Ctrl+Alt+0 the panel shown or hidden - as [windows] switch_keys has
// it; the game's own (Alt+digit chat macros, Ctrl+digit emotions, Shift with any) left alone
KeyAction keyAction(unsigned vk, bool ctrl, bool alt, bool shift, WindowKeys keys);

}  // namespace uagfx::gamewindows
