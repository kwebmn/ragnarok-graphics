#include "win/chat_patch.hpp"

#include <windows.h>

#include <cstring>

#include "core/chat_exe.hpp"
#include "win/log.hpp"

namespace uagfx::win {

bool installChatFix() {
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(chat_exe::kCheck));
    uint8_t now[chat_exe::kSize] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), at, now, sizeof now, &got) || got != sizeof now ||
        std::memcmp(now, chat_exe::kOriginal, sizeof now) != 0) {
        LOG_INFO("chat: the game's own check (it is not where this build of uaRO.exe has it)");
        return false;
    }
    DWORD old = 0;
    if (!VirtualProtect(at, chat_exe::kSize, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("chat: the game's own check (its code cannot be changed, error %lu)", GetLastError());
        return false;
    }
    std::memcpy(at, chat_exe::kFixed, chat_exe::kSize);
    DWORD unused = 0;
    VirtualProtect(at, chat_exe::kSize, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, chat_exe::kSize);
    LOG_INFO("chat: a line sent while a map loads no longer crashes the game (uaRO.exe 0x%X)", chat_exe::kCheck);
    return true;
}

}  // namespace uagfx::win
