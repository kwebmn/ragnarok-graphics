#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::commandcard {

// 3.0.11-beta.206 (the tester's third list: «alt j - command list в старому стилі» - the game's command list, its
// Alt+Y): the game's command window (UI window 0x70) as a card in style A at the screen's middle - its three lists on
// tabs, two columns: the switches (/battlemode, /noctrl ... with their state, the game's on/off), the commands (/sit,
// /where ...) and the windows by their Alt keys; a click does what the game's click does. The parts that need no
// Windows; the words by GDI (win/command_window).
enum class Tab : uint8_t { Switches, Commands, Windows };
constexpr int kColumns = 2;

struct Entry {
    std::string text;  // UTF-8: the game's command, or a window's name
    int code = 0;      // the game's command code (mode message 0x2F), or a window's key (Alt+)
    int state = -1;    // a switch's: 1 on, 0 off; -1 none
};
// Each entry's state from the game's pairs {state, code} (its switches' list)
void markStates(std::vector<Entry>& entries, const std::vector<std::pair<int, int>>& states);
std::string keyName(int vk);  // «Alt+C»; "" for a key not a letter

struct Texts {
    std::string title, tabs[3], on, off;  // «Список команд», the tabs, a switch's state
};
struct Show {
    Tab tab = Tab::Switches;
    int hover = -1, hoverTab = -1;
    bool hoverClose = false;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close;
    std::vector<panel::Box> tabs, rows;  // rows: kColumns a row, as many as the longest list
};
// At the screen's middle across, a third down (inside the screen); `most` the longest list's entries
Layout layout(size_t most, int targetW, int targetH, float scale);

enum class Part : uint8_t { None, Tab, Row, Close };
struct Hit {
    Part part = Part::None;
    int index = -1;
};
Hit hitTest(const Layout& L, float x, float y);  // the picture's pixels


enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const std::vector<Entry>& entries, const Show& sh, const Texts& t);

}  // namespace uagfx::commandcard
