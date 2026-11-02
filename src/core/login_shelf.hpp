#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::loginshelf {

// 3.0.11-beta.176 (the login concept's «Полиця»): the login screen's shelf of accounts (core/accounts) - a card an
// account in a row under the game's logo (the character it last entered with: its picture, name, job, level, map; the
// player's label; the login), a click enters; «Додати акаунт» after them; four in sight, ◀ ▶ beyond; «Інший акаунт»,
// «Створити акаунт», «Вийти» under the row; a card's menu at its ⋯. The parts that need no Windows; the words by GDI
// (win/login_shelf).
struct Card {
    std::string label, name, job, map, login;  // UTF-8; name "" - no character yet (the login big)
    int level = 0;
    bool busy = false, refused = false, hasPassword = true;
    int busyWindow = 0;  // 3.0.11-beta.178: the game window that plays it («вже в грі (вікно N)»)
    const panel::Picture* art = nullptr;   // the class's portrait (core/class_art); null - its head or a plain ground
    const panel::Picture* head = nullptr;  // 3.0.11-beta.178: the character's head (no portrait for the job)
};
struct Texts {
    std::string title, titleHint;  // «Оберіть акаунт», «— один клік, і ви в грі»
    std::string play, enterKey;    // «Грати», «Enter»
    std::string loginWord;         // «логін {1}»
    std::string add, addHint;      // «Додати акаунт», «логін і пароль один раз, далі — в один клік»
    std::string other, create, exit;
    std::string refused, busy, noChar;  // «Пароль не підходить», «Вже в грі», «Ще не грали»
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the shelf's picture in the target's pixels (the title to the buttons)
    panel::Box title;
    std::vector<panel::Box> cards;  // the accounts in sight: cards[i] is account first + i
    panel::Box add;                 // 0 wide: not in sight
    panel::Box left, right;         // the arrows: 0 wide when nothing is beyond
    panel::Box other, create, exit;
    int first = 0;
};
constexpr int kInSight = 4;
Layout layout(int targetW, int targetH, float scale, size_t cards, int first);
enum class Part : uint8_t { None, Card, Menu, Add, Other, Create, Exit, Left, Right };
struct Hit {
    Part part = Part::None;
    int index = -1;  // Card, Menu: the account's
};
Hit hitTest(const Layout& L, float x, float y);
panel::Box menuButton(const Layout& L, int visible);  // the ⋯ of cards[visible]
bool classicInstead(size_t accounts);                 // the first launch: the classic card, its note says why
std::string jobLine(const Card& c);                   // «High Priest · 99 · Prontera»

struct Show {
    int focus = 0;   // the account Enter plays
    int hover = -1;  // the account under the cursor
    Part hoverPart = Part::None;
    int editing = -1;       // the account whose label is typed (its words in editText, a caret)
    std::string editText;
    bool caret = false;
};
void paint(panel::Picture& out, const Layout& L, const std::vector<Card>& cards, const Show& sh, const Texts& t);
enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 13;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const std::vector<Card>& cards, const Show& sh, const Texts& t);

// A card's menu, over the card under its ⋯: a row an item
enum class MenuItem : uint8_t { AutoEnter, KeepPassword, Label, Edit, MoveLeft, MoveRight, Remove };
std::vector<MenuItem> menu(const Card& c);
struct MenuLayout {
    float x = 0, y = 0, w = 0, h = 0;  // in the shelf's picture
    std::vector<panel::Box> rows;
};
MenuLayout menuLayout(const Layout& L, int visible, size_t items);
int menuHit(const MenuLayout& M, float x, float y);  // the row, -1 none
struct MenuTexts {
    std::string autoEnter, keepPassword, label, edit, moveLeft, moveRight, remove, removeSure;
};
struct MenuShow {
    int hover = -1;
    bool autoEnter = true, keepPassword = true, removeSure = false;
};
void paintMenu(panel::Picture& out, const Layout& L, const MenuLayout& M, const MenuShow& sh);
std::vector<Words> menuWords(const MenuLayout& M, const std::vector<MenuItem>& items, const MenuShow& sh,
                             const MenuTexts& t, float scale);

}  // namespace uagfx::loginshelf
