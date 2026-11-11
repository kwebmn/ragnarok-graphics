#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::osk {

// 3.0.11-beta.214 (the owner: «3. можно» - the grid keyboard of consoles): the controller's on-screen keyboard - the
// chat wheel's A opens it at the screen's lower middle; the D-pad or the left stick moves over the keys, A types the
// key into the game's own chat line, X erases, Y a space, LB / RB the grid (Ukrainian, Russian, English, symbols),
// LT Shift, Start sends the line as Enter does, B closes it (the line kept). The parts that need no Windows; the words
// by GDI (win/osk_window).
enum class Lang : uint8_t { Uk, Ru, En, Symbols };
constexpr int kLangs = 4;
Lang next(Lang l, int by);
enum class Special : uint8_t { None, Shift, Space, Back, Lang, Send };
struct Key {
    std::string text;  // UTF-8 (a special's: its own words come from Texts)
    Special special = Special::None;
    float span = 1;  // in keys
};
using Grid = std::vector<std::vector<Key>>;
Grid rows(Lang l, bool shift);  // the letters' rows, the specials' row last
struct Focus {
    int row = 0, col = 0;
};
// The D-pad: dx round a row, dy to the key under the same place (rows centred), round the grid
Focus move(const Grid& g, Focus f, int dx, int dy);
const Key& keyAt(const Grid& g, Focus f);

struct Texts {
    std::string space, send, lang, hint;  // «пробіл», «Надіслати», the grid's name, the buttons' line
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, line, hint;
    std::vector<std::vector<panel::Box>> keys;  // as the grid's
};
Layout layout(const Grid& g, int targetW, int targetH, float scale);
enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Center;
};
// `line` the game's chat line (UTF-8), `caret` its caret shown now
std::vector<Words> words(const Layout& L, const Grid& g, Focus f, const std::string& line, bool caret, const Texts& t);

}  // namespace uagfx::osk
