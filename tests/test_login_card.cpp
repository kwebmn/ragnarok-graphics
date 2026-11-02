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

// 3.0.11-beta.175 (the login concept's shelf): «Додати на полицю після входу» under «Запам'ятати логін» - our own
// tick (no button of the game's), gold when on
TEST(login_card_add_to_shelf_tick) {
    const logincard::Layout L = logincard::layout(1920, 1080, 1.0f);
    CHECK(L.addShelf.y >= L.remember.y + L.remember.h - 0.5f && L.enter.y > L.addShelf.y + L.addShelf.h - 0.5f);
    CHECK(logincard::hitTest(L, L.addShelf.x + L.addShelf.w / 2, L.addShelf.y + L.addShelf.h / 2) ==
          logincard::Part::AddShelf);
    CHECK(logincard::gameButton(logincard::Part::AddShelf) == 0);
    logincard::Texts t = texts();
    t.addShelf = "Додати на полицю після входу";
    logincard::Fields f;
    CHECK(f.addShelf);  // on at first
    CHECK(has(logincard::words(L, f, logincard::Show{}, t), "Додати на полицю після входу"));
    const auto tickPixel = [&](const logincard::Fields& fields) {
        panel::Picture p;
        p.width = L.w;
        p.height = L.h;
        p.px.assign(size_t(L.w) * size_t(L.h), 0u);
        logincard::paint(p, L, fields, logincard::Show{});
        return p.px[size_t(L.addTick.y + L.addTick.h / 2) * size_t(L.w) + size_t(L.addTick.x + L.addTick.w / 2)];
    };
    const uint32_t on = tickPixel(f);
    f.addShelf = false;
    const uint32_t off = tickPixel(f);
    CHECK(((on >> 16) & 0xFF) > (on & 0xFF) + 40);    // gold: its red well over its blue
    CHECK(((off >> 16) & 0xFF) <= (off & 0xFF) + 8);  // off: the card's ground through it
}

// 3.0.11-beta.176 (the login concept's «Полиця»): the card for one other login has «‹ Полиця» back at its top left;
// the first launch's card says what the shelf will do (two lines at its foot)
TEST(login_card_back_to_shelf_and_the_first_note) {
    const logincard::Layout plain = logincard::layout(1920, 1080, 1.0f);
    CHECK(plain.back.w == 0 && plain.note.h == 0);
    logincard::Texts t = texts();
    t.shelf = "‹ Полиця";
    logincard::Fields f;
    f.shelfBack = true;
    const logincard::Layout L = logincard::layout(1920, 1080, 1.0f, logincard::Options{});
    CHECK(L.back.w == 0);  // the layout's: given by the fields
    logincard::Options back;
    back.back = true;
    const logincard::Layout B = logincard::layout(1920, 1080, 1.0f, back);
    CHECK(B.back.w > 0 && B.back.y < B.title.y + B.title.h && B.back.x < B.title.x + B.title.w / 2);
    CHECK(logincard::hitTest(B, B.back.x + 4, B.back.y + B.back.h / 2) == logincard::Part::Shelf);
    CHECK(has(logincard::words(B, f, logincard::Show{}, t), "‹ Полиця"));
    logincard::Options note;
    note.note = true;
    const logincard::Layout N = logincard::layout(1920, 1080, 1.0f, note);
    CHECK(N.note.h > 0 && N.note.y >= N.link.y + N.link.h - 0.5f && N.h > plain.h);
    CHECK(N.y == plain.y);  // the top kept: it grows downward
    logincard::Fields first;
    first.note = "Полиця ще порожня: після входу акаунт з'явиться на ній, далі — вхід одним кліком";
    const auto w = logincard::words(N, first, logincard::Show{}, t);
    int lines = 0;
    for (const logincard::Words& x : w)
        if (x.box.y >= N.note.y - 0.5f && x.rgb == 0x9AA3B5) {
            ++lines;
            CHECK(panel::wordsWidth(x.text, x.px, false) <= N.note.w + 0.5f);
        }
    CHECK(lines == 2);
}

// 3.0.11-beta.177 (the login concept's «Класичний»): ▾ at the account field's right end (the saved logins, when the
// shelf has them), the eye at the password's (its letters while held, else dots), «Створити акаунт» and «Забули
// пароль?» under the buttons; the saved logins' list under the account's field
TEST(login_card_dropdown_and_eye) {
    logincard::Options o;
    o.drop = true;
    const logincard::Layout L = logincard::layout(1920, 1080, 1.0f, o);
    const auto mid = [](const panel::Box& b) { return std::pair<float, float>{b.x + b.w / 2, b.y + b.h / 2}; };
    CHECK(L.drop.w > 0 && L.drop.x > L.account.x + L.account.w / 2);
    CHECK(L.drop.x + L.drop.w <= L.account.x + L.account.w + 0.5f && L.drop.y >= L.account.y - 0.5f);
    CHECK(L.eye.w > 0 && L.eye.x > L.password.x + L.password.w / 2);
    CHECK(L.eye.x + L.eye.w <= L.password.x + L.password.w + 0.5f && L.eye.y >= L.password.y - 0.5f);
    CHECK(logincard::hitTest(L, mid(L.drop).first, mid(L.drop).second) == logincard::Part::Drop);
    CHECK(logincard::hitTest(L, mid(L.eye).first, mid(L.eye).second) == logincard::Part::Eye);
    CHECK(logincard::layout(1920, 1080, 1.0f).drop.w == 0);  // no saved logins: no ▾
    CHECK(L.link.x < L.forgot.x && std::abs(L.link.y - L.forgot.y) < 0.5f && L.forgot.y > L.enter.y);
    CHECK(logincard::hitTest(L, mid(L.forgot).first, mid(L.forgot).second) == logincard::Part::Forgot);
    CHECK(logincard::hitTest(L, mid(L.link).first, mid(L.link).second) == logincard::Part::Register);
    logincard::Texts t = texts();
    t.forgot = "Забули пароль?";
    logincard::Fields f;
    f.passwordLength = 4;
    CHECK(has(logincard::words(L, f, logincard::Show{}, t), "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2"));
    CHECK(has(logincard::words(L, f, logincard::Show{}, t), "Забули пароль?"));
    f.passwordShown = "abcd";  // the eye held
    CHECK(has(logincard::words(L, f, logincard::Show{}, t), "abcd"));
    const std::vector<logincard::DropRow> rows{{"kweb", "Raphael · High Priest 99"}, {"kweb_shop", ""}};
    const logincard::DropLayout D = logincard::dropLayout(L, rows.size());
    CHECK(D.rows.size() == 3);  // the two and «+ Інший логін»
    CHECK(D.y >= L.account.y + L.account.h - 0.5f && std::abs(D.x - L.account.x) < 0.5f);
    CHECK(logincard::dropHit(D, D.rows[1].x + 5, D.rows[1].y + D.rows[1].h / 2) == 1);
    CHECK(logincard::dropHit(D, D.x - 5, D.y) == -1);
    const auto dw = logincard::dropWords(D, rows, -1, "+ Інший логін", 1.0f);
    CHECK(has(dw, "kweb") && has(dw, "Raphael · High Priest 99") && has(dw, "kweb_shop") && has(dw, "+ Інший логін"));
}
