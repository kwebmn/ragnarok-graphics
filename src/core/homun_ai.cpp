#include "core/homun_ai.hpp"

namespace uagfx::homunai {
namespace {

#include "core/ai_lua.inc"

const char* skipNewline(const char* s) { return *s == '\n' ? s + 1 : s; }

}  // namespace

const char* aiLua() { return skipNewline(kAiLua); }
const char* aiMLua() { return skipNewline(kAiMLua); }

std::string config(const Choices& c) {
    return std::string(kMarker) + " - its choices, written by uaRO Graphics («Інтерфейс»)\n" +
           "UARO_AGGRESSIVE = " + (c.aggressive ? "1" : "0") + "\n" + "UARO_DEFEND = " + (c.defend ? "1" : "0") +
           "\n" + "UARO_SKILLS = " + (c.skills ? "1" : "0") + "\n" + "UARO_FOLLOW = " + std::to_string(c.follow) +
           "\n" + "UARO_HEAL_AT = " + std::to_string(c.healAt) + "\n" +
           "UARO_SKILL_LEVEL = " + std::to_string(c.skillLevel) + "\n";
}

bool ours(const std::string& existing) {
    return existing.empty() || existing.compare(0, std::string(kMarker).size(), kMarker) == 0;
}

}  // namespace uagfx::homunai
