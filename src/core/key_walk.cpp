#include "core/key_walk.hpp"

#include "pad/core/logic/pad.hpp"

namespace uagfx::keywalk {

uint8_t dirOfVk(unsigned vk) {
    switch (vk) {
    case 0x26: return kUp;
    case 0x28: return kDown;
    case 0x25: return kLeft;
    case 0x27: return kRight;
    default: return 0;
    }
}

Vec vectorOf(uint8_t held) {
    const int x = ((held & kRight) ? 1 : 0) - ((held & kLeft) ? 1 : 0);
    const int y = ((held & kUp) ? 1 : 0) - ((held & kDown) ? 1 : 0);
    const float k = x != 0 && y != 0 ? 0.70710678f : 1.0f;
    return Vec{float(x) * k, float(y) * k};
}

bool arrowWalks(const Gate& g) { return g.enabled && g.inGame && g.chatEmpty && !g.modifier && !g.otherField; }

uint16_t windowButtonOf(unsigned vk, bool shift) {
    switch (vk) {
    case 0x26: return uapad::kPadUp;     // VK_UP
    case 0x28: return uapad::kPadDown;   // VK_DOWN
    case 0x25: return uapad::kPadLeft;   // VK_LEFT
    case 0x27: return uapad::kPadRight;  // VK_RIGHT
    case 0x0D: return uapad::kPadA;      // VK_RETURN
    case 0x1B: return uapad::kPadB;      // VK_ESCAPE
    case 0x21: return uapad::kPadLB;     // VK_PRIOR
    case 0x22: return uapad::kPadRB;     // VK_NEXT
    case 0x5D: return uapad::kPadY;      // VK_APPS
    case 0x79: return shift ? uapad::kPadY : 0;  // VK_F10
    default: return 0;
    }
}

uint8_t heldNow(uint8_t dirs, KeyDown down) {
    static constexpr struct {
        uint8_t dir;
        unsigned vk;
    } kKeys[] = {{kUp, 0x26}, {kDown, 0x28}, {kLeft, 0x25}, {kRight, 0x27}};
    uint8_t now = 0;
    for (const auto& k : kKeys)
        if ((dirs & k.dir) && down(k.vk)) now = uint8_t(now | k.dir);
    return now;
}

uint16_t windowButtonsNow(uint16_t buttons, KeyDown down) {
    static constexpr struct {
        uint16_t button;
        unsigned vk;
    } kKeys[] = {{uapad::kPadUp, 0x26},    {uapad::kPadDown, 0x28}, {uapad::kPadLeft, 0x25}, {uapad::kPadRight, 0x27},
                 {uapad::kPadA, 0x0D},     {uapad::kPadB, 0x1B},    {uapad::kPadLB, 0x21},   {uapad::kPadRB, 0x22},
                 {uapad::kPadY, 0x5D},     {uapad::kPadY, 0x79}};  // Y: the menu key or (Shift+) F10
    uint16_t now = 0;
    for (const auto& k : kKeys)
        if ((buttons & k.button) && down(k.vk)) now = uint16_t(now | k.button);
    return now;
}

}  // namespace uagfx::keywalk
