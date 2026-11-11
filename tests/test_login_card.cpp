// 3.0.11-beta.151 (task 3 of the remaining windows, the login's part): the game's login window in the new look
// (core/login_card) - the account's and the password's fields as the game's own have them, «Запам'ятати логін»,
// «Увійти» and «Вийти», the site's registration under them
#include <algorithm>
#include <string>
#include <vector>

#include "core/login_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

logincard::Texts texts() {
    logincard::Texts t;
    t.title = "uaRO";
    t.subtitle = "Вхід у гру";
    t.account = "Логін";
    t.password = "Пароль";
    t.remember = "Запам'ятати логін";
    t.enter = "Увійти";
    t.exit = "Вийти";
    t.register_ = "Немає акаунта? Реєстрація на uaro.kiev.ua";
    return t;
}

bool has(const std::vector<logincard::Words>& w, const std::string& text) {
    return std::any_of(w.begin(), w.end(), [&](const logincard::Words& x) { return x.text == text; });
}

}  // namespace

// The card at the screen's middle; the two fields one over the other, the box, the two buttons, the link at its foot;
// the mouse on each part; the game's buttons: 0x78 enter, 0x9b exit, 0x93 the box
TEST(login_card_places_and_the_mouse) {
    const logincard::Layout L = logincard::layout(1920, 1080, 1.0f);
    CHECK(std::abs(L.x + L.w / 2 - 960) <= 2 && std::abs(L.y + L.h / 2 - 540) < 200);
    CHECK(L.password.y > L.account.y + L.account.h - 1 && L.remember.y > L.password.y);
    CHECK(L.enter.y > L.remember.y && L.exit.x > L.enter.x && L.link.y > L.enter.y);
    using P = logincard::Part;
    const auto mid = [](const panel::Box& b) { return std::pair<float, float>{b.x + b.w / 2, b.y + b.h / 2}; };
    CHECK(logincard::hitTest(L, mid(L.account).first, mid(L.account).second) == P::Account);
    CHECK(logincard::hitTest(L, mid(L.password).first, mid(L.password).second) == P::Password);
    CHECK(logincard::hitTest(L, mid(L.remember).first, mid(L.remember).second) == P::Remember);
    CHECK(logincard::hitTest(L, mid(L.enter).first, mid(L.enter).second) == P::Enter);
    CHECK(logincard::hitTest(L, mid(L.exit).first, mid(L.exit).second) == P::Exit);
    CHECK(logincard::hitTest(L, mid(L.link).first, mid(L.link).second) == P::Register);
    CHECK(logincard::hitTest(L, -5, -5) == P::None);
    CHECK(logincard::gameButton(P::Enter) == 0x78 && logincard::gameButton(P::Exit) == 0x9B);
    CHECK(logincard::gameButton(P::Remember) == 0x93 && logincard::gameButton(P::Account) == 0);
}

// 3.0.11-beta.170 (R-4TNJP8, «не входит в игру» - «Unregistered ID» on every try): .151 had the game's two fields the
// wrong way round. The game's login window keeps the account at + 0x60 and the password at + 0x64 - the one it draws
// as '*' (the field's mask letter + 0x50; it loads the saved login into + 0x60 and saves it from there), so the
// password typed into «Пароль» went into the game's account field and to the server as the login. Which is which now
// from the mask itself: the masked one the password; both or neither masked - not known, the game's own window kept
TEST(login_card_fields_by_the_games_mask) {
    const logincard::FieldPlaces game = logincard::fieldPlaces(0x60, 0, 0x64, '*');
    CHECK(game.known() && game.account == 0x60 && game.password == 0x64);
    const logincard::FieldPlaces other = logincard::fieldPlaces(0x60, '*', 0x64, 0);
    CHECK(other.known() && other.account == 0x64 && other.password == 0x60);
    CHECK(!logincard::fieldPlaces(0x60, 0, 0x64, 0).known());      // no password field
    CHECK(!logincard::fieldPlaces(0x60, '*', 0x64, '*').known());  // no account field
    CHECK(!logincard::FieldPlaces{}.known());
}

