#include "win/texture_hook.hpp"

#include <windows.h>

#include <atomic>
#include <cstddef>
#include <cstring>

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
std::atomic<bool> g_fast{true};
std::atomic<uint32_t> g_kept{0};

using MipsFn = void(__attribute__((thiscall)) *)(uint32_t texture, int* rect);

// 2.5.20: CTexture::Update (FUN_004176c0) as the game makes it - its fields, sizes and steps, the lock, the rows, the
// unlock, its mip levels by its own FUN_004175d0 - with the rows made by core/texture_exe (the same bits, SSE2).
void fastUpdate(uint32_t texture, int x, int y, int width, int height, const uint32_t* argb, uint32_t key,
                int pitch) {
    uint8_t* t = reinterpret_cast<uint8_t*>(uintptr_t(texture));
    uint32_t tw = 0, th = 0, fitW = uint32_t(width), fitH = uint32_t(height);
    std::memcpy(&tw, t + 4, 4);
    std::memcpy(&th, t + 8, 4);
    while (tw < fitW) fitW >>= 1;  // the picture's size the texture holds
    while (th < fitH) fitH >>= 1;
    std::memcpy(t + 0x18, &fitW, 4);
    std::memcpy(t + 0x1C, &fitH, 4);
    t[0x14] = uint8_t(key);
    IDirectDrawSurface7* s = nullptr;
    std::memcpy(&s, t + texture_exe::kSurface, sizeof s);
    if (!s || !argb) return;
    if (!pitch) pitch = width;
    DDSURFACEDESC2 d{};
    d.dwSize = sizeof d;
    if (FAILED(s->GetSurfaceDesc(&d))) return;
    int stepX = 1, stepY = 1;  // a lower texture detail: every step-th texel
    for (uint32_t w = uint32_t(width); d.dwWidth < w;) w = uint32_t(width / ++stepX);
    for (uint32_t h = uint32_t(height); d.dwHeight < h;) h = uint32_t(height / ++stepY);
    const int rows = height / stepY, cols = width / stepX;
    if (!rows || !cols) return;
    RECT r{x / stepX, y / stepY, x / stepX + cols, y / stepY + rows};
    if (FAILED(s->Lock(&r, &d, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr))) return;
    uint32_t format = 0;
    std::memcpy(&format, t + texture_exe::kFormat, 4);
    uint8_t* out = static_cast<uint8_t*>(d.lpSurface);
    const uint32_t* row = argb;
    for (int j = 0; j < rows; ++j, out += d.lPitch, row += size_t(stepY) * size_t(pitch))
        texture_exe::convertRow(format, row, stepX, cols, (key & 0xFF) != 0, out);
    s->Unlock(nullptr);
    int all[4] = {0, 0, int(tw), int(th)};
    reinterpret_cast<MipsFn>(uintptr_t(texture_exe::kMips))(texture, all);
}

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
    if (g_fast.load())
        fastUpdate(texture, x, y, width, height, argb, key, pitch);
    else
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
    LOG_INFO("full colour: the game's textures are seen in their pictures' own colours (uaRO.exe 0x%X)%s%s",
             texture_exe::kUpdate, g_on.load() ? "" : " - [quality] full_colour = 0: the 16-bit ones drawn",
             g_fast.load() ? "; made by the driver, the rows by SSE2" : "");
    return true;
}

void setFullColour(bool on) { g_on = on; }

void setFastTextures(bool on) { g_fast = on; }

uint32_t fullColourTextures() { return g_kept.load(); }

}  // namespace uagfx::win
