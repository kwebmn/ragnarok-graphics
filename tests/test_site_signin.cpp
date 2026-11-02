// 3.0.11-beta.180 (the login concept: the site account in the client): the site sign-in card (core/site_signin) - its
// three ways (the site's password, a code in the browser, the cabinet's key), each way's fields and button, the code
// big while it waits, the mouse on each part
#include <algorithm>
#include <cmath>
#include <string>

#include "core/site_signin.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
sitesignin::Texts texts() {
    sitesignin::Texts t;
    t.title = "Акаунт сайту";
    t.subtitle = "Усі ваші ігрові логіни одним входом";
    t.ways[0] = "Пароль сайту";
    t.ways[1] = "Код у браузері";
    t.ways[2] = "Ключ із кабінету";
    t.email = "E-mail";
    t.password = "Пароль сайту";
    t.code2fa = "Код двофакторної перевірки";
    t.key = "Ключ";
    t.signIn = "Увійти";
    t.startBrowser = "Підтвердити в браузері";
    t.waiting = "Чекаємо підтвердження… {1} с";
    t.back = "‹ Назад";
    return t;
}
bool has(const std::vector<sitesignin::Words>& w, const std::string& s) {
    return std::any_of(w.begin(), w.end(), [&](const sitesignin::Words& x) { return x.text == s; });
}
}  // namespace

TEST(site_signin_ways_and_fields) {
    using P = sitesignin::Part;
    const sitesignin::Layout L = sitesignin::layout(1920, 1080, 1.0f, sitesignin::Way::Password, false);
    CHECK(std::abs(L.x + L.w / 2 - 960) <= 2);
    CHECK(L.email.h > 0 && L.password.h > 0 && L.code.h == 0 && L.key.h == 0 && L.button.y > L.password.y);
    const auto at = [&](const sitesignin::Layout& l, const panel::Box& b) {
        return sitesignin::hitTest(l, b.x + b.w / 2, b.y + b.h / 2);
    };
    CHECK(at(L, L.ways[1]) == P::Way1 && at(L, L.email) == P::Email && at(L, L.password) == P::Password);
    CHECK(at(L, L.button) == P::Button && at(L, L.back) == P::Back);
    const sitesignin::Layout two = sitesignin::layout(1920, 1080, 1.0f, sitesignin::Way::Password, true);
    CHECK(two.code.h > 0 && two.code.y > two.password.y && two.button.y > two.code.y);  // the site asks its code
    const sitesignin::Layout key = sitesignin::layout(1920, 1080, 1.0f, sitesignin::Way::Key, false);
    CHECK(key.key.h > 0 && key.email.h == 0 && at(key, key.key) == P::Key);
    const sitesignin::Layout web = sitesignin::layout(1920, 1080, 1.0f, sitesignin::Way::Browser, false);
    CHECK(web.email.h == 0 && web.codeBig.h > 0 && web.button.h > 0);
}

TEST(site_signin_words) {
    const sitesignin::Texts t = texts();
    sitesignin::Show sh;
    sh.way = sitesignin::Way::Browser;
    sh.userCode = "ABCD-EFGH";
    sh.secondsLeft = 540;
    const sitesignin::Layout L = sitesignin::layout(1920, 1080, 1.0f, sh.way, false);
    const auto w = sitesignin::words(L, sh, t);
    CHECK(has(w, "ABCD-EFGH") && has(w, "Чекаємо підтвердження… 540 с") && has(w, "Код у браузері"));
    sh.way = sitesignin::Way::Password;
    sh.email = "me@x.ua";
    sh.passwordLength = 3;
    sh.error = "Невірний e-mail або пароль";
    const sitesignin::Layout P = sitesignin::layout(1920, 1080, 1.0f, sh.way, false);
    const auto pw = sitesignin::words(P, sh, t);
    CHECK(has(pw, "me@x.ua") && has(pw, "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2"));
    CHECK(has(pw, "Невірний e-mail або пароль"));
    CHECK(!has(pw, "ABCD-EFGH"));  // the browser's code only on its way
}

// 3.0.11-beta.181: the site account's forms in the same card - a new game login (its login, password and sex), an
// existing one linked (its login and password), a game password changed (the current and the new) - no ways' tabs
TEST(site_signin_forms) {
    using P = sitesignin::Part;
    const auto at = [](const sitesignin::Layout& l, const panel::Box& b) {
        return sitesignin::hitTest(l, b.x + b.w / 2, b.y + b.h / 2);
    };
    const sitesignin::Layout n = sitesignin::layout(1920, 1080, 1.0f, sitesignin::Way::NewLogin, false);
    CHECK(n.ways[0].w == 0 && n.login.h > 0 && n.gamePass.h > 0 && n.sex[0].w > 0 && n.sex[1].x > n.sex[0].x);
    CHECK(at(n, n.login) == P::Login && at(n, n.gamePass) == P::GamePass && at(n, n.sex[1]) == P::Sex1);
    CHECK(n.button.y > n.sex[0].y && n.email.h == 0);
    const sitesignin::Layout k = sitesignin::layout(1920, 1080, 1.0f, sitesignin::Way::Link, false);
    CHECK(k.login.h > 0 && k.gamePass.h > 0 && k.sex[0].w == 0);
    const sitesignin::Layout c = sitesignin::layout(1920, 1080, 1.0f, sitesignin::Way::GamePassword, false);
    CHECK(c.gamePass.h > 0 && c.newPass.h > 0 && c.login.h == 0 && at(c, c.newPass) == P::NewPass);
    sitesignin::Texts t;
    t.forms[0] = "Новий ігровий логін";
    t.login = "Логін";
    t.sexes[0] = "Чоловічий";
    t.sexes[1] = "Жіночий";
    t.create = "Створити логін";
    sitesignin::Show sh;
    sh.way = sitesignin::Way::NewLogin;
    sh.login = "kweb2";
    sh.sex = 'F';
    const auto w = sitesignin::words(n, sh, t);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const sitesignin::Words& x) { return x.text == s; });
    };
    CHECK(has("Новий ігровий логін") && has("kweb2") && has("Жіночий") && has("Створити логін"));
}

// 3.0.11-beta.181 (the registration in the client, one form): «1 Акаунт → 2 Персонаж → 3 У гру» over the login, the
// password twice, the e-mail, the sex, the rules' tick and their link; «вільний» at the login's right once asked;
// «Вже є акаунт на сайті?» under the button
TEST(site_signin_register_form) {
    using P = sitesignin::Part;
    const sitesignin::Layout L = sitesignin::layout(1920, 1080, 1.0f, sitesignin::Way::Register, false);
    CHECK(L.ways[0].w == 0 && L.steps.h > 0 && L.login.h > 0 && L.gamePass.h > 0 && L.newPass.h > 0 && L.email.h > 0);
    CHECK(L.sex[0].w > 0 && L.agree.h > 0 && L.button.y > L.agree.y && L.haveAccount.y > L.button.y);
    CHECK(L.email.y > L.newPass.y && L.newPass.y > L.gamePass.y && L.gamePass.y > L.login.y);
    const auto at = [&](const panel::Box& b) { return sitesignin::hitTest(L, b.x + 3, b.y + b.h / 2); };
    CHECK(at(L.agree) == P::Agree && at(L.rules) == P::Rules && at(L.haveAccount) == P::HaveAccount);
    CHECK(at(L.email) == P::Email && at(L.newPass) == P::NewPass);
    CHECK(L.y + L.h <= 1080 && L.y >= 0);
    sitesignin::Texts t;
    t.regTitle = "Новий акаунт";
    t.steps = "1 Акаунт → 2 Персонаж → 3 У гру";
    t.free = "вільний";
    t.taken = "зайнятий";
    t.agree = "Погоджуюся з правилами сервера";
    sitesignin::Show sh;
    sh.way = sitesignin::Way::Register;
    sh.login = "kweb_new";
    sh.loginFree = 1;
    const auto w = sitesignin::words(L, sh, t);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const sitesignin::Words& x) { return x.text == s; });
    };
    CHECK(has("Новий акаунт") && has("1 Акаунт → 2 Персонаж → 3 У гру") && has("вільний") && !has("зайнятий"));
    CHECK(has("Погоджуюся з правилами сервера"));
}
