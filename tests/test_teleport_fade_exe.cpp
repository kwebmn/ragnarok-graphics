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

// 3.0.13-beta.8 (the owner's pick 2 of «50 идей»): the shake's start returns at once - `ret 0x14` for its 5 arguments,
// as the function's own three returns do
#include "core/screen_shake_exe.hpp"
TEST(screen_shake_start_returns_at_once) {
    namespace ss = uagfx::screen_shake_exe;
    CHECK(ss::kShakeStart == 0x4FA830u);
    CHECK(ss::kGame[0] == 0x55 && ss::kNone[0] == 0xC2 && ss::kNone[1] == 0x14 && ss::kNone[2] == 0x00);
}

// 3.0.13-beta.8 (the owner's pick 23 of «50 идей»): the jokes' `jne` over the not-opened path - two bytes, short
#include "core/skill_jokes_exe.hpp"
TEST(skill_jokes_jump_over_the_unopened_path) {
    namespace sj = uagfx::skill_jokes_exe;
    CHECK(sj::kOpened == 0x577748u && sj::kGame[0] == 0x75);
    CHECK(sj::kOpened + 2 + sj::kGame[1] == 0x577771u);  // where the opened file is read
    CHECK(sj::kSilent[0] == 0x90 && sj::kSilent[1] == 0x90);
}

// 3.0.13-beta.9 (the owner's pick 25 of «50 идей»): the swear filter's two checks answer «none» at once - xor eax,
// eax; ret 4 for their one argument, in the five bytes of their prologue
#include "core/swear_filter_exe.hpp"
TEST(swear_filter_checks_answer_none) {
    namespace sf = uagfx::swear_filter_exe;
    CHECK(sf::kChecks[0] == 0x4FD9A0u && sf::kChecks[1] == 0x4FDAD0u);
    CHECK(sf::kNever[0] == 0x33 && sf::kNever[1] == 0xC0 && sf::kNever[2] == 0xC2 && sf::kNever[3] == 0x04);
    CHECK(sizeof sf::kGame == sizeof sf::kNever);
}
