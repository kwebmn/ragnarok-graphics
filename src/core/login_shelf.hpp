#pragma once
#include "core/ui_sheet_model.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/site_api.hpp"
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
    bool fromSite = false;  // 3.0.11-beta.181: the site account's login (no password here: asked once)
    bool localRow = true;   // on this computer's shelf (false: the site's alone)
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
    std::string noPassword;             // 3.0.11-beta.181: «Пароль — ввести один раз»
    std::string chars, newLogin, linkLogin;  // «Персонажі ›», «+ Новий ігровий логін», «Прив'язати наявний»
    std::string searchHint, noMatch;         // 3.0.11-beta.189: «Пошук…», «Нічого не знайдено»
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the shelf's picture in the target's pixels (the title to the buttons)
    panel::Box title;
    std::vector<panel::Box> cards;  // the accounts in sight: cards[i] is account first + i
    panel::Box add;                 // 0 wide: not in sight
    panel::Box left, right;         // the arrows: 0 wide when nothing is beyond
    panel::Box other, create, exit;
    panel::Box chars, newLogin, linkLogin;  // 3.0.11-beta.181: signed in to the site (0 wide: not)
    int first = 0;
    // 3.0.11-beta.189: the place the row goes to (first is the card half in sight at its left while it glides); the
    // row (the cards clipped to it); the bar under it and its thumb (0 wide: all in sight); the search's field, its ×
    int target = 0, most = 0;  // most: the place with the row's end in sight
    panel::Box row, bar, thumb, search, searchClear;
};
constexpr int kInSight = 4;
// 3.0.11-beta.189 (the owner: «если там много аккаунтов, сделай какието стрелочки, скрол чтобы мотать было интереснее.
// возможно еще и поиск», «чтобы мышкой скролить можно было»): the row's shown place gliding to `first` (glide), the
// search's field on the title row's left (more accounts than in sight), «Додати акаунт» left out while it searches
struct Scroll {
    float pos = -1;  // -1: at first
    bool search = false;
    bool add = true;
};
Layout layout(int targetW, int targetH, float scale, size_t cards, int first, bool site = false,
              const Scroll& sc = Scroll{});
int mostFirst(size_t cards, bool add);         // the row's end in sight
float glide(float pos, int first, float ms);  // the row's shown place `ms` later (-1: not shown yet - there)
int barFirst(const Layout& L, float x);        // a press on the bar at x: the place to go to
// All the search's words (any case) in one of `words` or another; an empty search: all
bool matches(const std::string& search, const std::vector<std::string>& words);
enum class Part : uint8_t {
    None, Card, Menu, Add, Other, Create, Exit, Left, Right, Chars, NewLogin, LinkLogin,
    Search, SearchClear, Bar  // 3.0.11-beta.189
};
struct Hit {
    Part part = Part::None;
    int index = -1;  // Card, Menu: the account's
};
Hit hitTest(const Layout& L, float x, float y);
panel::Box menuButton(const Layout& L, int visible);  // the ⋯ of cards[visible]
bool classicInstead(size_t accounts);                 // the first launch: the classic card, its note says why
int padMove(int index, int count, int dir);  // 3.0.11-beta.179: the pad's ◀ ▶ - the next card, stopping at the ends
std::string jobLine(const Card& c);                   // «High Priest · 99 · Prontera»

struct Show {
    int focus = 0;   // the account Enter plays
    int hover = -1;  // the account under the cursor
    Part hoverPart = Part::None;
    int editing = -1;       // the account whose label is typed (its words in editText, a caret)
    std::string editText;
    bool caret = false;
    std::string search;  // 3.0.11-beta.189: typed into the search (the caret's blink: caret, editing -1)
};
enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 13;
    bool bold = false;
    Align align = Align::Left;
    bool row = false;  // 3.0.11-beta.189: a card's - drawn within the row alone (a card half in sight)
};
std::vector<Words> words(const Layout& L, const std::vector<Card>& cards, const Show& sh, const Texts& t);

// 3.0.11-beta.181 (the site account): «Персонажі» - a row a login (its name over it), up to three character cards and
// «Новий персонаж» after them; four rows in sight, ▲ ▼ beyond; «‹ Акаунти» back to the cards
struct CharRow {
    panel::Box label;
    std::vector<panel::Box> cards;
    panel::Box newChar;
};
struct CharLayout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // in the target's pixels; the boxes in the picture's
    panel::Box title, back, up, down;  // up / down 0 wide: nothing beyond
    std::vector<CharRow> rows;         // the rows in sight: rows[i] is the login first + i
    int first = 0;
};
constexpr int kCharsInRow = 3, kRowsInSight = 4;
CharLayout characterRows(int targetW, int targetH, float scale, const std::vector<int>& charsPerLogin, int first = 0);
struct CharHit {
    int row = -1, card = -1;  // the login's (its place in charsPerLogin), its character's
    bool newChar = false, back = false, up = false, down = false;
};
CharHit characterHit(const CharLayout& L, float x, float y);
struct CharLogin {  // a row's login and its characters (the site's)
    std::string login;  // UTF-8
    bool hasPassword = false;
    std::vector<siteapi::SiteChar> chars;
    std::vector<const panel::Picture*> art;  // a portrait a character (core/class_art), null none
};
struct CharTexts {
    std::string title, back, newChar, noPassword;
};
struct CharShow {
    int hoverRow = -1, hoverCard = -1;  // hoverCard -1 with a row: its «Новий персонаж»
    bool hoverBack = false;
};
void paintCharacters(panel::Picture& out, const CharLayout& L, const std::vector<CharLogin>& logins,
                     const CharShow& sh);
std::vector<Words> characterWords(const CharLayout& L, const std::vector<CharLogin>& logins, const CharShow& sh,
                                  const CharTexts& t);

// A card's menu, over the card under its ⋯: a row an item
enum class MenuItem : uint8_t { AutoEnter, KeepPassword, Label, Edit, MoveLeft, MoveRight, Remove, GamePassword };
std::vector<MenuItem> menu(const Card& c);
struct MenuLayout {
    float x = 0, y = 0, w = 0, h = 0;  // in the shelf's picture
    std::vector<panel::Box> rows;
};
MenuLayout menuLayout(const Layout& L, int visible, size_t items);
int menuHit(const MenuLayout& M, float x, float y);  // the row, -1 none
struct MenuTexts {
    std::string autoEnter, keepPassword, label, edit, moveLeft, moveRight, remove, removeSure;
    std::string gamePassword;  // 3.0.11-beta.181: «Змінити пароль гри»
};
struct MenuShow {
    int hover = -1;
    bool autoEnter = true, keepPassword = true, removeSure = false;
};
void paintMenu(panel::Picture& out, const Layout& L, const MenuLayout& M, const MenuShow& sh);
std::vector<Words> menuWords(const MenuLayout& M, const std::vector<MenuItem>& items, const MenuShow& sh,
                             const MenuTexts& t, float scale);

// 3.0.18-beta.41 (uaUI's «Полиця»): what paint(), paintCharacters() and paintMenu() draw, as the markup's data
// (core/ui_sheet_model) into `m` (the caller sets its size and words)
void sheetOf(sheet::Model& m, const Layout& L, const std::vector<Card>& cards, const Show& sh, const Texts& t);
void sheetOfCharacters(sheet::Model& m, const CharLayout& L, const std::vector<CharLogin>& logins,
                     const CharShow& sh);
void sheetOfMenu(sheet::Model& m, const Layout& L, const MenuLayout& M, const MenuShow& sh);

}  // namespace uagfx::loginshelf
