#include "core/buff_timers.hpp"

#include <algorithm>
#include <cstdio>

#include "core/buff_table.hpp"

namespace uagfx::buffs {

uint32_t duration(uint16_t icon, uint16_t skill, uint16_t level) {
    uint32_t longest = 0;
    for (size_t i = 0; i < kSkillBuffCount; ++i) {
        const SkillBuff& b = kSkillBuffs[i];
        if (b.icon != icon) continue;
        if (skill && b.skill == skill && level >= 1) return b.ms[std::min<uint16_t>(level, 10) - 1];
        longest = std::max(longest, b.ms[9]);
    }
    for (size_t i = 0; i < kItemBuffCount; ++i)
        if (kItemBuffs[i].icon == icon) longest = std::max(longest, kItemBuffs[i].ms);
    return longest;
}

void Timers::cast(uint16_t skill, uint16_t level, uint64_t now) { last_ = Cast{true, skill, level, now}; }

void Timers::icon(uint16_t icon, bool on, uint64_t now) {
    if (!on) {
        ends_.erase(icon);
        return;
    }
    const bool fresh = last_.known && now >= last_.at && now - last_.at <= kCastWindowMs;
    uint32_t ms = 0;
    if (fresh) {
        ms = duration(icon, last_.skill, last_.level);
        bool its = false;  // the cast was of a skill that gives this icon
        for (size_t i = 0; i < kSkillBuffCount && !its; ++i)
            its = kSkillBuffs[i].icon == icon && kSkillBuffs[i].skill == last_.skill;
        if (!its) ms = 0;
    }
    const auto running = ends_.find(icon);
    if (!ms && running != ends_.end() && running->second > now) return;  // sent again: its time goes on
    if (!ms) ms = duration(icon, 0, 0);
    if (ms) ends_[icon] = now + ms;
}

uint32_t Timers::left(uint16_t icon, uint64_t now) const {
    const auto it = ends_.find(icon);
    return it == ends_.end() || it->second <= now ? 0u : uint32_t(it->second - now);
}

std::string label(uint32_t ms) {
    if (ms == 0) return "";
    const uint32_t s = (ms + 999) / 1000;
    char buf[16];
    if (s < 60)
        std::snprintf(buf, sizeof buf, "%u", s);
    else if (s < 600)
        std::snprintf(buf, sizeof buf, "%u:%02u", s / 60, s % 60);
    else
        std::snprintf(buf, sizeof buf, "%um", s / 60);
    return buf;
}

}  // namespace uagfx::buffs
