#pragma once
#include <string>

#include "pad/core/i18n.hpp"
#include "pad/core/logic/controller.hpp"

namespace uapad {

// 0.7: the line shown over the game while an NPC window takes the buttons (UTF-8), with the controller's own button
// names (Cross and L1 on a PlayStation one); "" for no window.
std::string npcHint(NpcDialog d, Lang lang);
// 3.0.11-beta.85: the line while a support skill asks whom; `key`: the skill's own button (on the character)
std::string choiceHint(uint16_t key, Lang lang);

}  // namespace uapad
