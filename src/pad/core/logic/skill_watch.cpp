#include "pad/core/logic/skill_watch.hpp"

namespace uapad {

std::optional<std::string> SkillWatch::line(std::string text) {
    if (lines_ >= kMostLines) return std::nullopt;
    ++lines_;
    return text;
}

std::optional<std::string> SkillWatch::asked(uint16_t skill, const std::string& name, uint16_t lv, uint32_t target,
                                             int cells, int range, uint32_t nowMs) {
    waiting_ = true;
    skill_ = skill;
    sentMs_ = nowMs;
    label_ = (name.empty() ? "skill " + std::to_string(skill) : name) + " lv " + std::to_string(lv);
    std::string text = "skill on a target: the client asks for " + label_ + " on " + std::to_string(target) + " - ";
    text += cells >= 0 ? std::to_string(cells) + " cells apart by the server's walks" : "apart unknown";
    if (range > 0) text += " (its range " + std::to_string(range) + ")";
    return line(text);
}

std::optional<std::string> SkillWatch::answer(const char* what, uint32_t nowMs) {
    waiting_ = false;
    return line("skill on a target: " + label_ + " - the server " + what + " in " + std::to_string(nowMs - sentMs_) +
                " ms");
}

std::optional<std::string> SkillWatch::used(uint16_t skill, uint32_t src, uint32_t nowMs) {
    if (!waiting_ || skill != skill_ || src != self_) return std::nullopt;
    return answer("used it", nowMs);
}

std::optional<std::string> SkillWatch::castBegun(uint16_t skill, uint32_t src, uint32_t nowMs) {
    if (!waiting_ || skill != skill_ || src != self_) return std::nullopt;
    return answer("began its cast", nowMs);
}

std::optional<std::string> SkillWatch::refused(uint16_t skill, int cause, uint32_t nowMs) {
    if (!waiting_ || skill != skill_) return std::nullopt;
    const std::string what = "refused it (cause " + std::to_string(cause) + ")";
    return answer(what.c_str(), nowMs);
}

std::optional<std::string> SkillWatch::tick(uint32_t nowMs) {
    if (!waiting_ || nowMs - sentMs_ < kWaitMs) return std::nullopt;
    waiting_ = false;
    return line("skill on a target: " + label_ + " - no answer from the server in " + std::to_string(kWaitMs) +
                " ms (out of its range then, or not usable)");
}

}  // namespace uapad
