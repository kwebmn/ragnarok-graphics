// 3.0.11-beta.181 (the login concept: the registration in the client, one form): the form's rules as the site's
// (core/register_form) - the login Latin, digits, _ and - 4 to 23; the password 8 to 32 and the same twice; an e-mail;
// the sex chosen; the rules agreed. The first refused field first.
#include <string>

#include "core/register_form.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(register_form_rules_are_the_sites) {
    regform::Form f{"kweb_new", "secret123", "secret123", "kweb@example.com", 0, true};
    CHECK(regform::check(f).empty());
    auto bad = f;
    bad.login = "кв";
    CHECK(!regform::check(bad).empty() && regform::check(bad)[0].first == regform::Field::Login);
    bad = f;
    bad.password = bad.password2 = "short";
    CHECK(regform::check(bad)[0].first == regform::Field::Password);
    bad = f;
    bad.password2 = "secret124";
    CHECK(regform::check(bad)[0].first == regform::Field::Password2);
    bad = f;
    bad.login = std::string(24, 'a');
    CHECK(regform::check(bad)[0].first == regform::Field::Login);
    bad = f;
    bad.agree = false;
    CHECK(regform::check(bad)[0].first == regform::Field::Agree);
    bad = f;
    bad.email = "kweb@example";
    CHECK(regform::check(bad)[0].first == regform::Field::Email);
    bad = f;
    bad.sex = -1;
    CHECK(regform::check(bad)[0].first == regform::Field::Sex);
    bad = f;
    bad.password = bad.password2 = std::string(33, 'x');
    CHECK(regform::check(bad)[0].first == regform::Field::Password);
    bad.login = "a";
    CHECK(regform::check(bad).size() == 2);  // every refused field, in the form's order
}
