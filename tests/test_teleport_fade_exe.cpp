// 3.0.13-beta.7 (the owner's pick 18 of «50 идей»): the same map's teleport without the game's darkening - the bytes
// at the site are `push 0; mov ecx, esi; call FUN_004f5790`, the call's target worked out of its own offset
#include <cstdint>
#include <cstring>

#include "core/teleport_fade_exe.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(teleport_fade_site_calls_the_darkening) {
    namespace tf = teleport_fade_exe;
    CHECK(tf::kGame[0] == 0x6A && tf::kGame[1] == 0x00 && tf::kGame[4] == 0xE8);
    int32_t rel = 0;
    std::memcpy(&rel, tf::kGame + 5, 4);
    const uint32_t next = tf::kSite + 9;  // the instruction after the call
    CHECK(next + uint32_t(rel) == 0x4F5790u);
    CHECK(next == 0x57EC1Eu);  // where ECX is loaded again
    for (uint8_t b : tf::kQuick) CHECK(b == 0x90);
}
