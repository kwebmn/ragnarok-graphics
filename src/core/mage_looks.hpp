#pragma once
#include <cstdint>

namespace uagfx::magelook {

// 3.0.15-beta.11 (the owner: all the mages' skills redrawn in one go, «сделай както так чтобы было удобно
// переключать», then «а можно без этого?» - no record of each): [effects] mage_effects. The Mage's, Wizard's and
// Sage's skill effects by their numbers (uAthena's db/const.txt EF_*) and element; each drawn by its parts as Storm
// Gust's were (3.0.15-beta.8, R-8X3RCC's record): a layer the game adds on laid over crisp in the element's colours,
// the widest soft, a solid face (three corners, laid over) lit by its own slope. Its switches: the whole and each
// element, the casting's own look too (core/cast_look.hpp).
enum class Element : uint8_t { None, Fire, Water, Wind, Earth, Spirit, Holy, Dark, Poison, Steel, Life, Gold };
// 3.0.15-beta.12 (the owner: «нравится, делай еффекты по всем остальным персонажам»): every class's skills - whose
// branch an effect is (its switch) and its colours' tone
enum class Branch : uint8_t { None, Mage, Sword, Acolyte, Archer, Thief, Merchant, Other,
                               // 3.0.16-beta.1 (the owner: «делай все»): the hits, the potions, buffs and items, the
                               // states, the travel (teleport, levels, refining), the monsters and homunculi
                               Hits, Items, Status, Travel, Monster };
struct Look {
    Branch branch = Branch::None;
    Element tone = Element::None;
};
Look lookOf(int effectId);
Element elementOf(int effectId);  // a mage's skill's element (Fire-Spirit), None for the rest

// The element's colours: the core (densest), the edge, the deep (a solid face away from the light)
struct Palette {
    float core[3], edge[3], deep[3];
};
Palette paletteOf(Element e);

// 3.0.16-beta.10 (the owner: «эффект крутящийся вокруг героя ... както не серьезно выглядит и в графику не заходит»,
// R-45TDN7: the crisp layer made the Monk's spirit spheres flat blue discs with a yellow dot): Orb - the spheres'
// own look, still added on as the game's: the big blue pass a soft warm aura, the small yellow one a lit glowing ball
// 3.0.16-beta.24 (R-P4DFMN: crisp - flat red slabs; .23 the game's own - «слишком тусклый огонь»): Flame - Fire Wall's
// soft flames added on as the game adds them, three times as bright and a little warmer
enum class Part : uint8_t { None, Layer, Haze, Solid, Orb, Flame };
constexpr int kFireWallEffect = 25;  // EF_FIREWALL
constexpr int kSpheresEffect = 228;  // the spirit spheres (packet 0x01d0's), EF_SPIRITBALL... the Monk's, the coins
constexpr float kHazeWidth = 0.25f;  // a layer this share of the screen wide or wider: soft
Part partOf(bool blend, uint32_t srcBlend, uint32_t dstBlend, uint32_t vertexCount, float width, float screenWidth);
Part partFor(int effectId, Part p);  // 3.0.16-beta.10: an added part of the spheres' effect - Orb

// DrawCmd::effectId of a part in the new look: 0xFE00 | part << 4 | element; 0 none
constexpr uint16_t kBase = 0xFE00;
inline uint16_t idOf(Part p, Element e) { return uint16_t(kBase | (uint16_t(p) << 4) | uint16_t(e)); }
inline bool isOurs(uint16_t id) { return (id & 0xFF00) == kBase; }
inline Part partOfId(uint16_t id) { return Part((id >> 4) & 15); }
inline Element elementOfId(uint16_t id) { return Element(id & 15); }

}  // namespace uagfx::magelook
