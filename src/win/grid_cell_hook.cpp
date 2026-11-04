#include "win/grid_cell_hook.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "core/grid_cell.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

bool g_installed = false;
volatile bool g_hidden = false;
bool g_told = false;

// In place of the draw's mov eax, [kFlag]: the same register, the flags set by its test after
uint32_t gridFlag() {
    const uint32_t flag = *reinterpret_cast<const volatile uint32_t*>(uintptr_t(gridcell::kFlag));
    return gridcell::answer(flag, g_hidden);
}

}  // namespace

bool installGridCell() {
    uint8_t game[9], now[9] = {};
    gridcell::original(game);
    SIZE_T got = 0;
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(gridcell::kRead));
    if (!ReadProcessMemory(GetCurrentProcess(), at, now, sizeof now, &got) || got != sizeof now ||
        std::memcmp(now, game, sizeof now) != 0) {
        LOG_INFO("pad: the game's cell under the mouse stays while the controller plays - its draw is not where this "
                 "build of uaRO.exe has it");
        return false;
    }
    uint8_t call[5] = {0xE8};
    const int32_t by = int32_t(reinterpret_cast<uintptr_t>(&gridFlag) - (uintptr_t(gridcell::kRead) + 5));
    std::memcpy(call + 1, &by, sizeof by);
    DWORD old = 0;
    if (!VirtualProtect(at, sizeof call, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("pad: the game's cell under the mouse stays while the controller plays - its code cannot be changed "
                 "(error %lu)", GetLastError());
        return false;
    }
    std::memcpy(at, call, sizeof call);
    DWORD unused = 0;
    VirtualProtect(at, sizeof call, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, sizeof call);
    g_installed = true;
    LOG_INFO("pad: the game's cell under the mouse read through the driver (uaRO.exe 0x%X) - none while the controller "
             "plays", gridcell::kRead);
    return true;
}

void hideGridCell(bool hidden) {
    g_hidden = hidden;
    if (hidden && g_installed && !g_told) {
        g_told = true;
        LOG_INFO("pad: the game's cell under the mouse hidden while the controller plays (the mouse brings it back)");
    }
}

}  // namespace uagfx::win
