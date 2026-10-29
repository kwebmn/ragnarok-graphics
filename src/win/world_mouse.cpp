#include "win/world_mouse.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <iterator>

#include "pad/win/client_mouse.hpp"
#include "pad/win/game_window.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

// uaRO.exe's: its windows' manager's look at the mouse (the manager in ECX), its one call in the game mode's frame
// (FUN_00566c80), and a plain window's cursor - the arrow on the game mode (two arguments it does not read; each
// window's slot +0x78, the manager calls it on the window under the cursor every frame)
using InputFn = int(__attribute__((thiscall)) *)(uint32_t manager);
using ArrowFn = void(__attribute__((stdcall)) *)(int x, int y);
constexpr uint32_t kInput = 0x4CCFB0, kArrow = 0x4C9D10;
constexpr uint32_t kCall = 0x566C9A;

// The bytes the call, the answer's use and both functions are made of in the build this knows.
struct Known {
    uint32_t at;
    const char* bytes;
    size_t size;
};
const Known kKnown[] = {
    {0x566C95, "\xB9\xD8\x7E\x72\x00\xE8\x11\x63\xF6\xFF\x8B\xF8", 12},  // mov ecx,manager; call; mov edi,eax
    {0x566D03, "\x83\xFF\x02\x75\x07", 5},                                // cmp edi,2: the world's after all
    {0x4CCFB0, "\x55\x8B\xEC\x83\xEC\x28\xA1\x04\xE7\x6E\x00", 11},      // the manager's: the cursor's x first
    {0x4C9D10, "\x8B\x0D\x7C\xB5\x74\x00\x6A\x00\xE8\x33\xBD\x02\x00\xC2\x08\x00", 16},  // the mode; 0; ret 8
};

// In Blocker's order
const char* const kParts[] = {"the windows' panel", "the answer card", "the new minimap", "«Довідник»"};
unsigned g_told = 0;  // the parts whose first cursor over them is logged

int __attribute__((thiscall)) windowsInput(uint32_t manager) {
    const int answer = reinterpret_cast<InputFn>(uintptr_t(kInput))(manager);
    if (answer == 1) return answer;  // a window of the game's own, or one holding the mouse
    int x = 0, y = 0, width = 0, height = 0;
    if (!uapad::clientmouse::read(x, y, width, height)) return answer;
    const int part = uapad::gamewindow::blockedUnder(x, y, width, height);
    if (part < 0 || part >= int(std::size(kParts))) return answer;
    if (answer == 0) reinterpret_cast<ArrowFn>(uintptr_t(kArrow))(0, 0);  // no window of the game's set one there
    if (!(g_told & (1u << part))) {
        g_told |= 1u << part;
        LOG_INFO("world mouse: the cursor over %s (%d, %d in %dx%d) - a window for the game, the world under it does "
                 "not react", kParts[part], x, y, width, height);
    }
    return 1;
}

}  // namespace

bool installWorldMouse() {
    static int done = -1;  // once: 1 hooked, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    for (const Known& k : kKnown) {
        uint8_t bytes[16] = {};
        SIZE_T got = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(k.at)), bytes, k.size,
                               &got) ||
            got != k.size || std::memcmp(bytes, k.bytes, k.size) != 0) {
            LOG_INFO("world mouse: the world reacts to the cursor under the driver's parts too - the game's code is "
                     "not where this build of uaRO.exe has it (0x%X)", k.at);
            return false;
        }
    }
    if (!uapad::clientmouse::supported()) {
        LOG_INFO("world mouse: the world reacts to the cursor under the driver's parts too - the game's cursor is "
                 "not where this build of uaRO.exe has it");
        return false;
    }
    auto* rel = reinterpret_cast<uint8_t*>(uintptr_t(kCall + 1));
    DWORD old = 0;
    if (!VirtualProtect(rel, 4, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("world mouse: the world reacts to the cursor under the driver's parts too - the game's code cannot "
                 "be changed (error %lu)", GetLastError());
        return false;
    }
    const int32_t to = int32_t(reinterpret_cast<uintptr_t>(&windowsInput) - (uintptr_t(kCall) + 5));
    std::memcpy(rel, &to, sizeof to);
    DWORD unused = 0;
    VirtualProtect(rel, 4, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), rel, 4);
    done = 1;
    LOG_INFO("world mouse: the driver's parts in the picture are windows for the game (uaRO.exe 0x%X) - the world "
             "under them does not react to the cursor", kCall);
    return true;
}

}  // namespace uagfx::win
