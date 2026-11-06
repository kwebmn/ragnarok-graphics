#include "win/teleport_fade.hpp"

#include <windows.h>

#include <cstring>

#include "core/screen_shake_exe.hpp"
#include "core/skill_jokes_exe.hpp"
#include "core/teleport_fade_exe.hpp"
#include "win/code_switch.hpp"
#include "win/log.hpp"

namespace uagfx::win {

void quickTeleport(bool wanted) {
    static int now = -1;     // -1 not looked at, 0 the game's, 1 quick
    static bool known = true;
    if (!known || now == (wanted ? 1 : 0)) return;
    namespace tf = teleport_fade_exe;
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(tf::kSite));
    uint8_t have[sizeof tf::kGame] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), at, have, sizeof have, &got) || got != sizeof have ||
        (std::memcmp(have, tf::kGame, sizeof have) != 0 && std::memcmp(have, tf::kQuick, sizeof have) != 0)) {
        known = false;
        if (wanted) LOG_INFO("teleport: the darkening is not where this build of uaRO.exe has it - the game's own");
        return;
    }
    DWORD old = 0;
    if (!VirtualProtect(at, sizeof have, PAGE_EXECUTE_READWRITE, &old)) {
        known = false;
        LOG_INFO("teleport: the game's code cannot be changed (error %lu)", GetLastError());
        return;
    }
    std::memcpy(at, wanted ? tf::kQuick : tf::kGame, sizeof have);
    DWORD unused = 0;
    VirtualProtect(at, sizeof have, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, sizeof have);
    if (now >= 0 || wanted)
        LOG_INFO("teleport: on the same map %s", wanted ? "at once - without the game's 255 ms of darkening"
                                                        : "with the game's darkening");
    now = wanted ? 1 : 0;
}

void stillScreen(bool wanted) {
    namespace ss = screen_shake_exe;
    static CodeSwitch shake{"screen shake (Earthquake and the like) off", ss::kShakeStart, ss::kGame, ss::kNone,
                            sizeof ss::kGame};
    shake.want(wanted);
}

void silentJokes(bool wanted) {
    namespace sj = skill_jokes_exe;
    static CodeSwitch jokes{"Frost Joke and Scream: no line in the chat", sj::kOpened, sj::kGame, sj::kSilent,
                            sizeof sj::kGame};
    jokes.want(wanted);
}

}  // namespace uagfx::win
