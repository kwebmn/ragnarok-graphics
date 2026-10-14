#include "win/effect_hook.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "win/context.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

using effect_exe::Kind;
using effect_exe::Tag;

// CEffectPrim::Render's own convention: the primitive in ECX, the view matrix the callee takes off the stack.
using RenderFn = void(__attribute__((thiscall)) *)(uint32_t prim, void* matrix);
// The face sort's: the renderer in ECX, the face and its flags the callee takes off the stack.
using SortFn = void(__attribute__((thiscall)) *)(uint32_t renderer, uint32_t face, uint32_t flags);

uintptr_t g_render = 0;  // the Render the vtable held
uintptr_t g_sort = 0;    // 0.28: the face sort's first instructions, then a jump to the rest of it
// The game's thread renders its primitives, which sort their faces then and there: what the primitive being rendered
// is of, while it renders.
Tag g_current;
// Under the driver's lock: the faces are noted on the game's thread and looked up when it draws them; a present
// (Bink's too, from its own thread) starts the next frame's.
effect_exe::Faces g_faces;

void __attribute__((thiscall)) primRender(uint32_t prim, void* matrix) {
    // The effect it is of, as the game's own update reads it (FUN_00611bf0: 0 once the effect is gone).
    const uint32_t master = *reinterpret_cast<const uint32_t*>(uintptr_t(prim) + effect_exe::kPrimMaster);
    Tag tag;
    if (master && *reinterpret_cast<const uint32_t*>(uintptr_t(master)) == effect_exe::kEffectVtable) {
        tag.kind = effect_exe::kindOf(*reinterpret_cast<const int*>(uintptr_t(master) + effect_exe::kEffectId));
        tag.seed = uint8_t((prim >> 4) * 2654435761u >> 24);  // the primitive's own
    }
    g_current = tag;
    reinterpret_cast<RenderFn>(g_render)(prim, matrix);
    g_current = Tag{};
}

void __attribute__((thiscall)) sortFace(uint32_t renderer, uint32_t face, uint32_t flags) {
    if (g_current.kind != Kind::None && face) {
        const uint32_t vertices = *reinterpret_cast<const uint32_t*>(uintptr_t(face));
        Guard guard;
        g_faces.note(vertices, g_current);
    }
    reinterpret_cast<SortFn>(g_sort)(renderer, face, flags);
}

// 0.28: the face sort's entry jumps to sortFace; its first six bytes, whole instructions, run in a trampoline of
// their own before the rest of it.
bool hookSort() {
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(effect_exe::kSortFace));
    uint8_t bytes[6] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), at, bytes, sizeof bytes, &got) || got != sizeof bytes ||
        !effect_exe::isSortEntry(bytes))
        return false;
    auto* t = static_cast<uint8_t*>(VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!t) return false;
    std::memcpy(t, bytes, sizeof bytes);
    t[6] = 0xE9;  // jmp kSortFace + 6
    const int32_t back = int32_t(uintptr_t(effect_exe::kSortFace) + 6 - (reinterpret_cast<uintptr_t>(t) + 11));
    std::memcpy(t + 7, &back, sizeof back);
    FlushInstructionCache(GetCurrentProcess(), t, 16);
    DWORD old = 0;
    if (!VirtualProtect(at, sizeof bytes, PAGE_EXECUTE_READWRITE, &old)) {
        VirtualFree(t, 0, MEM_RELEASE);
        return false;
    }
    g_sort = reinterpret_cast<uintptr_t>(t);
    uint8_t jump[6] = {0xE9, 0, 0, 0, 0, 0x90};  // jmp sortFace; nop
    const int32_t to = int32_t(reinterpret_cast<uintptr_t>(&sortFace) - (uintptr_t(effect_exe::kSortFace) + 5));
    std::memcpy(jump + 1, &to, sizeof to);
    std::memcpy(at, jump, sizeof jump);
    DWORD unused = 0;
    VirtualProtect(at, sizeof bytes, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, sizeof bytes);
    return true;
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
        LOG_INFO("effects: the game's effects stay as it draws them (the level 99 aura, the fireflies) - they are not "
                 "where this build of uaRO.exe has them");
        return false;
    }
    // The sort first: without it no draw is known, and the primitives are best left alone.
    if (!hookSort()) {
        LOG_INFO("effects: the game's effects stay as it draws them (the level 99 aura, the fireflies) - its face "
                 "sort is not where this build of uaRO.exe has it");
        return false;
    }
    if (!VirtualProtect(slot, sizeof now, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("effects: the game's effects stay as it draws them - the game's code cannot be changed (error %lu)",
                 GetLastError());
        return false;
    }
    g_render = now;
    *slot = reinterpret_cast<uintptr_t>(&primRender);
    DWORD unused = 0;
    VirtualProtect(slot, sizeof now, old, &unused);
    done = 1;
    LOG_INFO("effects: the game's effects known by their draws (uaRO.exe 0x%X, 0x%X) - the level 99 aura, the "
             "fireflies", effect_exe::kPrimRender, effect_exe::kSortFace);
    return true;
}

Tag drawnEffect(const void* vertices) {
    return g_render ? g_faces.of(uint32_t(reinterpret_cast<uintptr_t>(vertices))) : Tag{};
}

void effectFrame() { g_faces.clear(); }

}  // namespace uagfx::win
