#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/item_window.hpp"
#include "core/window_panel.hpp"

namespace uagfx::menuwin {

// 3.0.11-beta.140 (the owner «давай» to the first of the game's windows not redrawn yet): the game's menu and its death
// screen in style A - a card under the middle of the screen (the character seen above it), a row a button. Three
// windows of the game: Esc while alive (UI window 0x30: character select, exit, back), the death (0x13: the save point
// first) and the death with a Token of Siegfried in the bag (0x88: reviving here first). Its buttons press the game's
// window's own (message 6 with their ids) - the game does what its own did. The parts that need no Windows; the words
// by GDI (win/menu_window).
enum class Kind : uint8_t { Alive, Dead, DeadToken };
// 3.0.18-beta.121 (the concept «Ігровий інтерфейс», stage 4): ours over the game's in the menu - «Налаштування»,
// «Розмістити інтерфейс», «Тест графіки», «Надіслати звіт» (not the game's buttons: the driver does them)
enum class Action : uint8_t { Revive, SavePoint, CharSelect, Exit, Back, Settings, Layout, Test, Report, Keys };
bool ours(Action a);
// 3.0.18-beta.225 (the keyboard concept's cheat sheet): «Клавіші» - the card itself shows the keys (keysView), the
// game's menu stays open
bool local(Action a);
std::vector<Action> actions(Kind k);  // as shown, top down
int buttonId(Kind k, Action a);       // the game's window's own button; 0 not in it
int firstFocus(Kind k);               // «Повернутися до гри» - Enter by chance changes nothing

struct Texts {
    std::string menu, dead;  // the title: «Меню», «Ви загинули»
    std::string revive, savePoint, charSelect, exit, back;
    std::string settings, layout, test, report;  // 3.0.18-beta.121
    std::string keys, keysBack;                  // 3.0.18-beta.225: «Клавіші», «Назад»
    std::string enter, esc;  // the keys' hints (the controller's buttons while played with it)
    // 3.0.11-beta.277 (the owner's feature 8): the death's last hits - «Останні удари · {1} шкоди», their lines
    std::string recapTitle;
    std::vector<std::string> recap;
};
struct Show {
    int hover = -1;  // the button under the pointer
    int focus = -1;  // the keys' (and the controller's) - its hint on its right
    bool keys = false;  // 3.0.18-beta.225: the card shows «Клавіші»
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box glass, title;
    std::vector<panel::Box> buttons;  // the picture's pixels
    panel::Box recap;                 // 3.0.11-beta.277: the last hits' lines over the buttons (h 0: none)
};
Layout layout(Kind k, int targetW, int targetH, float scale, size_t recapLines = 0);
int buttonAt(const Layout& L, float x, float y);  // the picture's pixels; -1 none

using Words = itemwin::Words;
using Measure = itemwin::Measure;
std::vector<Words> words(const Layout& L, Kind k, const Show& sh, const Texts& t, const Measure& m);

// 3.0.18-beta.225: the keys list (the settings app's «Головне» one, settings::Text::MainKeysList) a line a key:
// «keys — what» (« - » too); a line without either: its words whole
struct KeyLine {
    std::string keys, what;
};
std::vector<KeyLine> keyLines(const std::string& text);
// The card as «Клавіші»: the title, the keys in a gold column, what they do beside wrapped to the card, «Назад» (its
// one button) under them; in the screen, under its middle when it fits
struct KeysView {
    Layout layout;
    std::vector<Words> words;
};
KeysView keysView(int targetW, int targetH, float scale, const std::vector<KeyLine>& lines, const Texts& t,
                  const Show& sh, const Measure& m);

}  // namespace uagfx::menuwin
