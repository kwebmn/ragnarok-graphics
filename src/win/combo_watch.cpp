#include "win/combo_watch.hpp"

#include <windows.h>

#include <cstring>

#include "core/combo.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win::combowatch {
namespace {

constexpr uint32_t kOwnAccount = 0x771F7C;  // uaRO.exe's session: the character's own account

struct State {
    combo::Window window;
    int told = 0;
    ULONGLONG steelUntil = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t rd32(const uint8_t* p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p + at, 4);
    return v;
}

}  // namespace

void packet(uint16_t id, const uint8_t* p) {
    if (!p || (id != 0x01d2 && id != 0x01de && id != 0x0114 && id != 0x011a)) return;
    uint32_t own = 0;
    if (id == 0x011a) {
        uint16_t skill = 0, level = 0;
        std::memcpy(&skill, p + 2, 2);
        std::memcpy(&level, p + 4, 2);
        if (skill == 268 && readGame(kOwnAccount, &own, 4) && own && rd32(p, 10) == own) {
            state().steelUntil = GetTickCount64() + 30000ULL * (level ? level : 1);
            LOG_INFO("combo: Steel Body level %u - %u s", unsigned(level), 30u * (level ? level : 1u));
        }
        return;
    }
    if (!readGame(kOwnAccount, &own, 4) || !own || rd32(p, id == 0x01d2 ? 2 : 4) != own) return;
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    if (id == 0x01d2) {
        s.window.opened(rd32(p, 6), now);
        if (s.told++ < 20) LOG_INFO("combo: 0x01d2 - a window of %u ms", rd32(p, 6));
        return;
    }
    uint16_t skill = 0;
    std::memcpy(&skill, p + 2, 2);
    if (skill) s.window.skillDone(skill, now);
}

std::vector<uint16_t> next() {
    const uint16_t after = state().window.after(GetTickCount64());
    return after ? combo::nextOf(after) : std::vector<uint16_t>{};
}

float left() { return state().window.left(GetTickCount64()); }

bool steelBody() { return GetTickCount64() < state().steelUntil; }

}  // namespace uagfx::win::combowatch
