#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pad/core/i18n.hpp"
#include "pad/core/logic/controller.hpp"

namespace uapad {

// 0.7: the line shown over the game while an NPC window takes the buttons (UTF-8), with the controller's own button
// names (Cross and L1 on a PlayStation one); "" for no window.
// 3.0.11-beta.258: `digit` - the number field's place value the D-pad changes (1, 10, 100 ...)
std::string npcHint(NpcDialog d, Lang lang, int digit = 1);
// 3.0.11-beta.85: the line while a support skill asks whom; `key`: the skill's own button (on the character)
std::string choiceHint(uint16_t key, Lang lang);

// 3.0.11-beta.93: the line while a window B closes is open (the big map, the game's equipment, skills, options, party)
std::string windowHint(Lang lang);
// 3.0.11-beta.145: the line while a ground skill is aimed; `key`: its own button (let go: cast)
std::string aimHint(uint16_t key, Lang lang);

// 3.0.11-beta.92 (the concept «Контроллер в интерфейсе uaRO», stage 6): the same lines as parts - each its words
// before the buttons («утримати»), its buttons (PadButton bits, the arrows too), its words after - for the interface's
// own hint bar to draw the buttons as the controller's glyphs
struct HintPart {
    std::string before;
    std::vector<uint16_t> buttons;
    std::string words;
};
std::vector<HintPart> hintParts(const std::string& line, uint16_t key = 0);  // a line with {A}, {B}, ↑ ↓ ...
std::vector<HintPart> npcHintParts(NpcDialog d, Lang lang, int digit = 1);
std::vector<HintPart> choiceHintParts(uint16_t key, Lang lang);
std::vector<HintPart> windowHintParts(Lang lang);
std::vector<HintPart> aimHintParts(uint16_t key, Lang lang);

// 3.0.11-beta.179 (the login concept: the pad on the login screen): which of the login screen's lines
enum class LoginHint : uint8_t { None, Shelf, Menu, Card, CardBack, Continue, ExitAsk };
std::string loginHint(LoginHint h, Lang lang);
std::vector<HintPart> loginHintParts(LoginHint h, Lang lang);

}  // namespace uapad
