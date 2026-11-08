#include "win/upkeep_watch.hpp"

#include <cstring>
#include <vector>

#include "core/hotbar_panel.hpp"
#include "core/ui_chime.hpp"
#include "core/upkeep.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/log.hpp"
#include "win/sound_hook.hpp"

namespace uagfx::win::upkeepwatch {
namespace {

constexpr size_t kReadable = 2048;  // as win/packet_tap reads them

struct State {
    upkeep::Watch watch;
    bool brokenOn = false, hungerOn = false;
    int petLevel = 0, homunLevel = 0, told = 0;
};

State& state() {
    static State s;
    return s;
}

void chime(bool out) {
    static const std::vector<uint8_t> low = chime::wav(chime::Kind::AmmoLow), none = chime::wav(chime::Kind::AmmoOut);
    playOwn(out ? none.data() : low.data());
}

// A fall to hungry (1) or very (2) chimes once; fed again quiet
void hunger(State& s, int fullness, int& was, const char* who) {
    const int level = upkeep::hungerLevel(fullness);
    if (level > was && s.hungerOn) {
        chime(level == 2);
        if (s.told++ < 20) LOG_INFO("upkeep: the %s is %s (%d)", who, level == 2 ? "very hungry" : "hungry", fullness);
    }
    was = level;
}

uint32_t rd32(const uint8_t* p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p + at, 4);
    return v;
}
uint16_t rd16(const uint8_t* p, size_t at) { return uint16_t(p[at] | p[at + 1] << 8); }

// 3.0.16-beta.28 (the tester: after entering the game, before a teleport, the own Isis is taken for a monster - no
// accessory, no menu on the right button; after a Fly Wing fine): the pet's packets in their order as the game gets
// them, with the client's own pet (uaRO.exe DAT_00771a70, set by 0x01a4 type 0; the menu needs it and the actor's
// +0x248 == 100 from the spawn's hair 100) and its accessory (DAT_00771a98) - uAthena sends the same packets on both
// ways, the difference is to be seen. At most 60 lines a session.
void trace(uint16_t id, const uint8_t* p) {
    static int told = 0;
    static uint32_t unit = 0;  // the own pet's, from 0x01a4 type 0
    if (told >= 60 || !p) return;
    const auto mine = [] { return *reinterpret_cast<const volatile uint32_t*>(uintptr_t(0x771A70)); };
    const auto worn = [] { return *reinterpret_cast<const volatile uint32_t*>(uintptr_t(0x771A98)); };
    switch (id) {
        case 0x0073:
        case 0x0091:
        case 0x0092:
            if (unit) LOG_INFO("pet trace: 0x%04x (map entered/changed), client pet %u", id, mine()), ++told;
            break;
        case 0x007c:
        case 0x0078:
        case 0x007b: {
            const uint32_t who = rd32(p, 2);
            const uint16_t hair = id == 0x007c ? rd16(p, 14) : rd16(p, 16);
            if (who != unit && (hair != 100 || unit)) return;  // others' pets once the own is known
            LOG_INFO("pet trace: 0x%04x unit %u class %u hair %u accessory %u, client pet %u accessory %u", id, who,
                     id == 0x007c ? rd16(p, 20) : rd16(p, 14), hair, id == 0x007c ? rd16(p, 24) : rd16(p, 20),
                     mine(), worn());
            ++told;
            break;
        }
        case 0x01a4:
            if (p[2] == 0) unit = rd32(p, 3);
            LOG_INFO("pet trace: 0x01a4 type %u unit %u value %d, client pet %u accessory %u", p[2], rd32(p, 3),
                     int(rd32(p, 7)), mine(), worn());
            ++told;
            break;
        case 0x01a2:
            LOG_INFO("pet trace: 0x01a2 status, client pet %u", mine()), ++told;
            break;
        case 0x0080:
            if (unit && rd32(p, 2) == unit)
                LOG_INFO("pet trace: 0x0080 unit %u gone (%u), client pet %u", unit, p[6], mine()), ++told;
            break;
        default: break;
    }
}

}  // namespace

void packet(uint16_t id, const uint8_t* p) {
    trace(id, p);
    State& s = state();
    if (!s.watch.packet(id, p, kReadable)) return;
    if (id == 0x00a4) {
        const std::vector<int> fresh = s.watch.takeFresh();
        if (!fresh.empty() && s.brokenOn) chime(true);
        if (s.told++ < 20)
            LOG_INFO("upkeep: %zu broken piece(s) in the equipment list, %zu just broken", s.watch.broken().size(),
                     fresh.size());
        return;
    }
    hunger(s, s.watch.pet(), s.petLevel, "pet");
    hunger(s, s.watch.homun(), s.homunLevel, "homunculus");
}

void setOn(bool brokenGear, bool petHunger) {
    State& s = state();
    if (brokenGear != s.brokenOn) LOG_INFO("upkeep: [interface] broken_gear = %d", brokenGear ? 1 : 0);
    if (petHunger != s.hungerOn) LOG_INFO("upkeep: [interface] pet_hunger = %d", petHunger ? 1 : 0);
    s.brokenOn = brokenGear;
    s.hungerOn = petHunger;
}

std::string broken(unsigned codePage) {
    const State& s = state();
    if (!s.brokenOn || s.watch.broken().empty()) return {};
    if (s.watch.broken().size() > 1) return std::to_string(s.watch.broken().size());
    const std::string id = std::to_string(*s.watch.broken().begin());
    const auto& names = clienttables::itemNames();
    const auto n = names.find(id);
    return n != names.end() ? utf8Of(widened(hotbar::spaced(n->second), codePage)) : "#" + id;
}

int pet() { return state().hungerOn ? state().watch.pet() : -1; }
int homun() { return state().hungerOn ? state().watch.homun() : -1; }

}  // namespace uagfx::win::upkeepwatch
