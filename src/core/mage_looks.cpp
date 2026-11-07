#include "core/mage_looks.hpp"

namespace uagfx::magelook {

Element elementOf(int id) {
    switch (id) {
        // fire: Fire Bolt, Fire Ball, Fire Wall, its hits, Fire Pillar, Meteor Storm, Sightrasher, Fire Ivy, the
        // endows and Volcano, Flame Launcher
        case 24: case 25: case 31: case 49: case 50: case 62: case 92: case 96: case 97: case 118: case 138:
        case 173: case 176: case 225: case 255:
            return Element::Fire;
        // water: Cold Bolt, Frost Diver, its hit, Ice Wall, Storm Gust, Water Ball, Frost Nova (ice crash), the
        // endow, Deluge, Frost Weapon
        case 26: case 27: case 28: case 51: case 74: case 89: case 116: case 117: case 135: case 174: case 236:
        case 256:
            return Element::Water;
        // wind: Lightning Bolt, Thunderstorm, its hit, Lord of Vermilion, Jupitel Thunder, the endow, Violent Gale,
        // Lightning Loader, the electric hits
        case 29: case 30: case 52: case 90: case 93: case 94: case 175: case 237: case 257: case 432: case 433:
        case 452:
            return Element::Wind;
        // earth: Stone Curse, Earth Spike, Quagmire, Heaven's Drive (crash earth), its hit, the endow, Land
        // Protector, Seismic Weapon
        case 23: case 79: case 95: case 102: case 142: case 147: case 177: case 238: case 258:
            return Element::Earth;
        // ghost and the rest: Safety Wall, Soul Strike, Sight, Napalm Beat, Energy Coat, the magic hit, Spell
        // Breaker, Dispel, Magic Rod, Absorb Spirits, Magic Crasher, Napalm Vulcan, Soul Burn, True Sight
        case 13: case 15: case 22: case 32: case 169: case 182: case 234: case 235: case 244: case 253: case 315:
        case 380: case 386: case 401: case 403: case 406: case 451:
            return Element::Spirit;
        default:
            return Element::None;
    }
}

Palette paletteOf(Element e) {
    switch (e) {
        case Element::Fire: return {{1.0f, 0.93f, 0.72f}, {1.0f, 0.45f, 0.12f}, {0.55f, 0.16f, 0.05f}};
        case Element::Water: return {{1.0f, 1.0f, 1.0f}, {0.62f, 0.80f, 1.0f}, {0.28f, 0.52f, 0.84f}};
        case Element::Wind: return {{1.0f, 1.0f, 0.88f}, {0.78f, 0.62f, 1.0f}, {0.36f, 0.26f, 0.66f}};
        case Element::Earth: return {{1.0f, 0.95f, 0.82f}, {0.78f, 0.60f, 0.36f}, {0.36f, 0.26f, 0.16f}};
        case Element::Spirit: return {{1.0f, 1.0f, 1.0f}, {0.84f, 0.78f, 1.0f}, {0.42f, 0.36f, 0.62f}};
        default: return {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.5f}};
    }
}

Part partOf(bool blend, uint32_t srcBlend, uint32_t dstBlend, uint32_t vertexCount, float width, float screenWidth) {
    if (!blend) return Part::None;
    if (dstBlend == 6 && vertexCount == 3) return Part::Solid;  // INVSRCALPHA: a solid's faces (Storm Gust's spikes)
    if ((srcBlend == 5 || srcBlend == 2) && dstBlend == 2)       // SRCALPHA or ONE, ONE: added on
        return width >= kHazeWidth * screenWidth ? Part::Haze : Part::Layer;
    return Part::None;
}

}  // namespace uagfx::magelook
