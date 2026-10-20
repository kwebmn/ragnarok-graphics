#include "win/server_night.hpp"

#include <windows.h>

#include <cstring>

#include "core/server_night.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

int g_known = -1;     // once: 1 this build of uaRO.exe, 0 another
bool g_held = false;  // the night's darkness taken away by the driver

bool codeIs(uint32_t at, const uint8_t* bytes, size_t size) {
    uint8_t now[64] = {};
    SIZE_T got = 0;
    return size <= sizeof now &&
           ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(at)), now, size, &got) &&
           got == size && std::memcmp(now, bytes, size) == 0;
}

bool known() {
    if (g_known < 0) {
        g_known = codeIs(server_night::kWalk, server_night::kWalkBytes, server_night::kWalkSize) &&
                  codeIs(server_night::kForNight, server_night::kForNightBytes, server_night::kForNightSize);
        if (!g_known) LOG_INFO("server's night: the game's own (not where this build of uaRO.exe has it)");
    }
    return g_known == 1;
}

// Whether the player has the status now (the game's list, kept by its state-change packets)
bool hasStatus(int id) {
    const int* first = *reinterpret_cast<int* const*>(uintptr_t(server_night::kStatusFirst));
    const int* last = *reinterpret_cast<int* const*>(uintptr_t(server_night::kStatusLast));
    if (!first || last < first || last - first > 1024) return false;
    for (const int* p = first; p != last; ++p)
        if (*p == id) return true;
    return false;
}

}  // namespace

bool serverNightNow() { return known() && hasStatus(server_night::kNight); }

void serverNightFrame(bool leaveOut) {
    if ((!leaveOut && !g_held) || !known()) return;
    volatile uint8_t& dark = *reinterpret_cast<volatile uint8_t*>(uintptr_t(server_night::kDark));
    const bool held = g_held;
    const uint8_t now = server_night::darkNow(dark, hasStatus(server_night::kNight), hasStatus(server_night::kSpirit),
                                              leaveOut, g_held);
    if (now != dark) dark = now;
    if (g_held != held)
        LOG_INFO(g_held ? "server's night: its darkness left out (effects.no_server_night)"
                        : "server's night: as the game shows it");
}

}  // namespace uagfx::win
