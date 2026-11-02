// 3.0.11-beta.172 (the login concept: hints before a mistake): Caps Lock and the keyboard's layout under the login
// card's fields (core/login_hints) - the game's logins are Latin only (the site's rule), so Cyrillic there is the
// layout; the same keys in Latin offered
#include <string>

#include "core/login_hints.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(login_hints_layout_and_caps) {
    CHECK(loginhint::latinGuess("лцуи") == "kweb");
    CHECK(loginhint::latinGuess("ЛЦУИ") == "KWEB");
    CHECK(loginhint::latinGuess("kweb_1") == "kweb_1");
    CHECK(loginhint::latinGuess("кщк") == "ror");
    CHECK(loginhint::latinGuess("рщтуі") == "hones");
    CHECK(loginhint::forAccount("лцуи", false).kind == loginhint::Kind::Layout);
    CHECK(loginhint::forAccount("лцуи", false).guess == "kweb");
    CHECK(loginhint::forAccount("kweb", true).kind == loginhint::Kind::CapsLock);
    CHECK(loginhint::forAccount("kweb", false).kind == loginhint::Kind::None);
    CHECK(loginhint::forPassword(true).kind == loginhint::Kind::CapsLock);
    CHECK(loginhint::forPassword(false).kind == loginhint::Kind::None);
}
