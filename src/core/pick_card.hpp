#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::pickcard {

// 3.0.11-beta.157 (task 7 of the remaining windows): the game's «choose a thing» windows - the magnifier's list of
// things not known yet (UI window 0x49) and a card's list of the things it goes into (0x4A) - as one card in style A:
// its title and a line under it, a row a thing (its icon, its name), the chosen one outlined in gold, OK and cancel
// the game's own buttons. The parts that need no Windows; the words by GDI (win/pick_card).
constexpr int kOkButton = 0x76, kCancelButton = 0x77;
struct Entry {
    std::string name;  // UTF-8
    int refine = 0;
    const panel::Picture* icon = nullptr;
    std::string inside;  // 3.0.18-beta.142: its cards and free slots, a forged one's maker ("" none)
    uint32_t info = 0;   // its ITEM_INFO in the game (its card)
    std::string colour = "#eef2f8";  // 3.0.18-beta.144: its name's (its element's)
};
struct Texts {
    std::string title, message, ok, cancel, empty, hint;  // hint: the controller's buttons, "" none
};
struct Show {
    int selected = -1, hover = -1, scroll = 0;
    int hoverPart = 0;  // its Part
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, message, list, hint, ok, cancel;
    std::vector<panel::Box> rows;  // shown (7 at most)
    panel::Box bar;  // 3.0.11-beta.187: the scroll's bar at the list's right - 0 wide when all rows are shown
};
Layout layout(int targetW, int targetH, float scale, size_t count);
int mostScroll(const Layout& L, size_t count);
enum class Part : uint8_t { None, Row, Ok, Cancel };
struct Hit {
    Part part = Part::None;
    int row = -1;  // of all (the scroll counted)
};
Hit hitTest(const Layout& L, int scroll, size_t count, float x, float y);
// 3.0.11-beta.171 (the login concept): the row to press OK on at once - the only one; -1 when the player chooses
int autoChoice(size_t entries, int chosen);


enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
// 3.0.19-beta.1 (the testers: «тут код карти прописує, а встарому назви»): a Teleport / Warp Portal row naming a map
// (its code, «.gat» or not, any case) - the client's name for it (data\mapnametable.txt via refindex::mapNames: whole,
// a space after each comma) and the code (shown under it); a row naming no map known - as it is, no code
struct Place {
    std::string name, code;
};
Place placeOf(const std::map<std::string, std::string>& names, const std::string& row);

}  // namespace uagfx::pickcard
