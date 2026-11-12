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

}  // namespace uagfx::keywalk
