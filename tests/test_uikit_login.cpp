#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/login_live.hpp"

using namespace uagfx;
namespace lc = uagfx::logincard;
namespace ll = uagfx::uikit::login;

namespace {
panel::Box mid(const panel::Box& b) { return panel::Box{b.x + b.w / 2, b.y + b.h / 2, 0, 0}; }
lc::Texts texts() {
    lc::Texts t;
    t.title = "uaRO";
    t.subtitle = "Вхід у гру";
    t.account = "Логін";
    t.password = "Пароль";
    t.remember = "Запам'ятати логін";
    t.addShelf = "Додати на полицю після входу";
    t.enter = "Увійти";
    t.exit = "Вийти";
    t.register_ = "Створити акаунт";
    t.forgot = "Забули пароль?";
    t.resetPassword = "Скинути пароль";
    return t;
}
}  // namespace

// 3.0.18-beta.107: the login card laid out by RmlUi; its boxes into core logincard's layout BY NAME - the account's
// field is the one above, the password's under it; every part still hit where it is drawn
TEST(uikit_login_fields_by_name_and_every_part_hit) {
    lc::Fields f;
    f.account = "kweb";
    f.passwordLength = 6;
    lc::Show sh;
    lc::Options o;
    o.drop = true;
    static ll::Model m;
    lc::Layout L = lc::layout(1920, 1080, 1, o);
    ll::fill(m, L, o, f, sh, texts());
    CHECK_EQ(m.secret, std::string("\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2"));
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(ll::rml(), [&](Rml::DataModelConstructor& c) { ll::bind(c, m); }, 770, 300, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    const lc::Layout core = L;
    CHECK(ll::refit(d, L));
    CHECK(std::abs(float(L.w) - 380) < 2.5f);
    CHECK(L.account.y < L.password.y);  // the roles by name, as drawn
    CHECK(std::abs(L.account.y - core.account.y) < 3 && std::abs(L.password.y - core.password.y) < 3);
    CHECK(lc::hitTest(L, L.account.x + 20, mid(L.account).y) == lc::Part::Account);
    CHECK(lc::hitTest(L, L.password.x + 20, mid(L.password).y) == lc::Part::Password);
    CHECK(lc::hitTest(L, mid(L.drop).x, mid(L.drop).y) == lc::Part::Drop);
    CHECK(lc::hitTest(L, mid(L.eye).x, mid(L.eye).y) == lc::Part::Eye);
    CHECK(lc::hitTest(L, mid(L.remember).x, mid(L.remember).y) == lc::Part::Remember);
    CHECK(lc::hitTest(L, mid(L.addShelf).x, mid(L.addShelf).y) == lc::Part::AddShelf);
    CHECK(lc::hitTest(L, mid(L.enter).x, mid(L.enter).y) == lc::Part::Enter);
    CHECK(lc::hitTest(L, mid(L.exit).x, mid(L.exit).y) == lc::Part::Exit);
    CHECK(lc::hitTest(L, mid(L.link).x, mid(L.link).y) == lc::Part::Register);
    CHECK(lc::hitTest(L, mid(L.forgot).x, mid(L.forgot).y) == lc::Part::Forgot);
    CHECK(L.enter.x < L.exit.x);
    // a refusal of the password: its line under the password's field, the card taller, the account's field kept
    f.error = "Невірний пароль";
    f.errorField = loginerr::Field::Password;
    f.errorAction = loginerr::Action::ResetPassword;
    o.errorAt = loginerr::Field::Password;
    L = lc::layout(1920, 1080, 1, o);
    ll::fill(m, L, o, f, sh, texts());
    rmlhost::update();
    const int before = int(core.h);
    CHECK(ll::refit(d, L));
    CHECK(L.h > before);
    CHECK(L.error.y > L.password.y && L.errorAction.y > L.error.y);
    CHECK(lc::hitTest(L, L.errorAction.x + 10, mid(L.errorAction).y) == lc::Part::ErrorAction);
    CHECK(L.remember.y > L.errorAction.y);
    rmlhost::close(d);
}
