#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::sitesignin {

// 3.0.11-beta.180 (the login concept: the site account in the client): the login screen's card signing in to the
// uaro.kiev.ua account - all its game logins on the shelf at once. Three ways: the site's password (and its
// two-factor code when the site asks), a code confirmed in the browser (the cabinet's /account/device), the cabinet's
// key pasted. The parts that need no Windows; the words by GDI (win/site_signin).
// .181: the last four - forms (Register: the registration in the client, one form)
enum class Way : uint8_t { Password, Browser, Key, NewLogin, Link, GamePassword, Register };
enum class Field : uint8_t { None, Email, Password, Code, Key, Login, GamePass, NewPass };
struct Texts {
    std::string title, subtitle;
    std::string ways[3];
    std::string email, password, code2fa, key;
    std::string signIn, startBrowser;
    std::string waiting;  // «Чекаємо підтвердження… {1} с»
    std::string back;
    // 3.0.11-beta.181: the forms - their titles, fields, the sexes and their buttons
    std::string forms[3];  // «Новий ігровий логін», «Прив'язати наявний логін», «Змінити пароль гри»
    std::string login, gamePass, currentPass, newPass, sexes[2], create, link, change;
    // 3.0.11-beta.181: the registration
    std::string regTitle, steps, password2, agree, rules, regCreate, haveAccount, free, taken;
};
struct Show {
    Way way = Way::Password;
    Field focus = Field::None;
    std::string email, code2fa;  // UTF-8
    int passwordLength = 0, keyLength = 0;
    bool caret = false;
    std::string userCode;  // the browser's code while it waits ("" none yet)
    int secondsLeft = 0;
    std::string error;  // red under the fields
    bool busy = false;  // a request on its way: the button dimmed
    int hover = 0;      // a Part's number
    std::string login;  // 3.0.11-beta.181: the forms'
    int gamePassLength = 0, newPassLength = 0;
    char sex = 'M';      // 'M', 'F'; 0 not chosen yet (the registration)
    bool agree = false;  // the registration's rules ticked
    int loginFree = -1;  // its login asked: 1 free, 0 taken, -1 not known
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, subtitle, ways[3];
    panel::Box emailLabel, email, passwordLabel, password, codeLabel, code, keyLabel, key;  // 0 high: not on the way
    panel::Box loginLabel, login, gamePassLabel, gamePass, newPassLabel, newPass, sex[2];  // 3.0.11-beta.181
    panel::Box steps, free, agree, agreeTick, rules, haveAccount;  // the registration (0 high: not on the way)
    panel::Box codeBig, wait;  // the browser's code and its wait (the browser's way)
    panel::Box error, button, back;
};
Layout layout(int targetW, int targetH, float scale, Way way, bool twofa);
enum class Part : uint8_t {
    None, Way0, Way1, Way2, Email, Password, Code, Key, Button, Back, Login, GamePass, NewPass, Sex0, Sex1, Agree,
    Rules, HaveAccount
};
bool isForm(Way w);  // a form of the signed-in site account (no ways' tabs)
Part hitTest(const Layout& L, float x, float y);
void paint(panel::Picture& out, const Layout& L, const Show& sh);
enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 13;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Show& sh, const Texts& t);

}  // namespace uagfx::sitesignin
