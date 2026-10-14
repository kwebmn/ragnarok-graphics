#pragma once
#include <cstdint>
#include <unordered_map>

namespace uagfx::effect_exe {

// 0.27: which of uaRO.exe's effects a draw is of. An effect (CRagEffect, vtable kEffectVtable) keeps its id at
// kEffectId and draws through primitives (CEffectPrim, kPrimSize bytes) that point back at it from kPrimMaster (0
// once it is gone). A primitive's virtual Render (FUN_0061a050, thiscall: the view matrix; its vtable entry at
// kPrimRenderSlot) hands its faces to the renderer's face sort (FUN_00412020, thiscall: the face - its first field
// the address of its vertices - and its flags), straight or through CRenderer::AddRP; a sprite's face comes from the
// renderer's own pool. The renderer draws each face from those vertices at the end of the frame (FUN_0040f0a0,
// FUN_0040fb60: DrawPrimitive, FVF 0x1C4).
constexpr uint32_t kPrimRender = 0x61A050;
constexpr uint32_t kPrimRenderSlot = 0x69FBD4;  // CEffectPrim's vtable 0x69FBC8, entry 3
constexpr uint32_t kPrimSize = 0xF9E4;
constexpr uint32_t kPrimMaster = 0xF8;
constexpr uint32_t kEffectVtable = 0x69F9CC;
constexpr uint32_t kEffectId = 0x100;
// 0.37: an effect's own Render (FUN_005c25a0, thiscall: the view matrix) - its STR animation's layers; its vtable
// entry.
constexpr uint32_t kEffectRender = 0x5C25A0;
constexpr uint32_t kEffectRenderSlot = 0x69F9D8;  // CRagEffect's vtable 0x69F9CC, entry 3
constexpr uint32_t kSortFace = 0x412020;
// 0.28: whether these 6 bytes at kSortFace are the expected first instructions (push ebp; mov ebp, esp; sub esp,
// 0xC), moved whole into the hook's trampoline: else the exe is another build.
bool isSortEntry(const uint8_t bytes[6]);
// 0.32: the game's shadow under a character (FUN_00547b80, thiscall: the actor; the view matrix and a scale off the
// stack): a sprite of shadow.spr at the feet, its face from the renderer's pool. Its entry's first six bytes (push
// ebp; mov ebp, esp; sub esp, 0x64), likewise.
constexpr uint32_t kActorShadow = 0x547B80;
bool isShadowEntry(const uint8_t bytes[6]);

// The effects the driver does something with.
// Shadow: a character's (not an effect); 0.37: Skill - any other effect the game shows (skills, items, level ups).
enum class Kind : uint8_t { None, Aura, Firefly, Ray, Shadow, Skill };
// The level 99 aura: 200, 201 and 202 - its ring (effect\ring_blue.tga), bubbles (effect\pikapika2.bmp) and circle
// (effect\freezing_circle.bmp) - and 397, 398 and 362, those of the transcendent classes (362: effect\whitelight.tga).
// The game shows the circles alone unless /aura is on (0x772CE4, 0 at start, not kept): it reads that when it makes
// an aura - a map entered, a player come into view - not when the command is given. 0.28: 45, the maps' fireflies
// (a sprite primitive: 이팩트\particle1.spr, a soft white dot the game tints). 0.30: 307, 322, 323, 324, the
// forest's light (forestlight 1-4: primitive 0x42, effect\cloud11.tga on four five-sided tubes down to the ground).
Kind kindOf(int id);

// A draw's TL vertices (`stride` bytes each, the diffuse colour at byte 16) made `percent` % as bright: their colour
// and alpha each by its square root - the game's draws multiply the texture by both, and an effect adds its light as
// colour times alpha.
void soften(uint8_t* vertices, uint32_t count, uint32_t stride, int percent);

// What a draw is of, and a seed of its primitive's own (its fireflies flash each its own way).
struct Tag {
    Kind kind = Kind::None;
    uint8_t seed = 0;
};

// The faces of the effects the driver cares about sorted this frame, by the address of their vertices.
class Faces {
public:
    void note(uint32_t vertices, Tag tag);
    Tag of(uint32_t vertices) const;
    void clear();  // a new frame: a face's memory may hold another one by then

private:
    std::unordered_map<uint32_t, Tag> faces_;
};

}  // namespace uagfx::effect_exe
