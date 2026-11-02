#pragma once
#include <string>
#include <utility>
#include <vector>

#include "core/settings/strings.hpp"

namespace uagfx::regform {

// 3.0.11-beta.181 (the login concept: the registration in the client, one form - the site account and its first game
// login, one password): the form's rules as the site's (/api/client/register) - checked before it is sent, every
// refused field with its words, in the form's order.
enum class Field : uint8_t { Login, Password, Password2, Email, Sex, Agree };
struct Form {
    std::string login, password, password2, email;  // UTF-8
    int sex = -1;  // 0 male, 1 female, -1 not chosen
    bool agree = false;
};
std::vector<std::pair<Field, settings::Text>> check(const Form& f);

}  // namespace uagfx::regform
