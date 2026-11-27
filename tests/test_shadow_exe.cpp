#include <array>
#include <cstdint>
#include <cstring>

#include "core/shadow_exe.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.4-beta.9 (R-VBDSCP: uaRO.exe+0x147bce on the way into moscovia): a character's shadow drawn while a map loads -
// the game mode asked for is none yet; the guard takes the shadow function's own way out
TEST(shadow_exe_the_guard_s_code) {
    const uint32_t at = 0x10000000;  // where the guard lands
    const shadow_exe::Guard g = shadow_exe::guard(at);
    // mov eax, GetGameMode; call eax
    CHECK(g.code[0] == 0xB8);
    uint32_t target = 0;
    std::memcpy(&target, g.code + 1, 4);
    CHECK_EQ(target, shadow_exe::kGetGameMode);
    CHECK(g.code[5] == 0xFF && g.code[6] == 0xD0);
    CHECK(g.code[7] == 0x85 && g.code[8] == 0xC0);  // test eax, eax
    CHECK(g.code[9] == 0x75 && size_t(11 + g.code[10]) == shadow_exe::kGuardSize - 1);  // jnz to the ret
    uint32_t out = 0;
    std::memcpy(&out, g.code + 14, 4);  // mov dword [esp], the function's way out
    CHECK(g.code[11] == 0xC7 && g.code[12] == 0x04 && g.code[13] == 0x24 && out == shadow_exe::kWayOut);
    CHECK(g.code[shadow_exe::kGuardSize - 1] == 0xC3);  // ret
    // the game's call there, now to the guard: E8 with the distance from its next instruction
    uint8_t call[5];
    shadow_exe::callTo(at, call);
    int32_t rel = 0;
    std::memcpy(&rel, call + 1, 4);
    CHECK(call[0] == 0xE8 && uint32_t(int32_t(shadow_exe::kCall + 5) + rel) == at);
    // the game's own bytes the change is made over: mov ecx, 0x74b578; call 0x4f6ad0
    CHECK(shadow_exe::kOriginal[0] == 0xB9 && shadow_exe::kOriginal[5] == 0xE8);
}

// 3.0.24-beta.31 (R-MD8VTF: uaRO.exe+0x146de5 on malangdo, beta.25; R-VWW92M: uaRO.exe+0xe0005 on malangdo, beta.9 -
// each a read of +0xc4 of no game mode while a map changes, the shadow's kind of fault): three places guarded the
// same way - the mode as before, none: the place's own way on (a skip the game takes itself, or its function's end)
TEST(shadow_exe_mode_guards_three_places) {
    CHECK_EQ(shadow_exe::kSiteCount, size_t(3));
    const shadow_exe::Site& shadow = shadow_exe::kSites[0];
    CHECK(shadow.at == 0x547BC1 && shadow.wayOut == 0x547E0E);  // the 3.0.4-beta.9 shadow
    const shadow_exe::Site& part = shadow_exe::kSites[1];
    CHECK(part.at == 0x546DDB && part.wayOut == 0x546E2E && part.checkAt == 0x546DD9);
    CHECK(part.checkSize == 2 && part.check[0] == 0x74 && part.check[1] == 0x53);  // je 0x546e2e - the game's own skip
    const shadow_exe::Site& loader = shadow_exe::kSites[2];
    CHECK(loader.at == 0x4DFFFB && loader.wayOut == 0x4E005B && loader.checkAt == 0x4E005B && loader.checkSize == 9);
    CHECK(loader.check[0] == 0x5F && loader.check[6] == 0xC2 && loader.check[7] == 0x14);  // ... ret 0x14
    for (size_t i = 0; i < shadow_exe::kSiteCount; ++i) {
        const shadow_exe::Site& s = shadow_exe::kSites[i];
        const std::array<uint8_t, 10> o = shadow_exe::originalAt(s.at);  // mov ecx, 0x74b578; call 0x4f6ad0
        CHECK(o[0] == 0xB9 && o[1] == 0x78 && o[2] == 0xB5 && o[3] == 0x74 && o[4] == 0x00 && o[5] == 0xE8);
        int32_t rel = 0;
        std::memcpy(&rel, o.data() + 6, 4);
        CHECK(uint32_t(int32_t(s.at + 10) + rel) == shadow_exe::kGetGameMode);
        const shadow_exe::Guard g = shadow_exe::guardFor(s.wayOut);
        uint32_t way = 0;
        std::memcpy(&way, g.code + 14, 4);
        CHECK(g.code[0] == 0xB8 && g.code[11] == 0xC7 && way == s.wayOut && g.code[18] == 0xC3);
        uint8_t call[5];
        shadow_exe::callFrom(s.at + 5, 0x10000000 + uint32_t(i) * 32, call);
        std::memcpy(&rel, call + 1, 4);
        CHECK(call[0] == 0xE8 && uint32_t(int32_t(s.at + 10) + rel) == 0x10000000 + uint32_t(i) * 32);
    }
    // the bytes of the game's build the guards were read from: the part's call E8 EB FC FA FF, the loader's E8 CB 6A 01
    // 00
    const std::array<uint8_t, 10> p = shadow_exe::originalAt(0x546DDB), l = shadow_exe::originalAt(0x4DFFFB);
    CHECK(p[6] == 0xEB && p[7] == 0xFC && p[8] == 0xFA && p[9] == 0xFF);
    CHECK(l[6] == 0xCB && l[7] == 0x6A && l[8] == 0x01 && l[9] == 0x00);
}
