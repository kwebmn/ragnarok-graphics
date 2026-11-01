#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::askcard {

// 3.0.11-beta.150 (the owner: «сделай все без остановки, в инлайн режиме» - task 2 of the remaining windows): the
// game's small windows as one card in style A at the screen's middle - a trade's, a party's, a guild's or a friend's
// request (UI windows 0x20, 0x23, 0x46, 0x6D) with accept and decline, an NPC's number or text to type (0x38, 0x64)
// with the game's own field shown as it has it and its OK. The parts that need no Windows; the words by GDI
// (win/ask_card).
// 3.0.11-beta.151: the game's messages too (UIWindowMgr::ErrorMsg's box - OK, Yes and No, OK and Cancel - and its
// wait with no button: «Unregistered ID ...», «Please wait...»)
enum class Kind : uint8_t { Trade, Party, Guild, Friend, Number, Text, Message, Question, Choice, Wait };
bool request(Kind k);  // accept and decline
bool input(Kind k);    // an NPC's number or words with the game's field and its OK
enum class Button : uint8_t { Accept, Decline, Ok, Yes, No, Cancel };
std::vector<Button> buttons(Kind k);
int gameButton(Button b);  // the game's window's own: 0x76 accept and OK, 0x77 decline and cancel, 0x79 yes, 0x7a no

struct Card {
    Kind kind = Kind::Trade;
    std::string title, message;
    std::string field;   // an input's words as the game's field has them (UTF-8)
    bool caret = false;  // its caret shown now (it blinks)
    int messageLines = 2;  // 3.0.11-beta.151: the rows its message takes at the card's width (the words by GDI)
};
struct Texts {
    std::string accept, decline, ok;  // «Прийняти», «Відмовити», «Готово»
    std::string yes, no, cancel;      // 3.0.11-beta.151: «Так», «Ні», «Скасувати»
    std::string acceptKey, declineKey, okKey;  // the keys or the controller's buttons beside them ("" none)
};
struct Show {
    int hover = -1;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, message, field;
    std::vector<panel::Box> buttons;  // buttons(kind)'s, in a row
};
// At the screen's middle across, a third down
Layout layout(const Card& c, int targetW, int targetH, float scale, const Texts& t);
// 3.0.11-beta.151: the message's room across (its rows measured against it by the window part)
float messageWidth(float scale);
int buttonAt(const Layout& L, float x, float y);  // the picture's pixels; -1 none

void paint(panel::Picture& out, const Layout& L, const Card& c, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
    bool wrap = false;  // 3.0.11-beta.151: broken into rows at its box's width (a message)
};
std::vector<Words> words(const Layout& L, const Card& c, const Show& sh, const Texts& t);

}  // namespace uagfx::askcard
