#include "pad/core/logic/npc_hint.hpp"

#include "pad/core/config/bindings.hpp"

namespace uapad {
namespace {

std::string label(uint16_t button) {
    for (size_t i = 0; i < kBindableCount; ++i)
        if (kBindableButtons[i] == button) return buttonLabel(i);
    return "";
}

void put(std::string& s, const std::string& key, const std::string& value) {
    for (size_t at = s.find(key); at != std::string::npos; at = s.find(key, at + value.size()))
        s.replace(at, key.size(), value);
}

}  // namespace

std::string npcHint(NpcDialog d, Lang lang) {
    Text t = Text::HintNext;
    switch (d) {
        case NpcDialog::None: return "";
        case NpcDialog::Next: t = Text::HintNext; break;
        case NpcDialog::Close: t = Text::HintClose; break;
        case NpcDialog::Waiting: t = Text::HintWait; break;
        case NpcDialog::Menu: t = Text::HintMenu; break;
        case NpcDialog::Number: t = Text::HintNumber; break;
    }
    std::string s = tr(lang, t);
    put(s, "{A}", label(kPadA));
    put(s, "{B}", label(kPadB));
    put(s, "{LB}", label(kPadLB));
    put(s, "{RB}", label(kPadRB));
    return s;
}

}  // namespace uapad
