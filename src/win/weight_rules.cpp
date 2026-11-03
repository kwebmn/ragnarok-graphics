#include "win/weight_rules.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "core/weight_rules.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win::weightrules {
namespace {

namespace wr = uagfx::weightrules;

// uaRO.exe's: the weight update's two comparisons with 50 % (cmp esi, 0x32 / jl / cmp edi, 0x32 / jge) and
// msgstringtable's line by its number (cdecl)
const uint8_t kCode[] = {0x83, 0xFE, 0x32, 0x7C, 0x38, 0x83, 0xFF, 0x32, 0x7D, 0x33};
constexpr uint32_t kMsgString = 0x504FB0;
using MsgStringFn = const char*(__cdecl*)(int line);

bool g_installed = false, g_lineDone = false;
ULONGLONG g_lineTriedAt = 0;

bool writeByte(uint32_t at, uint8_t v) {
    DWORD old = 0;
    auto* p = reinterpret_cast<uint8_t*>(uintptr_t(at));
    if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) return false;
    *p = v;
    DWORD unused = 0;
    VirtualProtect(p, 1, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), p, 1);
    return true;
}

}  // namespace

bool install() {
    if (g_installed) return true;
    uint8_t got[sizeof kCode] = {};
    if (!readGame(wr::kFirstCmp, got, sizeof got) || std::memcmp(got, kCode, sizeof kCode) != 0) {
        LOG_INFO("weight: the game's 50 %% line is not where this build of uaRO.exe has it - left as it is");
        return false;
    }
    g_installed =
        writeByte(wr::kFirstCmp + 2, uint8_t(wr::kPerCent)) && writeByte(wr::kFirstCmp + 7, uint8_t(wr::kPerCent));
    LOG_INFO("weight: the game's line about HP and SP not coming back at %d %% of the weight, as uAthena on uaRO "
             "(uaRO.exe 0x%X)%s",
             wr::kPerCent, wr::kFirstCmp, g_installed ? "" : " - not changed");
    return g_installed;
}

void frame() {
    if (!g_installed || g_lineDone) return;
    const ULONGLONG now = GetTickCount64();
    if (now - g_lineTriedAt < 1000) return;  // the table loaded with the game's start: looked at once a second
    g_lineTriedAt = now;
    const char* line = reinterpret_cast<MsgStringFn>(uintptr_t(kMsgString))(wr::kLine);
    char text[256] = {};
    if (!line || !readGame(uint32_t(reinterpret_cast<uintptr_t>(line)), text, sizeof text - 1) || !text[0]) return;
    std::string words(text, strnlen(text, sizeof text - 1));
    const std::string was = words;
    g_lineDone = true;
    if (!wr::seventy(words)) return;
    SIZE_T n = 0;
    const bool written =
        WriteProcessMemory(GetCurrentProcess(), const_cast<char*>(line), words.data(), words.size(), &n) &&
        n == words.size();
    LOG_INFO("weight: msgstringtable's line 0x%X %s - «%s»", wr::kLine, written ? "says 70 %" : "not changed",
             words.c_str());
}

}  // namespace uagfx::win::weightrules
