#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::emotecard {

// 3.0.11-beta.205 (the tester's third list: «alt l - список емоцій 1) в старому стилі 2) використання емоцій - по
// кліку пише емоцію в чат і для виконання потрібно натиснути ентер ... по даблкліку теж виконувались»): the game's
// emotion list (Alt+L, UI window 0x57) as a card in style A at the screen's middle - its 49 emotions seven a row, each
// the game's own picture (emotion.spr by emotion_index3.act) over its command; a click puts it into the chat as the
// game's list does, a double click sends it at once, ✕ closes. The parts that need no Windows; the words by GDI
// (win/emote_window).
constexpr int kEmotions = 49, kColumns = 7;
constexpr int kIconSide = 40;  // the pictures' room at scale 1

struct Texts {
    std::string title, hint;  // «Емоції», how a click and a double click use them
};
struct Show {
    int hover = -1;
    bool hoverClose = false;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, hint, close;
    std::vector<panel::Box> cells, icons, labels;  // kEmotions of each
};
// At the screen's middle across, a third down (inside the screen)
Layout layout(int targetW, int targetH, float scale);
int cellAt(const Layout& L, float x, float y);  // the picture's pixels; -1 none
bool closeAt(const Layout& L, float x, float y);


enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
// `commands` the list's own (its order; as many as it has)
std::vector<Words> words(const Layout& L, const std::vector<std::string>& commands, const Show& sh, const Texts& t);

}  // namespace uagfx::emotecard
