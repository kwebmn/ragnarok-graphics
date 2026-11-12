#include "core/key_walk.hpp"

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

bool arrowWalks(const Gate& g) { return g.enabled && g.inGame && g.chatEmpty && !g.modifier; }

}  // namespace uagfx::keywalk
