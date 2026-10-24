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
// games tell each other of their windows in a block they share - each game its own entry, the one of its log slot
// (1-8, the first started 1): the window's number for the keys and the panel. 3.0.3-beta.9: a layout of its own
// (Local\uaro-graphics-windows-2): a game asked to get its picture whole before it is shown (the summon), and its
// answer; a 23-letter name in two-byte UTF-8 whole.
constexpr int kMost = 8;
constexpr size_t kName = 48;  // UTF-8 with its end: the server's 23 bytes of a name, two-byte letters too

struct Entry {
    uint32_t seq;      // odd while its game writes it
    uint32_t pid;      // 0: free
    uint32_t window;   // the game's window (its handle)
    uint32_t number;   // 1-8, its place's
    uint64_t frontAt;  // GetTickCount64 when it was last the window in front; 0 never
    char name[kName];  // the character's, "" none yet (the login, the character select, another build of the exe)
    int16_t baseLevel, jobLevel;  // 0 not known
    int8_t hp, sp;                // percent, -1 not known
    uint8_t flags;                // 3.0.3-beta.9: kDrawsBehind
    uint8_t spare;
    uint32_t ready;  // 3.0.3-beta.9: the summon it answered - its whole picture drawn behind the window in front
    // 3.0.4-beta.4: when (stamp) its character fell, was hit, was whispered to; 0 never - 3.0.3's spare bytes
    uint32_t diedAt, hitAt, whisperAt;
};
static_assert(sizeof(Entry) == 96, "the shared block's layout is every game's");
// The game draws while its window is not active - in a window; on its full screen uaRO.exe draws nothing then
// (0x6ebfec, 0x6e4bdc): a summon would only wait
constexpr uint8_t kDrawsBehind = 1;
struct Table {
    int32_t summonPid;     // 3.0.3-beta.9: the game asked to get its picture whole (written by the window in front)
    int32_t summonSerial;  // and the ask's number
    uint8_t spare[56];
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
    uint32_t ready = 0;
    bool drawsBehind = false;
    uint32_t diedAt = 0, hitAt = 0, whisperAt = 0;  // 3.0.4-beta.4
};
using Alive = std::function<bool(const Entry&)>;  // its game still runs and the window is still its own
// The live windows by number: an entry in its own place (number = place + 1) whose game `alive` says runs
std::vector<Window> windows(const Table& t, const Alive& alive);
// Ctrl+Tab's: the window in front last before this game's (self: its process); none in front ever - the next number
// after this one's; nullptr with no other window
const Window* previous(const std::vector<Window>& all, uint32_t selfPid);

// 3.0.4-beta.4 (the user's idea 8): what a window's card shows of its character - fallen (until it lives again or the
// player looks at the window), whispered to (until looked at, kWhisperMs at most), hit (kHitMs). Times: GetTickCount64
// cut to 32 bits by stamp() (0 is never), compared across the cut's end.
struct Marks {
    bool died = false, whisper = false, hit = false;
};
constexpr uint32_t kHitMs = 4000, kWhisperMs = 300000;
inline uint32_t stamp(uint64_t tick) { return uint32_t(tick) | 1u; }
Marks marks(const Window& w, uint32_t now);
const Window* byNumber(const std::vector<Window>& all, int number);

// 3.0.3-beta.9 (the user: switching «более гладко», no blink): a switch by the keys or the panel asks the next window
// first to get its picture whole - its own resolution, DLSS, effects - behind the one in front, and shows it then,
// or once kSummonWaitMs passed; asked, a game draws whole for kSummonKeepMs at most.
constexpr uint64_t kSummonWaitMs = 400, kSummonKeepMs = 1000;
// A game's wake: an event its frame limiter waits on with its timer - the one asked draws at once, not up to a
// background frame (0.2-0.5 s) later
std::string wakeName(uint32_t pid);
// The block's ask is for this game (self) and new (seen: the last taken, set to it)
bool summoned(int32_t pid, int32_t serial, uint32_t self, int32_t& seen);
// The window asked answered (ready: its entry's; serial: the ask's), or it has had its time
bool switchNow(uint32_t ready, uint32_t serial, uint64_t since, uint64_t now);

// Where the focus went from a game on its full screen (uaRO.exe minimises itself then, WM_ACTIVATE at 0x6541e0):
// another game window, or this one again - it stays (no blink); Windows' own switcher still open, or nothing in
// front yet - wait; anything else (the desktop, a program) - minimised, as the game itself does
enum class Leave { Wait, Stay, Minimise };
Leave leaving(uint32_t foregroundPid, const std::string& foregroundClass, const std::vector<Window>& games,
              uint32_t selfPid);

// A window's place on the screen; one wholly over another (a game window behind another one is not seen)
struct Box {
    int left = 0, top = 0, right = 0, bottom = 0;
};
bool covers(const Box& outer, const Box& inner);

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
