#pragma once
#include <cstdint>
#include <string>

namespace uagfx::loginhint {

// 3.0.11-beta.172 (the login concept: hints before a mistake): what the login card says under a field while the
// player types - Caps Lock on, or Cyrillic in the login (the game's logins are Latin only: the keyboard's layout)
// with the same keys in Latin offered
enum class Kind : uint8_t { None, CapsLock, Layout };
struct Hint {
    Kind kind = Kind::None;
    std::string guess;  // the login as the same keys type it in Latin (Layout)
};
Hint forAccount(const std::string& utf8, bool caps);
Hint forPassword(bool caps);
// Ukrainian and Russian letters to the keys they sit on in Latin (QWERTY); anything else kept
std::string latinGuess(const std::string& utf8);

}  // namespace uagfx::loginhint
