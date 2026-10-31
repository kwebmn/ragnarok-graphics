#include "win/windows_draw.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "win/char_block.hpp"
#include "win/context.hpp"
#include "win/chat_window.hpp"
#include "win/hotbar.hpp"
#include "win/item_window.hpp"
#include "win/log.hpp"
#include "win/equip_window.hpp"
#include "win/mob_hp.hpp"
#include "win/npc_talk.hpp"
#include "win/shop_window.hpp"
#include "win/skill_window.hpp"
#include "win/stat_window.hpp"

namespace uagfx::win {
namespace {

// uaRO.exe's: the windows' manager's draw (ECX the manager, g_windowMgr 0x727ED8; it draws each window of its list
// with its shown flag +0x38 set) and its call in the game mode's frame
using DrawFn = void(__attribute__((thiscall)) *)(uint32_t manager);
constexpr uint32_t kDraw = 0x4CD720, kDrawCall = 0x559972;
struct Known {
    uint32_t at;
    const char* bytes;
    size_t size;
};
const Known kKnown[] = {
    {0x55996D, "\xB9\xD8\x7E\x72\x00\xE8\xA9\x3D\xF7\xFF", 10},  // mov ecx,manager; call the draw
    {0x4CD720, "\x55\x8B\xEC\x83\xEC\x20\x8B\x41\x38\x57", 10},  // the draw's: its windows' list
};
int g_told = 0;

void __attribute__((thiscall)) windowsDraw(uint32_t manager) {
    itemswin::beforeGameDraw();   // «Речі»
    statswin::beforeGameDraw();   // «Характеристики»
    skillswin::beforeGameDraw();  // 3.0.11-beta.96: «Навички»
    equipwin::beforeGameDraw();   // 3.0.11-beta.97: «Спорядження»
    npcwin::beforeGameDraw();     // 3.0.11-beta.98: the NPC talk
    shopwin::beforeGameDraw();    // 3.0.11-beta.100: the shop (its deal's choice pressed)
    mobhpwin::beforeGameDraw();   // 3.0.11-beta.100: the monsters' per cent asked
    charpanel::beforeGameDraw();  // the Basic Info window
    hotbarwin::beforeGameDraw();  // the hotkeys' bar
    chatwin::beforeGameDraw();    // the chat (out of sight)
    Context::get().markGameWindows();  // 3.0.11-beta.109: its own windows from here on - over the driver's
    if (g_told < 1) {
        ++g_told;
        LOG_INFO("windows draw: the game's replaced windows hidden before it draws its windows (uaRO.exe 0x%X)",
                 kDrawCall);
    }
    reinterpret_cast<DrawFn>(uintptr_t(kDraw))(manager);
}

}  // namespace

bool installWindowsDraw() {
    static int done = -1;  // once: 1 hooked, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    for (const Known& k : kKnown) {
        uint8_t bytes[16] = {};
        SIZE_T got = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(k.at)), bytes, k.size,
                               &got) ||
            got != k.size || std::memcmp(bytes, k.bytes, k.size) != 0) {
            LOG_INFO("windows draw: the game's code is not where this build of uaRO.exe has it (0x%X) - its replaced "
                     "windows hidden at the scene's end, as before", k.at);
            return false;
        }
    }
    auto* rel = reinterpret_cast<uint8_t*>(uintptr_t(kDrawCall + 1));
    DWORD old = 0;
    if (!VirtualProtect(rel, 4, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("windows draw: the game's code cannot be changed (error %lu)", GetLastError());
        return false;
    }
    const int32_t by = int32_t(reinterpret_cast<uintptr_t>(&windowsDraw) - (uintptr_t(kDrawCall) + 5));
    std::memcpy(rel, &by, sizeof by);
    DWORD unused = 0;
    VirtualProtect(rel, 4, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), rel, 4);
    done = 1;
    return true;
}

}  // namespace uagfx::win
