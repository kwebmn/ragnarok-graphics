#include "win/code_switch.hpp"

#include <windows.h>

#include <cstring>
#include <vector>

#include "win/log.hpp"

namespace uagfx::win {

void CodeSwitch::want(bool wanted) {
    if (!known || now == (wanted ? 1 : 0)) return;
    auto* code = reinterpret_cast<uint8_t*>(uintptr_t(at));
    std::vector<uint8_t> have(size);
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), code, have.data(), size, &got) || got != size ||
        (std::memcmp(have.data(), game, size) != 0 && std::memcmp(have.data(), ours, size) != 0)) {
        known = false;
        LOG_INFO("%s: not where this build of uaRO.exe has it (0x%X) - the game's own", what, at);
        return;
    }
    DWORD old = 0;
    if (!VirtualProtect(code, size, PAGE_EXECUTE_READWRITE, &old)) {
        known = false;
        LOG_INFO("%s: the game's code cannot be changed (error %lu)", what, GetLastError());
        return;
    }
    std::memcpy(code, wanted ? ours : game, size);
    DWORD unused = 0;
    VirtualProtect(code, size, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), code, size);
    if (now >= 0 || wanted) LOG_INFO("%s: %s", what, wanted ? "ours" : "the game's own");
    now = wanted ? 1 : 0;
}

}  // namespace uagfx::win
