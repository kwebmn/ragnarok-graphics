#include "win/shadow_patch.hpp"

#include <windows.h>

#include <array>
#include <cstring>

#include "core/shadow_exe.hpp"
#include "win/log.hpp"

namespace uagfx::win {

// 3.0.24-beta.31: every place of core/shadow_exe's table guarded - each only when this build's bytes are there (its
// mov and call, and the proof of its way on); one page holds the guards
bool installShadowFix() {
    static bool done = false;  // once: their calls are the guards' after it
    if (done) return true;
    const auto same = [](uint32_t at, const uint8_t* want, size_t size) {
        uint8_t got[16] = {};
        SIZE_T n = 0;
        return size <= sizeof got &&
               ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(at)), got, size, &n) &&
               n == size && std::memcmp(got, want, size) == 0;
    };
    bool ok[shadow_exe::kSiteCount] = {};
    size_t any = 0;
    for (size_t i = 0; i < shadow_exe::kSiteCount; ++i) {
        const shadow_exe::Site& s = shadow_exe::kSites[i];
        const std::array<uint8_t, 10> original = shadow_exe::originalAt(s.at);
        ok[i] = same(s.at, original.data(), original.size()) && same(s.checkAt, s.check, s.checkSize);
        if (ok[i]) ++any;
        else
            LOG_INFO("mode guard: the game's own for %s (its code is not where this build of uaRO.exe has it)",
                     s.what);
    }
    if (!any) return false;
    auto* page = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!page) return false;
    for (size_t i = 0; i < shadow_exe::kSiteCount; ++i)
        if (ok[i]) {
            const shadow_exe::Guard g = shadow_exe::guardFor(shadow_exe::kSites[i].wayOut);
            std::memcpy(page + 32 * i, g.code, sizeof g.code);
        }
    DWORD was = 0;
    if (!VirtualProtect(page, 4096, PAGE_EXECUTE_READ, &was)) return false;
    for (size_t i = 0; i < shadow_exe::kSiteCount; ++i) {
        if (!ok[i]) continue;
        const shadow_exe::Site& s = shadow_exe::kSites[i];
        uint8_t call[5];
        shadow_exe::callFrom(s.at + 5, uint32_t(reinterpret_cast<uintptr_t>(page + 32 * i)), call);
        auto* site = reinterpret_cast<uint8_t*>(uintptr_t(s.at + 5));
        if (!VirtualProtect(site, sizeof call, PAGE_EXECUTE_READWRITE, &was)) continue;
        std::memcpy(site, call, sizeof call);
        DWORD unused = 0;
        VirtualProtect(site, sizeof call, was, &unused);
        FlushInstructionCache(GetCurrentProcess(), site, sizeof call);
        LOG_INFO("mode guard: %s no longer crashes the game (uaRO.exe 0x%X)", s.what, s.at + 5);
    }
    done = true;
    return true;
}

}  // namespace uagfx::win
