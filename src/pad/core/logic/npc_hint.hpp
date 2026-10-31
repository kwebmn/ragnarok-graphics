#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pad/core/i18n.hpp"
#include "pad/core/logic/controller.hpp"

namespace uapad {

// 0.7: the line shown over the game while an NPC window takes the buttons (UTF-8), with the controller's own button
// names (Cross and L1 on a PlayStation one); "" for no window.
std::string npcHint(NpcDialog d, Lang lang);
// 3.0.11-beta.85: the line while a support skill asks whom; `key`: the skill's own button (on the character)
std::string choiceHint(uint16_t key, Lang lang);

// 3.0.11-beta.92 (the concept «Контроллер в интерфейсе uaRO», stage 6): the same lines as parts - each its words
// before the buttons («утримати»), its buttons (PadButton bits, the arrows too), its words after - for the interface's
// own hint bar to draw the buttons as the controller's glyphs
struct HintPart {
    std::string before;
    std::vector<uint16_t> buttons;
    std::string words;
};
std::vector<HintPart> hintParts(const std::string& line, uint16_t key = 0);  // a line with {A}, {B}, ↑ ↓ ...
std::vector<HintPart> npcHintParts(NpcDialog d, Lang lang);
std::vector<HintPart> choiceHintParts(uint16_t key, Lang lang);

}  // namespace uapad
