#include "core/ground_timers.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::groundtimers {

uint16_t skillOfUnit(uint8_t type) {
    switch (type) {  // db/skill_unit_db.txt
        case 0x7e: return 12;   // MG_SAFETYWALL
        case 0x85: return 25;   // AL_PNEUMA
        case 0x83: return 70;   // PR_SANCTUARY
        case 0x90: return 115;  // HT_SKIDTRAP
        case 0x93: return 116;  // HT_LANDMINE
        case 0x91: return 117;  // HT_ANKLESNARE
        case 0x94: return 118;  // HT_SHOCKWAVE
        case 0x95: return 119;  // HT_SANDMAN
        case 0x96: return 120;  // HT_FLASHER
        case 0x97: return 121;  // HT_FREEZINGTRAP
        case 0x8f: return 122;  // HT_BLASTMINE
        case 0x98: return 123;  // HT_CLAYMORETRAP
        case 0x99: return 125;  // HT_TALKIEBOX
        default: return 0;
    }
}

int durationMs(uint16_t skill, int level) {
    const int lv = std::max(level, 1);
    const auto byLevel = [&](std::initializer_list<int> ms) {
        const int n = int(ms.size());
        return *(ms.begin() + std::min(lv, n) - 1);
    };
    switch (skill) {  // db/skill_cast_db.txt Duration1
        case 12: return 5000 * std::min(lv, 10);
        case 25: return 10000;
        case 70: return 1000 + 3000 * std::min(lv, 10);
        case 115: return byLevel({300000, 240000, 180000, 120000, 60000});
        case 116:
        case 118: return byLevel({200000, 160000, 120000, 80000, 40000});
        case 117: return byLevel({250000, 200000, 150000, 100000, 50000});
        case 119:
        case 120:
        case 121: return byLevel({150000, 120000, 90000, 60000, 30000});
        case 122: return byLevel({25000, 20000, 15000, 10000, 5000});
        case 123: return 20000 * std::min(lv, 5);
        case 125: return 600000;
        default: return 0;
    }
}

float Ring::left(uint64_t nowMs) const {
    if (endMs <= startMs || nowMs >= endMs) return 0.0f;
    if (nowMs <= startMs) return 1.0f;
    return float(endMs - nowMs) / float(endMs - startMs);
}

void Timers::asked(uint16_t skill, int level, uint64_t nowMs) {
    if (!durationMs(skill, level)) return;
    asks_.erase(std::remove_if(asks_.begin(), asks_.end(),
                               [&](const Ask& a) { return a.skill == skill || nowMs - a.atMs > kCastWaitMs; }),
                asks_.end());
    asks_.push_back(Ask{skill, level, nowMs});
}

void Timers::seen(const groundunits::Seen& s, uint32_t self, uint64_t nowMs) {
    if (s.caster != self) return;
    const uint16_t skill = skillOfUnit(s.type);
    if (!skill) return;
    for (const Unit& u : units_)
        if (u.id == s.unit) return;  // back in sight
    int cast = 0;
    for (const Cast& c : casts_)
        if (c.skill == skill && nowMs - c.startMs <= kTogetherMs) cast = c.id;
    if (!cast) {
        const auto a = std::find_if(asks_.begin(), asks_.end(), [&](const Ask& k) {
            return k.skill == skill && nowMs >= k.atMs && nowMs - k.atMs <= kCastWaitMs;
        });
        if (a == asks_.end()) return;  // no cast of it just asked: its start not known
        cast = next_++;
        casts_.push_back(Cast{cast, skill, nowMs, nowMs + uint64_t(durationMs(skill, a->level))});
        asks_.erase(a);
    }
    units_.push_back(Unit{s.unit, cast, s.x, s.y});
}

void Timers::gone(uint32_t unit) {
    units_.erase(std::remove_if(units_.begin(), units_.end(), [&](const Unit& u) { return u.id == unit; }),
                 units_.end());
    casts_.erase(std::remove_if(casts_.begin(), casts_.end(),
                                [&](const Cast& c) {
                                    return std::none_of(units_.begin(), units_.end(),
                                                        [&](const Unit& u) { return u.cast == c.id; });
                                }),
                 casts_.end());
}

void Timers::clear() {
    asks_.clear();
    units_.clear();
    casts_.clear();
}

std::vector<Ring> Timers::rings(uint64_t nowMs) const {
    std::vector<Ring> out;
    for (const Cast& c : casts_) {
        if (nowMs >= c.endMs) continue;
        Ring r;
        r.skill = c.skill;
        r.startMs = c.startMs;
        r.endMs = c.endMs;
        int n = 0;
        for (const Unit& u : units_)
            if (u.cast == c.id) {
                r.x += float(u.x) + 0.5f;
                r.y += float(u.y) + 0.5f;
                ++n;
            }
        if (!n) continue;
        r.x /= float(n);
        r.y /= float(n);
        float reach = 0.0f;
        for (const Unit& u : units_)
            if (u.cast == c.id)
                reach = std::max(reach, std::hypot(float(u.x) + 0.5f - r.x, float(u.y) + 0.5f - r.y));
        r.radius = std::max(0.45f, reach + 0.45f);
        out.push_back(r);
    }
    return out;
}

}  // namespace uagfx::groundtimers
