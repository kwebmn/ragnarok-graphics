#include "core/server_night.hpp"

namespace uagfx::server_night {

const uint8_t kWalkBytes[kWalkSize] = {
    0xB9, 0xB8, 0x10, 0x77, 0x00,        // mov ecx, kDark
    0xE8, 0x88, 0x1F, 0x03, 0x00,        // call 0x5EAE10: the byte 0
    0x33, 0xDB,                          // xor ebx, ebx
    0xA1, 0x84, 0x84, 0x72, 0x00,        // mov eax, [kStatusFirst]
    0x85, 0xC0,                          // test eax, eax
    0x0F, 0x84, 0xD8, 0x09, 0x00, 0x00,  // je
    0x8B, 0x0D, 0x88, 0x84, 0x72, 0x00,  // mov ecx, [kStatusLast]
};

const uint8_t kForNightBytes[kForNightSize] = {
    0x3D, 0xA0, 0x00, 0x00, 0x00,  // cmp eax, kNight
    0x75, 0x10,                    // jne
    0xB9, 0xB8, 0x10, 0x77, 0x00,  // mov ecx, kDark
    0xE8, 0xE6, 0x17, 0x03, 0x00,  // call 0x5EAE00: the byte 1
};

uint8_t darkNow(uint8_t dark, bool night, bool spirit, bool leaveOut, bool& held) {
    if (leaveOut) {
        if (dark && night && !spirit) {
            held = true;
            return 0;
        }
        return dark;
    }
    if (!held) return dark;
    held = false;
    return night || spirit ? 1 : dark;
}

}  // namespace uagfx::server_night
