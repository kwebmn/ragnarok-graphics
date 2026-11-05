#include "win/skill_delay_watch.hpp"

#include <windows.h>

#include <cstring>

#include "core/char_status.hpp"
#include "core/skill_delay.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win::skilldelaywin {
namespace {

constexpr uint32_t kOwnAccount = 0x771F7C;

struct State {
    ULONGLONG from = 0;
    int ms = 0, told = 0;
};

State& state() {
    static State s;
    return s;
}

uint16_t rd16(const uint8_t* p, size_t at) { return uint16_t(p[at] | p[at + 1] << 8); }
uint32_t rd32(const uint8_t* p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p + at, 4);
    return v;
}

}  // namespace

void packet(uint16_t id, const uint8_t* p) {
    if (!p) return;
    int skill = 0, level = 0;
    uint32_t source = 0;
    switch (id) {
        case 0x01de: skill = rd16(p, 2), source = rd32(p, 4), level = rd16(p, 28); break;
        case 0x0114: skill = rd16(p, 2), source = rd32(p, 4), level = rd16(p, 26); break;
        case 0x011a: skill = rd16(p, 2), level = rd16(p, 4), source = rd32(p, 10); break;
        case 0x0117: skill = rd16(p, 2), source = rd32(p, 4), level = rd16(p, 8); break;
        default: return;
    }
    uint32_t own = 0;
    if (!skill || !readGame(kOwnAccount, &own, 4) || source != own) return;
    const int base = skilldelay::afterCast(skill, level);
    if (base <= 0) return;
    charstatus::Status st;
    const int dex = readOwnStatus(st) ? st.statBase[4] + st.statBonus[4] : 0;
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    const int ms = skilldelay::withDex(base, dex);
    if (now - s.from < 100 && ms == s.ms) return;  // the same skill's other targets
    s.from = now;
    s.ms = ms;
    if (s.told++ < 20)
        LOG_INFO("skills: %d level %d done - its delay %d ms (DEX %d of %d ms)", skill, level, ms, dex, base);
}

float left() {
    const State& s = state();
    return skilldelay::left(s.from, s.ms, GetTickCount64());
}

}  // namespace uagfx::win::skilldelaywin
