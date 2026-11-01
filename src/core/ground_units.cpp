#include "core/ground_units.hpp"

#include <algorithm>

namespace uagfx::groundunits {
namespace {

constexpr uint64_t kTogetherMs = 150;
constexpr uint16_t kCounted[] = {18, 87, 92, 80};  // Fire Wall, Ice Wall, Quagmire, Fire Pillar

uint32_t rd32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
int rd16(const uint8_t* p) { return int(uint32_t(p[0]) | uint32_t(p[1]) << 8); }

uint16_t skillOf(uint8_t type) {
    for (const uint16_t skill : kCounted)
        for (const uint8_t t : unitTypes(skill))
            if (t == type) return skill;
    return 0;
}

}  // namespace

bool readSeen(const uint8_t* p, size_t n, Seen& out) {
    if (n < 16 || rd16(p) != 0x011F) return false;
    out = Seen{rd32(p + 2), rd32(p + 6), rd16(p + 10), rd16(p + 12), p[14]};
    return true;
}

bool readGone(const uint8_t* p, size_t n, uint32_t& unit) {
    if (n < 6 || rd16(p) != 0x0120) return false;
    unit = rd32(p + 2);
    return true;
}

std::vector<uint8_t> unitTypes(uint16_t skill) {
    switch (skill) {
        case 18: return {0x7F};
        case 87: return {0x8D};
        case 92: return {0x8E};
        case 80: return {0x87, 0x88};
        default: return {};
    }
}

void Tracker::seen(const Seen& s, uint32_t self, uint64_t nowMs) {
    if (s.caster != self || self == 0) return;
    const uint16_t skill = skillOf(s.type);
    if (skill == 0) return;
    if (std::any_of(units_.begin(), units_.end(), [&](const Unit& u) { return u.id == s.unit; })) return;
    auto c = std::find_if(casts_.rbegin(), casts_.rend(), [&](const Cast& k) {
        return k.skill == skill && nowMs >= k.atMs && nowMs - k.atMs <= kTogetherMs;
    });
    int id = 0;
    if (c != casts_.rend()) {
        id = c->id;
        ++c->units;
    } else {
        id = next_++;
        casts_.push_back(Cast{id, skill, nowMs, 1});
    }
    units_.push_back(Unit{s.unit, skill, id});
}

void Tracker::gone(uint32_t unit, uint64_t nowMs) {
    const auto u = std::find_if(units_.begin(), units_.end(), [&](const Unit& x) { return x.id == unit; });
    if (u == units_.end()) return;
    const int cast = u->cast;
    const uint16_t skill = u->skill;
    units_.erase(u);
    const auto c = std::find_if(casts_.begin(), casts_.end(), [&](const Cast& k) { return k.id == cast; });
    if (c == casts_.end() || --c->units > 0) return;
    casts_.erase(c);
    const auto o = std::find_if(out_.begin(), out_.end(), [&](const Out& x) { return x.skill == skill; });
    if (o != out_.end())
        o->atMs = nowMs;
    else
        out_.push_back(Out{skill, nowMs});
}

void Tracker::clear() {
    units_.clear();
    casts_.clear();
}

int Tracker::standing(uint16_t skill) const {
    return int(std::count_if(casts_.begin(), casts_.end(), [&](const Cast& k) { return k.skill == skill; }));
}

uint64_t Tracker::burntOut(uint16_t skill) const {
    const auto o = std::find_if(out_.begin(), out_.end(), [&](const Out& x) { return x.skill == skill; });
    return o != out_.end() ? o->atMs : 0;
}

}  // namespace uagfx::groundunits
