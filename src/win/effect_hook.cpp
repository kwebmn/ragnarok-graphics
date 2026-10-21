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
// 0.32: the characters' shadow's: the actor in ECX, the view matrix and a scale off the stack.
using ShadowFn = void(__attribute__((thiscall)) *)(uint32_t actor, uint32_t matrix, float scale);

uintptr_t g_render = 0;  // the Render the vtable held
uintptr_t g_sort = 0;    // 0.28: the face sort's first instructions, then a jump to the rest of it
uintptr_t g_shadow = 0;  // 0.32: the characters' shadow's, likewise
uintptr_t g_effect = 0;  // 0.37: the Render the effects' vtable held
// The game's thread renders its primitives, which sort their faces then and there: what the primitive being rendered
// is of, while it renders.
Tag g_current;
// Under the driver's lock: the faces are noted on the game's thread and looked up when it draws them; a present
// (Bink's too, from its own thread) starts the next frame's.
effect_exe::Faces g_faces;
// 2.6.3: the character whose shadow the game asked for last - its sprites come next (the characters' shadows).
effect_exe::Owner g_owner;

void __attribute__((thiscall)) primRender(uint32_t prim, void* matrix) {
    // The effect it is of, as the game's own update reads it (FUN_00611bf0: 0 once the effect is gone).
    const uint32_t master = *reinterpret_cast<const uint32_t*>(uintptr_t(prim) + effect_exe::kPrimMaster);
    Tag tag;
    if (master && *reinterpret_cast<const uint32_t*>(uintptr_t(master)) == effect_exe::kEffectVtable) {
        const int id = *reinterpret_cast<const int*>(uintptr_t(master) + effect_exe::kEffectId);
        tag.kind = effect_exe::kindOf(id);
        tag.auraExtra = effect_exe::auraExtra(id);
        tag.seed = uint8_t((prim >> 4) * 2654435761u >> 24);  // the primitive's own
        tag.id = id > 0 && id < 65536 ? uint16_t(id) : 0;      // 2.7.21
    }
    g_current = tag;
    reinterpret_cast<RenderFn>(g_render)(prim, matrix);
    g_current = Tag{};
}

// 0.37: while an effect renders its own animation (an STR: most of the big skills), its faces are its.
void __attribute__((thiscall)) effectRender(uint32_t effect, void* matrix) {
    const Tag before = g_current;
    const int id = *reinterpret_cast<const int*>(uintptr_t(effect) + effect_exe::kEffectId);
    g_current = Tag{effect_exe::kindOf(id), uint8_t((effect >> 4) * 2654435761u >> 24), effect_exe::auraExtra(id),
                    id > 0 && id < 65536 ? uint16_t(id) : uint16_t(0)};  // 2.7.21: its number too
    reinterpret_cast<RenderFn>(g_effect)(effect, matrix);
    g_current = before;
}

void __attribute__((thiscall)) sortFace(uint32_t renderer, uint32_t face, uint32_t flags) {
    const Tag tag = g_owner.faceTag(g_current, flags);  // 2.6.3: a character's sprite - the character's seed
    if ((tag.kind != Kind::None || tag.seed) && face) {
        const uint32_t vertices = *reinterpret_cast<const uint32_t*>(uintptr_t(face));
        Guard guard;
        g_faces.note(vertices, tag);
    }
    reinterpret_cast<SortFn>(g_sort)(renderer, face, flags);
}

// 0.32: while the game draws a character's shadow, its face is the character's shadow; 2.6.3: the character's
// sprites, sorted next, are its.
void __attribute__((thiscall)) actorShadow(uint32_t actor, uint32_t matrix, float scale) {
    const Tag before = g_current;
    g_owner.shadow(actor);
    g_current = Tag{Kind::Shadow, effect_exe::ownerSeed(actor)};
    reinterpret_cast<ShadowFn>(g_shadow)(actor, matrix, scale);
    g_current = before;
}

// 0.28: a function's entry (`entry`, its first six bytes whole instructions that `valid` knows) jumps to `hook`;
// those six bytes run in a trampoline of their own before the rest of it (`trampoline`).
bool hookEntry(uint32_t entry, bool (*valid)(const uint8_t*), const void* hook, uintptr_t& trampoline) {
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(entry));
    uint8_t bytes[6] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), at, bytes, sizeof bytes, &got) || got != sizeof bytes ||
        !valid(bytes))
        return false;
    auto* t = static_cast<uint8_t*>(VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!t) return false;
    std::memcpy(t, bytes, sizeof bytes);
    t[6] = 0xE9;  // jmp entry + 6
    const int32_t back = int32_t(uintptr_t(entry) + 6 - (reinterpret_cast<uintptr_t>(t) + 11));
    std::memcpy(t + 7, &back, sizeof back);
    FlushInstructionCache(GetCurrentProcess(), t, 16);
    DWORD old = 0;
    if (!VirtualProtect(at, sizeof bytes, PAGE_EXECUTE_READWRITE, &old)) {
        VirtualFree(t, 0, MEM_RELEASE);
        return false;
    }
    trampoline = reinterpret_cast<uintptr_t>(t);
    uint8_t jump[6] = {0xE9, 0, 0, 0, 0, 0x90};  // jmp hook; nop
    const int32_t to = int32_t(reinterpret_cast<uintptr_t>(hook) - (uintptr_t(entry) + 5));
    std::memcpy(jump + 1, &to, sizeof to);
    std::memcpy(at, jump, sizeof jump);
    DWORD unused = 0;
    VirtualProtect(at, sizeof bytes, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, sizeof bytes);
    return true;
}

}  // namespace

bool auraOff() {
    return g_render && *reinterpret_cast<volatile const int*>(uintptr_t(effect_exe::kAuraFlag)) == 0;
}

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
    if (!hookEntry(effect_exe::kSortFace, &effect_exe::isSortEntry, reinterpret_cast<const void*>(&sortFace),
                   g_sort)) {
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
    // 0.37: the effects' own animations (the big skills' STR); without it only their primitives are known.
    auto* own = reinterpret_cast<uintptr_t*>(uintptr_t(effect_exe::kEffectRenderSlot));
    uintptr_t held = 0;
    if (ReadProcessMemory(GetCurrentProcess(), own, &held, sizeof held, &got) && got == sizeof held &&
        held == effect_exe::kEffectRender && VirtualProtect(own, sizeof held, PAGE_EXECUTE_READWRITE, &old)) {
        g_effect = held;
        *own = reinterpret_cast<uintptr_t>(&effectRender);
        VirtualProtect(own, sizeof held, old, &unused);
    } else {
        LOG_INFO("effects: the effects' own animations are not known - the skills' primitives alone are");
    }
    // 0.32: the characters' shadows (their own faces then); without it they stay the game's round ones.
    if (!hookEntry(effect_exe::kActorShadow, &effect_exe::isShadowEntry, reinterpret_cast<const void*>(&actorShadow),
                   g_shadow))
        LOG_INFO("effects: the characters' shadows stay the game's - its shadow is not where this build has it");
    LOG_INFO("effects: the game's effects known by their draws (uaRO.exe 0x%X, 0x%X) - the level 99 aura, the "
             "fireflies", effect_exe::kPrimRender, effect_exe::kSortFace);
    return true;
}

Tag drawnEffect(const void* vertices) {
    return g_render ? g_faces.of(uint32_t(reinterpret_cast<uintptr_t>(vertices))) : Tag{};
}

void effectFrame() {
    g_faces.clear();
    g_owner.clear();
}

}  // namespace uagfx::win
