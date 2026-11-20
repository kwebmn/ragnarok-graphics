#include "core/aura_exe.hpp"

#include <cstring>

namespace uagfx::aura_exe {

const uint8_t kOriginal[kOriginalSize] = {
    0xB9, 0x78, 0xB5, 0x74, 0x00,        // mov ecx, 0x74b578
    0xE8, 0xB1, 0x41, 0xFA, 0xFF,        // call 0x4f6ad0 (GetGameMode)
    0x8B, 0x80, 0xC4, 0x00, 0x00, 0x00,  // mov eax, [eax+0xc4] (its world)
    0x8B, 0x48, 0x3C,                    // mov ecx, [eax+0x3c] (the world's player)
    0xE8, 0x43, 0x65, 0x06, 0x00,        // call 0x5b8e70
};

Guard guard(uint32_t at) {
    Guard g{};
    uint8_t* c = g.code;
    c[0] = 0x85, c[1] = 0xC9;  // test ecx, ecx
    c[2] = 0x74, c[3] = 0x05;  // jz the ret
    c[4] = 0xE9;               // jmp the game's function
    const int32_t rel = int32_t(kCallee - (at + 9));
    std::memcpy(c + 5, &rel, 4);
    c[9] = 0xC3;  // ret
    return g;
}

void callTo(uint32_t at, uint8_t out[5]) {
    out[0] = 0xE8;
    const int32_t rel = int32_t(at - (kCall + 5));
    std::memcpy(out + 1, &rel, 4);
}

}  // namespace uagfx::aura_exe
