#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

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
// 2.7.27: a primitive's height over its effect's place (a float, y down positive: FUN_00612de0 adds it to the
// effect's) - Ruwach's orbs -10, their shadows on the ground 0 (FUN_005CC5D0)
constexpr uint32_t kPrimLift = 0x35C;
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
// 0.38: Portal - the warp NPC's portal (321, warpzone2: rings of effect\ring_blue.tga), the skill's and the like.
// 2.6.18: Darkness - blindness's (334) and the darkness's levels' (475-484) square round the player
// (core/blind_exe.hpp): not a skill made softer, laid over the world once its effects are drawn.
// 2.7.13: HitFlash - not the game's: a monster's sprite drawn again in white over itself, as it is hit (Device7);
// 2.7.14: Glint - a card's star on the ground (Device7), likewise
enum class Kind : uint8_t { None, Aura, Firefly, Ray, Shadow, Skill, Portal, Darkness, HitFlash, Glint, Panel };
// 3.0.3-beta.8: Panel - the driver's panel of the game windows (win/window_panel), a picture already at the
// target's pixels: sampled as it is, no interface filter
// The level 99 aura: 200, 201 and 202 - its ring (effect\ring_blue.tga), bubbles (effect\pikapika2.bmp) and circle
// (effect\freezing_circle.bmp) - and 397, 398 and 362, those of the transcendent classes (362: effect\whitelight.tga).
// The game shows the circles alone unless /aura is on (0x772CE4, 0 at start, not kept): it reads that when it makes
// an aura - a map entered, a player come into view - not when the command is given. 0.28: 45, the maps' fireflies
// (a sprite primitive: 이팩트\particle1.spr, a soft white dot the game tints). 0.30: 307, 322, 323, 324, the
// forest's light (forestlight 1-4: primitive 0x42, effect\cloud11.tga on four five-sided tubes down to the ground).
Kind kindOf(int id);

// 0.67, soft particles: the world's blended draws that fade into what is behind them - added light, a skill's or a
// portal's picture. 2.7.13: not a hit flash (Kind::HitFlash) - the frame's depth copied with the monster in it put it
// out wholly (the tester: «не вспыхивает», every hit given to its monster); 2.7.14: nor a card's star (Kind::Glint).
bool softEffect(bool addsLight, uint8_t kind);

// 0.39.1: how a draw is made softer, by how it blends (the game's draws multiply the texture by the vertices' colour
// and alpha): an added one (ONE, ONE) by its colour; one added or laid over by its alpha (SRCALPHA, ONE or
// INVSRCALPHA) by its alpha; one that multiplies the picture (ZERO, SRCCOLOR or DESTCOLOR, ZERO) towards white - which
// changes nothing; any other not at all. (0.37's scaled colour and alpha alike: a multiplying skill - Soul Strike -
// became a darker square.)
enum class Soften : uint8_t { None, Colour, Alpha, White };
Soften softenFor(bool blend, uint32_t srcBlend, uint32_t dstBlend);
// A draw's TL vertices (`stride` bytes each, the diffuse colour at byte 16) made `percent` % as strong, `how`.
void soften(uint8_t* vertices, uint32_t count, uint32_t stride, int percent, Soften how);
// 2.7.46: how a skill's draw is made softer - as softenFor, but one laid over by its alpha (SRCALPHA, INVSRCALPHA)
// not at all: it adds no light, made softer it only went see-through (the tester: «скилы ... сильно прозрачные»)
Soften softenSkill(bool blend, uint32_t srcBlend, uint32_t dstBlend);
// 2.7.38: whether a draw's added light is laid over softly - the target's colour its factor (1 - under), the lighter
// the picture under it the less it adds - with «Skills that do not blind» on: a skill's that adds light by ONE or by
// its alpha (premultiplied in the shader then)
bool softAdd(bool skillsSoft, bool addsLight, uint8_t kind, uint32_t srcBlend);
// 2.7.39: whether the game's effect `id` is a casting's - the light rising round the caster's feet as a skill is cast,
// one a kind of element (uaRO.exe's effect switch: 12 beginspell, 54-59 beginspell2-7)
bool isCasting(int id);
// How bright a casting is drawn, % of the game's, from the skills' (skillPercent): 40 % of it
int castPercent(int skillPercent);
// What that gives: `light` laid over `under` (0-1).
inline float laidOver(float under, float light) { return under + light * (1.0f - under); }

// What a draw is of, and a seed of its primitive's own (its fireflies flash each its own way).
struct Tag {
    Kind kind = Kind::None;
    uint8_t seed = 0;
    bool auraExtra = false;  // 0.63: the aura's ring or bubbles (auraExtra): what /aura shows
    uint16_t id = 0;         // 2.7.21: the effect's own number (kEffectId; 0 none) - Ruwach's orbs light by it
    bool raised = false;     // 2.7.27: its primitive lifted over the effect's place (kPrimLift) - not a shadow
};
// 0.63: the level 99 aura's parts /aura turns on - the ring and bubbles (200, 201; 397, 398 transcendent), not the
// circles (202, 362) the game always shows - and where the game keeps /aura (0 off; read only as an aura is made).
bool auraExtra(int id);
constexpr uint32_t kAuraFlag = 0x772CE4;

// 2.6.3: whose a sprite's face is. uaRO.exe asks for a character's round shadow (kActorShadow) right before it sorts
// the character's sprites (FUN_00548190, FUN_005aa130, the players' FUN_005ae540): the faces sorted after a shadow's
// call into the blended lists (flags bit 0), not an effect's, are that character's until the next shadow's - their
// tag kind None with the character's seed (the characters' shadows know its layers by it: actor_shadows::Layer).
// A character's seed, of its address; never 0 (nobody's).
uint8_t ownerSeed(uint32_t actor);
class Owner {
public:
    void shadow(uint32_t actor);  // the game asks for a character's shadow
    // The tag of a face sorted now with `flags`: an effect's own (a shadow's too); a blended one after a character's
    // shadow, the character's; else none.
    Tag faceTag(Tag current, uint32_t flags) const;
    void clear();  // a new frame: nobody's yet

private:
    uint8_t seed_ = 0;
};

// The faces of the effects the driver cares about (2.6.3: and of the characters' sprites) sorted this frame, by the
// address of their vertices. 2.5.20: a flat table cleared by the frame's stamp - asked at every draw of the game's, it
// was a map whose nodes were made and freed every frame.
class Faces {
public:
    void note(uint32_t vertices, Tag tag);
    Tag of(uint32_t vertices) const;
    void clear();  // a new frame: a face's memory may hold another one by then

private:
    struct Slot {
        uint32_t vertices = 0;
        uint32_t stamp = 0;  // the frame it was noted in: of another frame, the slot is free
        Tag tag;
    };
    size_t slotOf(uint32_t vertices) const;  // its slot, or the free one it would take
    void grow();
    std::vector<Slot> slots_;  // a power of two of them, at most half taken
    uint32_t stamp_ = 1;
    size_t count_ = 0;
};

}  // namespace uagfx::effect_exe
