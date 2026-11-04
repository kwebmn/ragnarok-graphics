#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::chatroomcard {

// 3.0.11-beta.207 (the tester's third list: «alt c - створити чат рум - в старому стилі»): the game's chat room
// making window (Alt+C, UI window 0x1B) as a card in style A at the screen's middle - the game's own title and password
// fields shown as they are (a click gives one the keys), the places as chips (the game's list: 2 to 20), open or with
// a password, «Створити» (the game's OK checks and sends) and «Скасувати». The parts that need no Windows; the words by
// GDI (win/chatroom_window).
enum class Field : uint8_t { None, Title, Password };
struct Card {
    std::string title;            // UTF-8, as the game's field has it
    size_t passwordLength = 0;    // the password's field: its length only (never its words)
    std::vector<int> limits;      // the game's place counts
    int limit = 0;                // the chosen count
    bool closed = false;          // with a password
    Field focus = Field::None;
    bool caret = false;  // the focused field's caret shown now (it blinks)
};
struct Texts {
    std::string heading, titleLabel, limitLabel, open, closed, passwordLabel, create, cancel;
};
enum class Part : uint8_t { None, Title, Limit, Open, Closed, Password, Create, Cancel, Close };
struct Hit {
    Part part = Part::None;
    int index = -1;  // a chip's
};
struct Show {
    Hit hover;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, heading, close, titleLabel, title, limitLabel, open, closed, passwordLabel, password, create,
        cancel;
    std::vector<panel::Box> limits;
};
std::string masked(size_t n);  // the password's dots
// At the screen's middle across, a third down; `limits` the chips' count
Layout layout(size_t limits, int targetW, int targetH, float scale);
Hit hitTest(const Layout& L, float x, float y);  // the picture's pixels

void paint(panel::Picture& out, const Layout& L, const Card& c, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Card& c, const Show& sh, const Texts& t);

}  // namespace uagfx::chatroomcard
