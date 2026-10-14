#include "win/effect_hook.hpp"

#include <windows.h>

#include <cstdint>

#include "win/context.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

using effect_exe::Kind;

// CEffectPrim::Render's own convention: the primitive in ECX, the view matrix the callee takes off the stack.
using RenderFn = void(__attribute__((thiscall)) *)(uint32_t prim, void* matrix);

uintptr_t g_render = 0;  // the Render the vtable held
// Under the driver's lock: the game's thread renders its primitives and draws them, a present (Bink's too, from its
// own thread) starts the next frame's.
effect_exe::Prims g_prims;

void __attribute__((thiscall)) primRender(uint32_t prim, void* matrix) {
    // The effect it is of, as the game's own update reads it (FUN_00611bf0: 0 once the effect is gone).
    const uint32_t master = *reinterpret_cast<const uint32_t*>(uintptr_t(prim) + effect_exe::kPrimMaster);
    if (master && *reinterpret_cast<const uint32_t*>(uintptr_t(master)) == effect_exe::kEffectVtable) {
        const Kind kind = effect_exe::kindOf(*reinterpret_cast<const int*>(uintptr_t(master) + effect_exe::kEffectId));
        if (kind != Kind::None) {
            Guard guard;
            g_prims.note(prim, kind);
        }
    }
    reinterpret_cast<RenderFn>(g_render)(prim, matrix);
}

}  // namespace

bool installEffectHook() {
    static int done = -1;  // once: 1 hooked, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    auto* slot = reinterpret_cast<uintptr_t*>(uintptr_t(effect_exe::kPrimRenderSlot));
    uintptr_t now = 0;
    SIZE_T got = 0;
    DWORD old = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), slot, &now, sizeof now, &got) || got != sizeof now ||
        now != effect_exe::kPrimRender) {
        LOG_INFO("effects: the level 99 aura stays as the game draws it - its effects are not where this build of "
                 "uaRO.exe has them");
        return false;
    }
    if (!VirtualProtect(slot, sizeof now, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("effects: the level 99 aura stays as the game draws it - the game's code cannot be changed (error "
                 "%lu)", GetLastError());
        return false;
    }
    g_render = now;
    *slot = reinterpret_cast<uintptr_t>(&primRender);
    DWORD unused = 0;
    VirtualProtect(slot, sizeof now, old, &unused);
    done = 1;
    LOG_INFO("effects: the game's effects known by their draws (uaRO.exe 0x%X) - the level 99 aura",
             effect_exe::kPrimRender);
    return true;
}

effect_exe::Kind drawnEffect(const void* vertices) {
    return g_render ? g_prims.of(uint32_t(reinterpret_cast<uintptr_t>(vertices))) : Kind::None;
}

void effectFrame() { g_prims.clear(); }

}  // namespace uagfx::win
