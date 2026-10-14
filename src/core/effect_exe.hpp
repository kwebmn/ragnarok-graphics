#pragma once
#include <cstdint>
#include <utility>
#include <vector>

namespace uagfx::effect_exe {

// 0.27: which of uaRO.exe's effects a draw is of. An effect (CRagEffect, vtable kEffectVtable) keeps its id at
// kEffectId and draws through primitives (CEffectPrim, kPrimSize bytes) that point back at it from kPrimMaster (0
// once it is gone). A primitive's virtual Render (FUN_0061a050, thiscall: the view matrix; its vtable entry at
// kPrimRenderSlot) puts its faces - their vertices inside the primitive - into the renderer's lists, and the
// renderer draws each face from those vertices at the end of the frame (FUN_0040f0a0: DrawPrimitive, FVF 0x1C4).
constexpr uint32_t kPrimRender = 0x61A050;
constexpr uint32_t kPrimRenderSlot = 0x69FBD4;  // CEffectPrim's vtable 0x69FBC8, entry 3
constexpr uint32_t kPrimSize = 0xF9E4;
constexpr uint32_t kPrimMaster = 0xF8;
constexpr uint32_t kEffectVtable = 0x69F9CC;
constexpr uint32_t kEffectId = 0x100;

// The effects the driver does something with.
enum class Kind : uint8_t { None, Aura };
// The level 99 aura: 200, 201 and 202 - its ring (effect\ring_blue.tga), bubbles (effect\pikapika2.bmp) and circle
// (effect\freezing_circle.bmp) - and 397, 398 and 362, those of the transcendent classes (362: effect\whitelight.tga).
// With /aura the game shows the circles alone.
Kind kindOf(int id);

// A draw's TL vertices (`stride` bytes each, the diffuse colour at byte 16) made `percent` % as bright: their colour
// and alpha each by its square root - the game's draws multiply the texture by both, and an effect adds its light as
// colour times alpha.
void soften(uint8_t* vertices, uint32_t count, uint32_t stride, int percent);

// The primitives of the effects the driver cares about whose faces went into this frame's lists: a draw whose
// vertices lie inside one of them is of its effect.
class Prims {
public:
    void note(uint32_t prim, Kind kind);
    Kind of(uint32_t vertices);  // None: in none of them
    void clear();                // a new frame: a primitive gone since may hold anything by now

private:
    std::vector<std::pair<uint32_t, Kind>> prims_;  // sorted by address when sorted_
    bool sorted_ = true;
};

}  // namespace uagfx::effect_exe
