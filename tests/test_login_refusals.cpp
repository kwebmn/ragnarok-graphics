// 3.0.11-beta.172 (the login concept: the server's answer in words): the login refusals (0x006a) the server sends -
// which field they are about, the card's own words, what to do (core/login_refusals)
#include <string>

#include "core/login_refusals.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(login_refusals_codes_to_fields) {
    using F = loginerr::Field;
    CHECK(loginerr::refusal(0).field == F::Account && loginerr::refusal(0).inlineOnly);
    CHECK(loginerr::refusal(0).action == loginerr::Action::CreateAccount);
    CHECK(loginerr::refusal(1).field == F::Password && loginerr::refusal(1).inlineOnly);
    CHECK(loginerr::refusal(1).action == loginerr::Action::ResetPassword);
    CHECK(loginerr::refusal(2).text == settings::Text::LoginErrExpired && !loginerr::refusal(2).inlineOnly);
    CHECK(loginerr::refusal(4).text == settings::Text::LoginErrMaintenance);
    CHECK(loginerr::refusal(5).text == settings::Text::LoginErrOldExe);
    CHECK(loginerr::refusal(6).text == settings::Text::LoginErrBannedUntil);
    CHECK(loginerr::refusal(7).text == settings::Text::LoginErrJammed);
    CHECK(loginerr::refusal(99).text == settings::Text::LoginErrRemoved);
    CHECK(loginerr::refusal(3).text == settings::Text::LoginErrRejected);  // any other code
    CHECK(loginerr::refusal(3).field == F::None && !loginerr::refusal(3).inlineOnly);
}

TEST(login_refusals_words_name_the_login) {
    const std::string w = loginerr::words(loginerr::refusal(0), settings::Lang::Uk, "kwbe", "");
    CHECK(w.find("kwbe") != std::string::npos);
    CHECK(loginerr::words(loginerr::refusal(6), settings::Lang::En, "x", "2026-11-12 10:00").find("2026-11-12") !=
          std::string::npos);
    CHECK(loginerr::words(loginerr::refusal(4), settings::Lang::Ru, "x", "").find("x") == std::string::npos);
}
