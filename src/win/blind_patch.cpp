#include "win/blind_patch.hpp"

#include <windows.h>

#include <cstring>

#include "core/blind_exe.hpp"
#include "win/log.hpp"

namespace uagfx::win {

bool installBlindFix() {
    uint8_t game[6], wide[6];
    blind_exe::original(game);
    blind_exe::widened(wide);
    float reach = 0.0f;
    SIZE_T got = 0;
    const auto* wideAt = reinterpret_cast<const void*>(uintptr_t(blind_exe::kWideReachAt));
    bool known = ReadProcessMemory(GetCurrentProcess(), wideAt, &reach, sizeof reach, &got) && got == sizeof reach &&
                 reach == blind_exe::kWideReach;
    for (uint32_t site : blind_exe::kSites) {
        uint8_t now[6] = {};
        known = known && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(site)), now,
                                           sizeof now, &got) &&
                got == sizeof now && std::memcmp(now, game, sizeof now) == 0;
    }
    if (!known) {
        LOG_INFO("blindness: the game's own darkness (it is not where this build of uaRO.exe has it)");
        return false;
    }
    for (uint32_t site : blind_exe::kSites) {
        auto* at = reinterpret_cast<uint8_t*>(uintptr_t(site));
        DWORD old = 0;
        if (!VirtualProtect(at, sizeof wide, PAGE_EXECUTE_READWRITE, &old)) {
            LOG_INFO("blindness: the game's own darkness (its code cannot be changed, error %lu)", GetLastError());
            return false;
        }
        std::memcpy(at, wide, sizeof wide);
        DWORD unused = 0;
        VirtualProtect(at, sizeof wide, old, &unused);
        FlushInstructionCache(GetCurrentProcess(), at, sizeof wide);
    }
    LOG_INFO("blindness: its darkness covers the whole screen (uaRO.exe 0x%X)", blind_exe::kRender);
    return true;
}

}  // namespace uagfx::win
