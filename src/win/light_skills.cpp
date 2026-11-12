#include "win/light_skills.hpp"

#include <windows.h>

#include <cstdint>

#include "core/light_skills.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"

namespace uagfx::win::lightskillswin {
namespace {

namespace ls = ::uagfx::lightskills;

// uaRO.exe's: the effects' light variant now (its effect starts read it: FUN_00594870/FUN_00594920 set it from the
// player's own on a map's way in, 1 on a guild war's map) and the player's own /mineffect
constexpr uint32_t kMinNow = 0x772D2C, kMinOwn = 0x772D30;

struct State {
    ls::State logic;
    uint64_t since = 0;
    uint32_t frames = 0, skills = 0;
    bool forced = false;  // set by the driver on `map`
    std::string map;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

void drawn() { ++state().skills; }

int frame(bool on, int keepFps, bool foreground, const std::string& map) {
    State& s = state();
    const uint64_t now = GetTickCount64();
    if (s.forced && map != s.map) s.forced = false;  // the game's own again, set on the way in
    if (!foreground || map.empty()) {
        s.since = 0;
        return -1;
    }
    if (!s.since) {
        s.since = now;
        s.frames = s.skills = 0;
        return -1;
    }
    ++s.frames;
    if (now - s.since < ls::kWindowMs) return -1;
    const uint32_t ms = uint32_t(now - s.since), frames = s.frames, skills = s.skills;
    s.since = now;
    s.frames = s.skills = 0;
    if (!on) s.logic = ls::State{};
    const bool want = on && ls::step(s.logic, now, ms, frames, skills, keepFps);
    uint32_t active = 0;
    if (!knownGameBuild() || !readGame(kMinNow, &active, 4)) return -1;
    const int fps = int(double(frames) * 1000.0 / double(ms) + 0.5);
    if (want && !s.forced && active == 0) {
        write32(kMinNow, 1);
        s.forced = true;
        s.map = map;
        if (s.told++ < 10)
            LOG_INFO("light skills: the game's light skill effects on (its /mineffect's) - %d fps under %d with %u "
                     "skills' draws a frame on %s; the player's own /mineffect as it was",
                     fps, keepFps, frames ? skills / frames : 0u, map.c_str());
        return fps;
    }
    if (!want && s.forced) {
        uint32_t own = 0;
        readGame(kMinOwn, &own, 4);
        write32(kMinNow, own ? 1u : 0u);
        s.forced = false;
        if (s.told++ < 10)
            LOG_INFO("light skills: the game's own skill effects again (%d fps, %u skills' draws a frame)%s", fps,
                     frames ? skills / frames : 0u, own ? " - the player's own /mineffect on" : "");
    }
    return -1;
}

}  // namespace uagfx::win::lightskillswin
