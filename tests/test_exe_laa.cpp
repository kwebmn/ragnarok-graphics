// 3.0.22-beta.44 (the owner 21.11: «да» - after R-6W84UK: 2767 MB free with the flag, 266 MB without): uaRO.exe made
// large address aware - 4 GB for the game on a 64-bit Windows; its PE header's one bit, read and set or cleared
#include <string>

#include "core/exe_laa.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
std::string fakeExe(uint16_t characteristics) {
    std::string b(0x200, '\0');
    b[0] = 'M', b[1] = 'Z';
    b[0x3C] = char(0x80);  // e_lfanew
    b[0x80] = 'P', b[0x81] = 'E';
    b[0x80 + 4 + 18] = char(characteristics & 0xFF);
    b[0x80 + 4 + 19] = char(characteristics >> 8);
    return b;
}
}  // namespace

TEST(exe_laa_read_and_set) {
    std::string exe = fakeExe(0x010F);  // uaRO.exe's own
    CHECK(exelaa::stateOf(exe) == exelaa::State::Clear);
    CHECK(exelaa::set(exe, true));
    CHECK(exelaa::stateOf(exe) == exelaa::State::Set);
    CHECK_EQ(uint8_t(exe[0x80 + 4 + 18]), 0x2F);  // 0x012F
    CHECK(exelaa::set(exe, false));
    CHECK(exelaa::stateOf(exe) == exelaa::State::Clear);
    CHECK_EQ(uint8_t(exe[0x80 + 4 + 18]), 0x0F);
    std::string junk = "not an exe";
    CHECK(exelaa::stateOf(junk) == exelaa::State::NotExe);
    CHECK(!exelaa::set(junk, true));
    std::string bad = fakeExe(0x010F);
    bad[0x3C] = char(0xF0), bad[0x3D] = char(0xFF);  // e_lfanew past the file
    CHECK(exelaa::stateOf(bad) == exelaa::State::NotExe);
}
