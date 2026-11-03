#include "win/window_guard.hpp"

#include <windows.h>

#include <cstring>
#include <vector>

#include "core/window_guard.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

namespace wg = uagfx::windowguard;

// uaRO.exe's: Enter's answer (thiscall: the windows' manager) byte for byte, and its one call - the manager's keys'
// 0x0D (FUN_004d9xxx: mov ecx, esi; call)
constexpr uint32_t kList = 0x38, kEnter = 0x4CE990, kEnterCall = 0x4D9BE5;
const uint8_t kEnterCode[] = {0x56, 0x8B, 0xF1, 0x57, 0x8B, 0x4E, 0x70};  // push esi; mov esi, ecx; push edi; ...
constexpr size_t kMostWindows = 512;
using EnterFn = void(__attribute__((thiscall)) *)(uint32_t manager);

int g_told = 0;
unsigned g_mended = 0;

// The list's nodes as the game walks them itself each frame (its windows' draw right after); the fields' windows
// only compared, never read
bool listRead(uint32_t at, uint32_t& value) {
    if (at < 0x10000) return false;
    value = *reinterpret_cast<const uint32_t*>(uintptr_t(at));
    return true;
}

void __attribute__((thiscall)) enterGuarded(uint32_t manager) {
    mendWindowFields(manager, "at Enter");
    reinterpret_cast<EnterFn>(uintptr_t(kEnter))(manager);
}

bool same(uint32_t at, const uint8_t* code, size_t size) {
    uint8_t got[16] = {};
    SIZE_T n = 0;
    return size <= sizeof got &&
           ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(at)), got, size, &n) &&
           n == size && std::memcmp(got, code, size) == 0;
}

// A call's target where this build has it: E8 and Enter's answer
bool callsEnter(uint32_t at) {
    uint8_t op[5] = {};
    SIZE_T n = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(at)), op, sizeof op, &n) ||
        n != sizeof op || op[0] != 0xE8)
        return false;
    int32_t rel = 0;
    std::memcpy(&rel, op + 1, sizeof rel);
    return at + 5 + uint32_t(rel) == kEnter;
}

}  // namespace

void mendWindowFields(uint32_t manager, const char* when) {
    uint32_t head = 0;
    if (!listRead(manager + kList, head)) return;
    bool whole = false;
    const std::vector<uint32_t> listed = wg::listedWindows(listRead, head, kMostWindows, whole);
    if (!whole) return;  // nothing to judge by
    std::vector<uint32_t> named;
    for (const wg::Field& f : wg::kEnterFields)
        named.push_back(*reinterpret_cast<const uint32_t*>(uintptr_t(manager + f.offset)));
    for (const size_t i : wg::gone(named, listed)) {
        const wg::Field& f = wg::kEnterFields[i];
        *reinterpret_cast<uint32_t*>(uintptr_t(manager + f.offset)) = 0;
        ++g_mended;
        if (g_told < 10) {
            ++g_told;
            LOG_ERROR("window guard: the game's windows' manager still named %s (UI window 0x%X, its +0x%X: 0x%08X) - "
                      "a window gone from its list of %zu, freed; the name cleared %s on %s, not called through "
                      "(R-ZS5B2D: Enter crashed the game there); %u so far",
                      f.name, f.window, f.offset, named[i], listed.size(), when, readGameMap().c_str(), g_mended);
        }
    }
}

bool installWindowGuard() {
    static int done = -1;  // once: 1 in, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    if (!same(kEnter, kEnterCode, sizeof kEnterCode) || !callsEnter(kEnterCall)) {
        LOG_INFO("window guard: the game's Enter is not where this build of uaRO.exe has it - left as it is");
        return false;
    }
    auto* rel = reinterpret_cast<uint8_t*>(uintptr_t(kEnterCall + 1));
    DWORD old = 0;
    if (!VirtualProtect(rel, 4, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("window guard: the game's code cannot be changed (error %lu) - left as it is", GetLastError());
        return false;
    }
    const int32_t to = int32_t(reinterpret_cast<uintptr_t>(&enterGuarded) - (uintptr_t(kEnterCall) + 5));
    std::memcpy(rel, &to, sizeof to);
    DWORD unused = 0;
    VirtualProtect(rel, 4, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), rel, 4);
    done = 1;
    LOG_INFO("window guard: the game's Enter (uaRO.exe 0x%X) goes through the driver - a window its windows' manager "
             "still names but has freed is not called through", kEnter);
    return true;
}

}  // namespace uagfx::win
