#include "win/texture_hook.hpp"

#include <windows.h>

#include <atomic>
#include <cstddef>

#include "core/texture_exe.hpp"
#include "win/dx7/surface7.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

// CTexture::Update's own convention: the texture in ECX, seven arguments the callee takes off the stack (the key is a
// byte pushed as a dword - passed on as it came).
using UpdateFn = void(__attribute__((thiscall)) *)(uint32_t texture, int x, int y, int width, int height,
                                                    const uint32_t* argb, uint32_t key, int pitch);

uintptr_t g_update = 0;  // the method the vtable held
std::atomic<bool> g_on{true};
std::atomic<uint32_t> g_kept{0};

// [at, at + n) lies in committed, readable memory: the game's objects are read on its word.
bool readable(uintptr_t at, size_t n) {
    MEMORY_BASIC_INFORMATION m{};
    if (!at || !VirtualQuery(reinterpret_cast<const void*>(at), &m, sizeof m) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return false;
    return at + n <= reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
}

void __attribute__((thiscall)) textureUpdate(uint32_t texture, int x, int y, int width, int height,
                                             const uint32_t* argb, uint32_t key, int pitch) {
    reinterpret_cast<UpdateFn>(g_update)(texture, x, y, width, height, argb, key, pitch);
    if (!g_on.load() || !argb || width <= 0 || height <= 0) return;
    if (reinterpret_cast<uintptr_t>(__builtin_return_address(0)) == texture_exe::kWindowTilesReturn) return;
    if (!readable(texture, texture_exe::kSurface + 4)) return;
    IDirectDrawSurface7* surface =
        *reinterpret_cast<IDirectDrawSurface7* const*>(uintptr_t(texture) + texture_exe::kSurface);
    if (!surface || !readable(reinterpret_cast<uintptr_t>(surface), sizeof(dx7::Surface7))) return;
    dx7::Surface7* s = dx7::Surface7::from(surface);
    if (s && s->fullColour(x, y, width, height, argb, pitch ? pitch : width)) ++g_kept;
}

}  // namespace

bool installTextureHook() {
    static int done = -1;  // once: 1 hooked, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    auto* slot = reinterpret_cast<uintptr_t*>(uintptr_t(texture_exe::kUpdateSlot));
    uintptr_t now = 0;
    SIZE_T got = 0;
    DWORD old = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), slot, &now, sizeof now, &got) || got != sizeof now ||
        now != texture_exe::kUpdate) {
        LOG_INFO("full colour: off - the game's textures are not where this build of uaRO.exe has them");
        return false;
    }
    if (!VirtualProtect(slot, sizeof now, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("full colour: off - the game's code cannot be changed (error %lu)", GetLastError());
        return false;
    }
    g_update = now;
    *slot = reinterpret_cast<uintptr_t>(&textureUpdate);
    DWORD unused = 0;
    VirtualProtect(slot, sizeof now, old, &unused);
    done = 1;
    LOG_INFO("full colour: the game's textures are seen in their pictures' own colours (uaRO.exe 0x%X)%s",
             texture_exe::kUpdate, g_on.load() ? "" : " - [quality] full_colour = 0: the 16-bit ones drawn");
    return true;
}

void setFullColour(bool on) { g_on = on; }

uint32_t fullColourTextures() { return g_kept.load(); }

}  // namespace uagfx::win
