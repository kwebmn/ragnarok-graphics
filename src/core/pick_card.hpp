#pragma once
#include <cstdint>
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
// 3.0.11-beta.187 (the tester: «мені не вистачає скрола тут»): the bar's thumb - as long as the share of the rows
// shown, where `scroll` is; 0 high with no bar
panel::Box thumb(const Layout& L, int scroll, size_t count);
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
std::vector<Words> words(const Layout& L, const std::vector<Entry>& entries, const Show& sh, const Texts& t);

// 3.0.18-beta.31 (uaUI's choice of things): a row's icon square (none when its entry has no icon)
panel::Box iconOf(const panel::Box& row, const Entry& e, float s);

}  // namespace uagfx::pickcard
