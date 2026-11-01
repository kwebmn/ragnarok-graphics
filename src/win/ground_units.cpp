#include "win/ground_units.hpp"

#include <windows.h>

#include <iterator>

#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win::groundunitswin {
namespace {

constexpr uint32_t kOwnAccount = 0x771F7C;  // uaRO.exe's session: the character's own account

struct State {
    groundunits::Tracker tracker;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

constexpr uint16_t kCounted[] = {18, 87, 92, 80};  // Fire Wall, Ice Wall, Quagmire, Fire Pillar

const char* nameOf(uint16_t skill) {
    switch (skill) {
        case 18: return "Fire Wall";
        case 87: return "Ice Wall";
        case 92: return "Quagmire";
        case 80: return "Fire Pillar";
        default: return "?";
    }
}

uint16_t countedSkill(uint8_t type) {
    for (const uint16_t skill : kCounted)
        for (const uint8_t t : groundunits::unitTypes(skill))
            if (t == type) return skill;
    return 0;
}

}  // namespace

void packet(uint16_t id, const uint8_t* packet) {
    State& s = state();
    if (id == 0x0091 || id == 0x0092) {
        s.tracker.clear();
        return;
    }
    const uint64_t now = GetTickCount64();
    if (id == 0x011F) {
        groundunits::Seen seen;
        uint32_t self = 0;
        if (!groundunits::readSeen(packet, 16, seen) || !readGame(kOwnAccount, &self, 4)) return;
        const uint16_t skill = countedSkill(seen.type);
        if (skill == 0) return;
        const int before = s.tracker.standing(skill);
        s.tracker.seen(seen, self, now);
        if (s.tracker.standing(skill) > before && s.told++ < 20)
            LOG_INFO("ground units: a %s cast standing at %d, %d (%d of it now)", nameOf(skill), seen.x, seen.y,
                     s.tracker.standing(skill));
        return;
    }
    uint32_t unit = 0;
    if (id != 0x0120 || !groundunits::readGone(packet, 6, unit)) return;
    uint64_t was[std::size(kCounted)];
    for (size_t i = 0; i < std::size(kCounted); ++i) was[i] = s.tracker.burntOut(kCounted[i]);
    s.tracker.gone(unit, now);
    for (size_t i = 0; i < std::size(kCounted); ++i)
        if (s.tracker.burntOut(kCounted[i]) != was[i] && s.told++ < 20)
            LOG_INFO("ground units: a %s cast burnt out (%d of it standing)", nameOf(kCounted[i]),
                     s.tracker.standing(kCounted[i]));
}

const groundunits::Tracker& tracker() { return state().tracker; }

}  // namespace uagfx::win::groundunitswin
