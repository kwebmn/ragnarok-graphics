#include "win/sphere_watch.hpp"

#include <cstring>

#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win::spherewatch {
namespace {

constexpr uint32_t kOwnAccount = 0x771F7C;  // uaRO.exe's session: the character's own account (win/ground_units)

struct State {
    int count = 0;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

void packet(uint16_t id, const uint8_t* packet) {
    State& s = state();
    if (id == 0x0091 || id == 0x0092) {
        s.count = 0;
        return;
    }
    if ((id != 0x01d0 && id != 0x01e1) || !packet) return;
    uint32_t unit = 0, own = 0;
    uint16_t n = 0;
    std::memcpy(&unit, packet + 2, 4);
    std::memcpy(&n, packet + 6, 2);
    if (!readGame(kOwnAccount, &own, 4) || !own || unit != own) return;
    if (int(n) != s.count && s.told < 20) {
        ++s.told;
        LOG_INFO("spheres: 0x%04x - the character's %d (was %d)", id, int(n), s.count);
    }
    s.count = int(n);
}

int count() { return inGameWorld() ? state().count : 0; }

}  // namespace uagfx::win::spherewatch
