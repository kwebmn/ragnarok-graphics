#include "win/sprite_hook.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "core/sprite_exe.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

// CTexture::UpdateSprite's own convention: the texture in ECX, six arguments the callee takes off the stack.
using WriteFn = void(__attribute__((thiscall)) *)(uint32_t texture, int x, int y, unsigned width, int height,
                                                   void* image, void* palette);

bool g_writing = false;  // the game's thread only: it draws and writes its sprites itself
Rect g_cell;

void __attribute__((thiscall)) spriteWrite(uint32_t texture, int x, int y, unsigned width, int height, void* image,
                                           void* palette) {
    g_cell = Rect{x, y, x + int(width), y + height};
    g_writing = true;
    reinterpret_cast<WriteFn>(uintptr_t(sprite_exe::kWrite))(texture, x, y, width, height, image, palette);
    g_writing = false;
}

}  // namespace

bool installSpriteHook() {
    static int done = -1;  // once: 1 hooked, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(sprite_exe::kWriteCall));
    uint8_t bytes[5] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), at, bytes, sizeof bytes, &got) || got != sizeof bytes ||
        !sprite_exe::isWriteCall(bytes)) {
        LOG_INFO("AI textures: sprites off - the game's sprite cache is not where this build of uaRO.exe has it");
        return false;
    }
    DWORD old = 0;
    if (!VirtualProtect(at, sizeof bytes, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("AI textures: sprites off - the game's code cannot be changed (error %lu)", GetLastError());
        return false;
    }
    const int32_t rel = int32_t(reinterpret_cast<uintptr_t>(&spriteWrite) - (uintptr_t(sprite_exe::kWriteCall) + 5));
    std::memcpy(at + 1, &rel, sizeof rel);
    DWORD unused = 0;
    VirtualProtect(at, sizeof bytes, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, sizeof bytes);
    done = 1;
    LOG_INFO("AI textures: the game's sprite frames are seen one by one (uaRO.exe 0x%X)", sprite_exe::kWriteCall);
    return true;
}

bool spriteCell(Rect& cell) {
    if (!g_writing) return false;
    cell = g_cell;
    return true;
}

}  // namespace uagfx::win
