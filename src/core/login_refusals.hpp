#pragma once
#include <cstdint>
#include <string>

#include "core/settings/strings.hpp"

namespace uagfx::loginerr {

// 3.0.11-beta.172 (the login concept: the server's answer in words): the server's refusals of a login (0x006a, its
// code at +2; uaRO.exe FUN_005A40C0 shows the msgstringtable lines 6, 0x10B, 8, 0x10A, 0x136, 0x1C1, 0x1B7, 0x170,
// else 9 in English) as the card's own words - under the field they are about, with what to do. The two common ones
// only there: the game's box answered unseen.
enum class Field : uint8_t { None, Account, Password };
enum class Action : uint8_t { None, CreateAccount, ResetPassword };
struct Refusal {
    Field field = Field::None;
    settings::Text text = settings::Text::LoginErrRejected;
    Action action = Action::None;
    bool inlineOnly = false;  // the card shows it; the game's box is answered without being seen
};
Refusal refusal(int code);
// Its words in the language: the login named (an unknown login, a wrong password), the date (a forbidden login)
std::string words(const Refusal& r, settings::Lang lang, const std::string& login, const std::string& until);

}  // namespace uagfx::loginerr
