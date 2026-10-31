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

std::string choiceHint(uint16_t key, Lang lang) {
    std::string s = tr(lang, Text::HintChoose);
    put(s, "{A}", label(kPadA));
    put(s, "{B}", label(kPadB));
    put(s, "{KEY}", label(key));
    return s;
}

std::string windowHint(Lang lang) {
    std::string s = tr(lang, Text::HintWindow);
    put(s, "{B}", label(kPadB));
    return s;
}

std::vector<HintPart> hintParts(const std::string& line, uint16_t key) {
    std::vector<HintPart> out;
    const std::string dot = "  \xC2\xB7  ";
    for (size_t from = 0; from <= line.size();) {
        size_t end = line.find(dot, from);
        if (end == std::string::npos) end = line.size();
        const std::string seg = line.substr(from, end - from);
        from = end + dot.size();
        if (seg.empty()) continue;
        HintPart part;
        const size_t colon = seg.find(": ");
        if (colon == std::string::npos) {
            part.words = seg;
            out.push_back(part);
            continue;
        }
        part.words = seg.substr(colon + 2);
        const std::string keys = seg.substr(0, colon);
        for (size_t at = 0; at < keys.size();) {
            size_t space = keys.find(' ', at);
            if (space == std::string::npos) space = keys.size();
            const std::string t = keys.substr(at, space - at);
            at = space + 1;
            const uint16_t b = t == "{A}"    ? uint16_t(kPadA)
                               : t == "{B}"  ? uint16_t(kPadB)
                               : t == "{LB}" ? uint16_t(kPadLB)
                               : t == "{RB}" ? uint16_t(kPadRB)
                               : t == "{KEY}" ? key
                               : t == "\xE2\x86\x91" ? uint16_t(kPadUp)     // ↑
                               : t == "\xE2\x86\x93" ? uint16_t(kPadDown)   // ↓
                               : t == "\xE2\x86\x90" ? uint16_t(kPadLeft)   // ←
                               : t == "\xE2\x86\x92" ? uint16_t(kPadRight)  // →
                                                        : uint16_t(0);
            if (b)
                part.buttons.push_back(b);
            else if (!t.empty() && t != "/")
                part.before += (part.before.empty() ? "" : " ") + t;
        }
        out.push_back(part);
        if (end == line.size()) break;
    }
    return out;
}

std::vector<HintPart> npcHintParts(NpcDialog d, Lang lang) {
    Text t = Text::HintNext;
    switch (d) {
        case NpcDialog::None: return {};
        case NpcDialog::Next: t = Text::HintNext; break;
        case NpcDialog::Close: t = Text::HintClose; break;
        case NpcDialog::Waiting: t = Text::HintWait; break;
        case NpcDialog::Menu: t = Text::HintMenu; break;
        case NpcDialog::Number: t = Text::HintNumber; break;
    }
    return hintParts(tr(lang, t));
}

std::vector<HintPart> choiceHintParts(uint16_t key, Lang lang) { return hintParts(tr(lang, Text::HintChoose), key); }

std::vector<HintPart> windowHintParts(Lang lang) { return hintParts(tr(lang, Text::HintWindow)); }

}  // namespace uapad
