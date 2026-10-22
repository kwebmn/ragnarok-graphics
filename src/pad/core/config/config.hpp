#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "pad/core/i18n.hpp"

namespace uapad {

enum class LogLevel { Error = 0, Info = 1, Debug = 2 };
enum class PressMode { Ring, Center };              // where the virtual LMB goes down when walking starts
enum class ButtonChannel { DirectInput, Messages };  // how the virtual LMB reaches the client

// [aim] mark: how the target shows on the client (only ours), in the menu's order. Gasp: a "!" when it is chosen.
// The others: a state the client draws on the target for as long as it is the target — the tint of poison,
// deadly poison or curse, the stars of stun, sleep's zzz, silence's sign, Sight's fireball, Ruwach's light.
enum class MarkStyle : uint8_t { Poison, DeadlyPoison, Curse, Stun, Sleep, Silence, Sight, Ruwach, Gasp, Off };
constexpr int kMarkStyles = 10;
std::optional<MarkStyle> parseMarkStyle(const std::string& token);  // also the 0.3 values 0 and 1
const char* markStyleToken(MarkStyle style);

// [window] position of a borderless window: centred on its monitor, left where the picture is, or at X,Y.
struct WindowPlacement {
    enum class Kind { Center, Keep, At };
    Kind kind = Kind::Center;
    int x = 0;
    int y = 0;
};

struct Config {
    bool enabled = true;
    LogLevel logLevel = LogLevel::Info;
    float deadzone = 0.25f;
    float walkRadius = 0.22f;  // fraction of the shorter side of the client area
    PressMode press = PressMode::Ring;
    ButtonChannel buttons = ButtonChannel::DirectInput;
    bool mouseFreedom = true;  // non-exclusive DirectInput mouse; the game cursor follows the Windows cursor
    float giveUpSeconds = 4.0f;
    float blacklistSeconds = 10.0f;
    int attackIntervalMs = 300;
    std::vector<uint16_t> groundSelf = {12, 25, 27, 70};  // Safety Wall, Pneuma, Warp Portal, Sanctuary
    // Hunter traps: the server refuses a trap with any character (us included) within 1 cell of it.
    std::vector<uint16_t> traps = {115, 116, 117, 118, 119, 120, 121, 122, 123, 125};
    int repeatMs = 250;  // a held slot button fires again this often; 0 = once per press
    MarkStyle mark = MarkStyle::Poison;
    bool rumble = true;
    Lang lang = Lang::En;
    bool mouseLock = false;                    // the cursor stays inside the game window while it is active
    bool focusClick = false;                   // the click that activates the window reaches the game
    std::array<int, 3> mouseKeys = {0, 0, 0};  // middle, 4th, 5th mouse button: 0 none, 1-9 the key F1-F9
    bool borderless = false;
    WindowPlacement position;
    int backgroundSleepMs = 10;  // a frame sleeps this long while the game is in the background; 0 = never
    // 0.6: which controller - auto (an Xbox one first, else a Sony one), xbox, sony
    enum class PadType { Auto, Xbox, Sony } padType = PadType::Auto;
    uint32_t lightbar = 0x0040FF;  // [sony] lightbar: the light bar's colour, 0xRRGGBB (0 = off)
    uint16_t touchpad = 0;         // [sony] touchpad: what its click is - none (0), back, start
    bool npcHint = true;           // 0.7: [npc] hint - the line over the game while an NPC window takes the buttons
};

// "none" / "F1"..."F9" in uaro-pad.ini <-> 0 / 1...9.
std::optional<int> parseFKey(const std::string& token);
std::string fKeyToken(int key);
// 0.6: [sony] lightbar - blue, red, green, purple, orange, white, off or RRGGBB.
std::optional<uint32_t> parseColour(const std::string& v);

// Parses uaro-pad.ini text. Unknown sections/keys and malformed values keep the defaults.
Config parseConfig(const std::string& text);

}  // namespace uapad
