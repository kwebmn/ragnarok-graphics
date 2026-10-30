#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::tracker {

// 3.0.11-beta.70 (the concept's «держать перед глазами» - «Отслеживание — что закрепить (квест, путь, вещь для сбора),
// выбирает игрок; окно стоит левее столбика баффов»; v17_hud's tracker without the counters the owner dropped): the
// tracker panel - what the player pinned, each an entry: its name, its kind, its lines (a quest's items to bring and
// whom to give them, the way's step, an item's sources), ✕ to let it go; ▾ folds them to their names, ✕ at the top
// hides the panel. The parts that need no Windows; the words go over it by GDI (win/tracker).

enum class Kind : uint8_t { Quest, Route, Item };
struct Line {
    std::string text, right;  // the right one at the right edge («×100», «58,2 %»)
    bool strong = false;      // whom to give a quest's items: in the text's colour, bold
};
struct Entry {
    Kind kind = Kind::Quest;
    std::string title, tag;  // «Bunny Band», «квест»
    std::vector<Line> lines;
};

struct Placed {
    panel::Box box, head, close;
    std::vector<panel::Box> lines;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box glass, title, fold, hide;
    std::vector<Placed> entries;  // those shown, from the first
    int hidden = 0;               // the entries past the room
    panel::Box more;              // «ще N» under them; w 0: none
};
// Its top right corner at (right, top), reaching `bottom` at the most; `folded`: the entries' names alone
Layout layout(const std::vector<Entry>& entries, bool folded, int right, int top, int bottom, float scale);

enum class Part : uint8_t { None, Panel, Fold, Hide, Head, Close };
struct Hit {
    Part part = Part::None;
    int entry = -1;
};
Hit hitTest(const Layout& L, float x, float y);  // the picture's pixels

void paint(panel::Picture& out, const Layout& L, const std::vector<Entry>& entries, const Hit& hover);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
struct Texts {
    std::string title;  // «Відстеження»
    std::string more;   // «ще {1}»
};
std::vector<Words> words(const Layout& L, const std::vector<Entry>& entries, const Texts& t, bool folded = false);

}  // namespace uagfx::tracker
