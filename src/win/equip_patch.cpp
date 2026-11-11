#include "win/equip_patch.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "core/equip_exe.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

using WearFn = int(__attribute__((thiscall)) *)(uint32_t session, uint32_t info);

int __attribute__((thiscall)) wearGuard(uint32_t session, uint32_t info) {
    uint32_t mask = 0;
    if (readGame(info + equip_exe::kMaskAt, &mask, 4) && !equip_exe::wearable(mask)) {
        static int told = 0;
        if (told < 10) {
            ++told;
            LOG_INFO("equipment: the item %u worn at the places 0x%X (it can go at 0x%X) - more than the game can "
                     "take, or none it knows: left out, not to crash it",
                     read32(info + equip_exe::kIndexAt), mask, read32(info + equip_exe::kPlacesAt));
        }
        return 1;  // as the game's: taken
    }
    return reinterpret_cast<WearFn>(uintptr_t(equip_exe::kWear))(session, info);
}

// The call at `site` is the game's E8 to its wearing
bool callsWear(uint32_t site) {
    uint8_t code[5] = {};
    if (!readGame(site, code, sizeof code) || code[0] != 0xE8) return false;
    int32_t rel = 0;
    std::memcpy(&rel, code + 1, 4);
    return site + 5 + uint32_t(rel) == equip_exe::kWear;
}

}  // namespace

bool installWearGuard() {
    static int done = -1;
    if (done >= 0) return done == 1;
    done = 0;
    const uint32_t sites[2] = {equip_exe::kListCall, equip_exe::kAnswerCall};
    for (const uint32_t site : sites)
        if (!callsWear(site)) {
            LOG_INFO("equipment: the game's own wearing (its calls are not where this build of uaRO.exe has them)");
            return false;
        }
    for (const uint32_t site : sites) {
        const int32_t rel = int32_t(uint32_t(reinterpret_cast<uintptr_t>(&wearGuard)) - (site + 5));
        if (!writeGame(site + 1, &rel, 4)) {
            LOG_INFO("equipment: the game's wearing could not be guarded at uaRO.exe 0x%X", site);
            return false;
        }
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(site)), 5);
    }
    done = 1;
    LOG_INFO("equipment: an item said to be worn at more places than the game takes no longer crashes it (uaRO.exe "
             "0x%X, 0x%X)", sites[0], sites[1]);
    return true;
}

}  // namespace uagfx::win
