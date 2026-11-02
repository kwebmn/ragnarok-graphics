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

// Its words: the account as typed, the password as dots (never its letters), the caret on the field typed in, the
// box's tick, the buttons, the link
TEST(login_card_words) {
    const logincard::Layout L = logincard::layout(1920, 1080, 1.0f);
    logincard::Fields f;
    f.account = "tester";
    f.passwordLength = 7;
    f.focus = logincard::Focus::Password;
    f.caret = true;
    f.remember = true;
    const std::vector<logincard::Words> w = logincard::words(L, f, logincard::Show{}, texts());
    CHECK(has(w, "tester") && has(w, "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2"
                                     "\xE2\x80\xA2|"));  // seven dots and the caret
    CHECK(has(w, "Логін") && has(w, "Пароль") && has(w, "Запам'ятати логін") && has(w, "Увійти") && has(w, "Вийти"));
    CHECK(has(w, "Немає акаунта? Реєстрація на uaro.kiev.ua") && has(w, "uaRO"));
    f.focus = logincard::Focus::Account;
    const std::vector<logincard::Words> w2 = logincard::words(L, f, logincard::Show{}, texts());
    CHECK(has(w2, "tester|"));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    logincard::paint(p, L, f, logincard::Show{});
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)] >> 24) > 0x80);
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

// 3.0.11-beta.172 (the login concept: the server's answer in words): the refusal under the field it is about - the
// card grows by its line and the action's link, the field outlined red; no refusal: the card as before
TEST(login_card_error_line_under_its_field) {
    const logincard::Layout plain = logincard::layout(1920, 1080, 1.0f);
    const logincard::Layout L = logincard::layout(1920, 1080, 1.0f, loginerr::Field::Password);
    CHECK(plain.error.h == 0 && L.error.h > 0 && L.h > plain.h);
    CHECK(L.error.y >= L.password.y + L.password.h && L.error.y + L.error.h <= L.remember.y);
    CHECK(L.errorAction.y >= L.error.y && L.errorAction.y + L.errorAction.h <= L.remember.y + 1);
    logincard::Fields f;
    f.error = "Пароль не підходить до kweb.";
    f.errorField = loginerr::Field::Password;
    f.errorAction = loginerr::Action::ResetPassword;
    logincard::Texts t = texts();
    t.resetPassword = "Відновити пароль на сайті";
    const auto w = logincard::words(L, f, logincard::Show{}, t);
    CHECK(has(w, "Пароль не підходить до kweb.") && has(w, "Відновити пароль на сайті"));
    CHECK(logincard::hitTest(L, L.errorAction.x + 4, L.errorAction.y + L.errorAction.h / 2) ==
          logincard::Part::ErrorAction);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    logincard::paint(p, L, f, logincard::Show{});
    const uint32_t edge = p.px[size_t(L.password.y) * size_t(L.w) + size_t(L.password.x + L.password.w / 2)];
    CHECK(((edge >> 16) & 0xFF) > ((edge >> 8) & 0xFF) + 40);  // red over green: the field's border
}

// 3.0.11-beta.172 (the login concept: hints before a mistake): Caps Lock or the layout under the field in amber, in
// the refusal's place (none there now); the card's top stays where it was - it grows downward only
TEST(login_card_hint_line_in_amber) {
    const logincard::Layout plain = logincard::layout(1920, 1080, 1.0f);
    const logincard::Layout L = logincard::layout(1920, 1080, 1.0f, loginerr::Field::Account);
    CHECK(L.y == plain.y && L.h > plain.h);
    logincard::Fields f;
    f.hint = "Розкладка УКР — логін зазвичай латиницею (kweb?)";
    f.hintField = loginerr::Field::Account;
    const auto w = logincard::words(L, f, logincard::Show{}, texts());
    const auto it = std::find_if(w.begin(), w.end(), [&](const logincard::Words& x) { return x.text == f.hint; });
    CHECK(it != w.end() && it->rgb == 0xF2C46B && it->box.y == L.error.y);
}
