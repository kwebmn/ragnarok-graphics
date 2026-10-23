#include "win/map_start.hpp"

#include <windows.h>

#include <cstdint>

#include "win/log.hpp"

namespace uagfx::win {
namespace {

// uaRO.exe's: CGameMode's map start (the mode in ECX, the map's file name; ret 4), and its place in the vtable
using StartFn = uint32_t(__attribute__((thiscall)) *)(void* mode, const char* map);
constexpr uint32_t kSlot = 0x69F38C, kStart = 0x559030;

volatile LONG g_starting = 0;
volatile LONG g_starts = 0;

uint32_t __attribute__((thiscall)) mapStart(void* mode, const char* map) {
    InterlockedIncrement(&g_starts);
    InterlockedIncrement(&g_starting);
    const uint32_t result = reinterpret_cast<StartFn>(uintptr_t(kStart))(mode, map);
    InterlockedDecrement(&g_starting);
    return result;
}

}  // namespace

bool installMapStartHook() {
    static int done = -1;  // once: 1 hooked, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    uint32_t now = 0;
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(kSlot)), &now, sizeof now,
                           &got) ||
        got != sizeof now || now != kStart) {
        LOG_INFO("map loads: not watched - this build of uaRO.exe has its game mode's map start elsewhere");
        return false;
    }
    auto* slot = reinterpret_cast<uint32_t*>(uintptr_t(kSlot));
    DWORD old = 0;
    if (!VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old)) {
        LOG_INFO("map loads: not watched - the game mode's table cannot be changed (error %lu)", GetLastError());
        return false;
    }
    *slot = uint32_t(reinterpret_cast<uintptr_t>(&mapStart));
    DWORD unused = 0;
    VirtualProtect(slot, sizeof *slot, old, &unused);
    done = 1;
    LOG_INFO("map loads: the game mode's map start watched (uaRO.exe 0x%X)", kStart);
    return true;
}

bool mapStarting() { return g_starting != 0; }

unsigned mapStarts() { return unsigned(g_starts); }

}  // namespace uagfx::win
