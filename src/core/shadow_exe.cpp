#include "core/shadow_exe.hpp"

#include <cstring>

namespace uagfx::shadow_exe {

const uint8_t kOriginal[kOriginalSize] = {
    0xB9, 0x78, 0xB5, 0x74, 0x00,  // mov ecx, 0x74b578
    0xE8, 0x05, 0xEF, 0xFA, 0xFF,  // call 0x4f6ad0
};

const uint8_t kWayOutCode[kWayOutSize] = {0x5F, 0x5E, 0x5B, 0x8B, 0xE5, 0x5D, 0xC2, 0x08, 0x00};

Guard guard(uint32_t) {
    Guard g{};
    uint8_t* c = g.code;
    c[0] = 0xB8;  // mov eax, GetGameMode
    std::memcpy(c + 1, &kGetGameMode, 4);
    c[5] = 0xFF, c[6] = 0xD0;   // call eax
    c[7] = 0x85, c[8] = 0xC0;   // test eax, eax
    c[9] = 0x75, c[10] = 0x07;  // jnz the ret
    c[11] = 0xC7, c[12] = 0x04, c[13] = 0x24;  // mov dword [esp], kWayOut
    std::memcpy(c + 14, &kWayOut, 4);
    c[18] = 0xC3;  // ret
    return g;
}

void callTo(uint32_t at, uint8_t out[5]) {
    out[0] = 0xE8;
    const int32_t rel = int32_t(at - (kCall + 5));
    std::memcpy(out + 1, &rel, 4);
}

}  // namespace uagfx::shadow_exe
