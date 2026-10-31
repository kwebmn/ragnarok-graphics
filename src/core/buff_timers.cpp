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

const char* name(uint16_t icon) {
    for (size_t i = 0; i < kSkillBuffCount; ++i)
        if (kSkillBuffs[i].icon == icon) return kSkillBuffs[i].name;
    for (size_t i = 0; i < kItemBuffCount; ++i)
        if (kItemBuffs[i].icon == icon) return kItemBuffs[i].name;
    return "";
}

namespace {

bool givenBy(uint16_t icon, uint16_t skill) {  // the skill gives this icon
    for (size_t i = 0; i < kSkillBuffCount; ++i)
        if (kSkillBuffs[i].icon == icon && kSkillBuffs[i].skill == skill) return true;
    return false;
}

}  // namespace

bool gives(uint16_t skill, uint16_t icon) { return skill && givenBy(icon, skill); }

void Timers::cast(uint16_t skill, uint16_t level, uint64_t now) {
    last_ = Cast{true, skill, level, now};
    for (const auto& [icon, at] : onAt_) {  // 3.0.4-beta.36: its icon put on just before - the server's order
        if (now < at || now - at > kCastWindowMs || !givenBy(icon, skill)) continue;
        if (const uint32_t ms = duration(icon, skill, level)) ends_[icon] = at + ms;
    }
}

void Timers::icon(uint16_t icon, bool on, uint64_t now) {
    if (!on) {
        ends_.erase(icon);
        onAt_.erase(icon);
        return;
    }
    onAt_[icon] = now;
    const bool fresh = last_.known && now >= last_.at && now - last_.at <= kCastWindowMs;
    uint32_t ms = 0;
    if (fresh) ms = givenBy(icon, last_.skill) ? duration(icon, last_.skill, last_.level) : 0;
    const auto running = ends_.find(icon);
    if (!ms && running != ends_.end() && running->second > now) return;  // sent again: its time goes on
    if (!ms) ms = duration(icon, 0, 0);
    if (ms) ends_[icon] = now + ms;
}

uint32_t Timers::left(uint16_t icon, uint64_t now) const {
    const auto it = ends_.find(icon);
    return it == ends_.end() || it->second <= now ? 0u : uint32_t(it->second - now);
}

std::vector<Timers::Running> Timers::running() const {
    std::vector<Running> out;
    for (const auto& [icon, end] : ends_) {
        const auto on = onAt_.find(icon);
        out.push_back(Running{icon, on != onAt_.end() && on->second <= end ? on->second : end, end});
    }
    return out;
}

std::vector<uint16_t> Ending::due(const Timers& t, uint64_t now, uint32_t withinMs) {
    std::vector<uint16_t> out;
    std::map<uint16_t, uint64_t> still;
    for (const Timers::Running& r : t.running()) {
        if (r.end <= now) continue;
        const auto told = told_.find(r.icon);
        if (told != told_.end() && told->second == r.end) {
            still[r.icon] = r.end;
            continue;
        }
        const uint64_t whole = r.end - r.from;
        if (r.end - now > withinMs || whole < kLeastMs || whole < 2 * uint64_t(withinMs)) continue;
        out.push_back(r.icon);
        still[r.icon] = r.end;
    }
    told_.swap(still);
    return out;
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
