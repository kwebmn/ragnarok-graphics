#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/login_refusals.hpp"
#include "core/window_panel.hpp"

namespace uagfx::logincard {

// 3.0.11-beta.151 (task 3 of the remaining windows, the login's part): the game's login window (UI window 3) in style
// A at the screen's middle - the account's and the password's fields as the game's own fields have them (the password
// as dots), «Запам'ятати логін», «Увійти» and «Вийти» pressing its own buttons, the site's registration under them. The
// parts that need no Windows; the words by GDI (win/login_card).
enum class Focus : uint8_t { None, Account, Password };
struct Fields {
    std::string account;     // UTF-8, as the game's field has it
    int passwordLength = 0;  // its letters' count only
    Focus focus = Focus::None;
    bool caret = false;  // shown now (it blinks)
    bool remember = false;
    // 3.0.11-beta.172: the server's refusal in words under its field, and what to do
    std::string error;
    loginerr::Field errorField = loginerr::Field::None;
    loginerr::Action errorAction = loginerr::Action::None;
    // 3.0.11-beta.172: a hint under a field while the player types (Caps Lock, the layout) - the refusal's place
    std::string hint;
    loginerr::Field hintField = loginerr::Field::None;
    bool addShelf = true;  // 3.0.11-beta.175: «Додати на полицю після входу» (the shelf's own setting)
    // 3.0.11-beta.176: «‹ Полиця» back (the card shown for one other login), the first launch's note at its foot
    bool shelfBack = false;
    std::string note;
};
struct Texts {
    std::string title, subtitle;     // «uaRO», «Вхід у гру»
    std::string account, password;   // «Логін», «Пароль»
    std::string remember;            // «Запам'ятати логін»
    std::string enter, exit;         // «Увійти», «Вийти»
    std::string register_;           // «Немає акаунта? Реєстрація на uaro.kiev.ua»
    std::string createAccount, resetPassword;  // 3.0.11-beta.172: the refusals' actions
    std::string addShelf;                      // 3.0.11-beta.175: «Додати на полицю після входу»
    std::string shelf;                         // 3.0.11-beta.176: «‹ Полиця»
};
struct Show {
    int hover = 0;  // a Part's number, 0 none
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, subtitle, accountLabel, account, passwordLabel, password, remember, tick;
    panel::Box addShelf, addTick;  // 3.0.11-beta.175: under the remember box, the same look
    panel::Box enter, exit, link;
    panel::Box error, errorAction;  // 3.0.11-beta.172: under the field a refusal is about; 0 high when none
    panel::Box back, note;  // 3.0.11-beta.176: «‹ Полиця» (0 wide: none), the note's two lines (0 high: none)
};
Layout layout(int targetW, int targetH, float scale, loginerr::Field errorAt = loginerr::Field::None,
              bool note = false, bool back = false);
enum class Part : uint8_t { None, Account, Password, Remember, Enter, Exit, Register, ErrorAction, AddShelf, Shelf };
Part hitTest(const Layout& L, float x, float y);
int gameButton(Part p);  // the game's window's own: 0x78 enter, 0x9b exit, 0x93 the box; 0 none

// 3.0.11-beta.170 (R-4TNJP8): which of the game's two fields is the account's - the password's is the one the game
// draws as its mask letter ('*'), the account's has none; both or neither masked: not known (the game's own window
// kept)
struct FieldPlaces {
    uint32_t account = 0, password = 0;  // the fields' places in the game's window
    bool known() const { return account && password; }
};
FieldPlaces fieldPlaces(uint32_t first, uint32_t firstMask, uint32_t second, uint32_t secondMask);

void paint(panel::Picture& out, const Layout& L, const Fields& f, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Fields& f, const Show& sh, const Texts& t);

}  // namespace uagfx::logincard
