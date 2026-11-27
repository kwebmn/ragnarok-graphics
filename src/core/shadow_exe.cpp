#include "core/shadow_exe.hpp"

#include <cstring>

namespace uagfx::shadow_exe {

const uint8_t kOriginal[kOriginalSize] = {
    0xB9, 0x78, 0xB5, 0x74, 0x00,  // mov ecx, 0x74b578
    0xE8, 0x05, 0xEF, 0xFA, 0xFF,  // call 0x4f6ad0
};

const uint8_t kWayOutCode[kWayOutSize] = {0x5F, 0x5E, 0x5B, 0x8B, 0xE5, 0x5D, 0xC2, 0x08, 0x00};

Guard guard(uint32_t) { return guardFor(kWayOut); }

Guard guardFor(uint32_t wayOut) {
    Guard g{};
    uint8_t* c = g.code;
    c[0] = 0xB8;  // mov eax, GetGameMode
    std::memcpy(c + 1, &kGetGameMode, 4);
    c[5] = 0xFF, c[6] = 0xD0;   // call eax
    c[7] = 0x85, c[8] = 0xC0;   // test eax, eax
    c[9] = 0x75, c[10] = 0x07;  // jnz the ret
    c[11] = 0xC7, c[12] = 0x04, c[13] = 0x24;  // mov dword [esp], the way on
    std::memcpy(c + 14, &wayOut, 4);
    c[18] = 0xC3;  // ret
    return g;
}

void callTo(uint32_t at, uint8_t out[5]) { callFrom(kCall, at, out); }

void callFrom(uint32_t call, uint32_t to, uint8_t out[5]) {
    out[0] = 0xE8;
    const int32_t rel = int32_t(to - (call + 5));
    std::memcpy(out + 1, &rel, 4);
}

const Site kSites[kSiteCount] = {
    {0x547BC1, 0x547E0E, 0x547E0E, {0x5F, 0x5E, 0x5B, 0x8B, 0xE5, 0x5D, 0xC2, 0x08, 0x00}, 9,
     "a character's shadow drawn while a map loads"},  // 3.0.4-beta.9, R-VBDSCP
    {0x546DDB, 0x546E2E, 0x546DD9, {0x74, 0x53}, 2,
     "a character's part drawn while the map changes"},  // R-MD8VTF: FUN_00546c50, its je 0x546e2e the way on
    {0x4DFFFB, 0x4E005B, 0x4E005B, {0x5F, 0x5E, 0x5B, 0x8B, 0xE5, 0x5D, 0xC2, 0x14, 0x00}, 9,
     "the map loader thread's step while the map changes"},  // R-VWW92M: its function's end (ret 0x14)
};

std::array<uint8_t, 10> originalAt(uint32_t at) {
    std::array<uint8_t, 10> o{0xB9, 0x78, 0xB5, 0x74, 0x00, 0xE8};
    const int32_t rel = int32_t(kGetGameMode - (at + 10));
    std::memcpy(o.data() + 6, &rel, 4);
    return o;
}

}  // namespace uagfx::shadow_exe
