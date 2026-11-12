// 3.0.18-beta.223 (R-H42AS4, R-3PETVR «процес є, а гра так і не запускається»: no log at all - the game stopped before
// its DirectDraw, where the driver's log opens): the start watch's words - an address as its module and offset, the
// main thread's place with the calls under it (the stack's words inside a module that follow a call), innermost first
#include <string>
#include <vector>

#include "core/start_watch.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(start_watch_places_by_module) {
    const std::vector<startwatch::Module> mods = {{"uaRO.exe", 0x400000, 0x300000}, {"mss32.dll", 0x10000000, 0x20000}};
    CHECK_EQ(startwatch::placeOf(mods, 0x10001234), std::string("mss32.dll+0x1234"));
    CHECK_EQ(startwatch::placeOf(mods, 0x00401000), std::string("uaRO.exe+0x1000"));
    CHECK_EQ(startwatch::placeOf(mods, 0x00300000), std::string("0x00300000"));  // in none
    const uint32_t stack[] = {5, 0x401000, 0x10002000, 0x450000, 0x7FFE0000, 0x402222};
    const auto calls = [](uint32_t a) { return a != 0x10002000; };  // that word follows no call
    CHECK_EQ(startwatch::where(mods, 0x10001234, stack, 6, calls),
             std::string("mss32.dll+0x1234 <- uaRO.exe+0x1000 <- uaRO.exe+0x50000 <- uaRO.exe+0x2222"));
    CHECK_EQ(startwatch::where(mods, 0x10001234, stack, 6, calls, 2),
             std::string("mss32.dll+0x1234 <- uaRO.exe+0x1000 <- uaRO.exe+0x50000"));  // the first two callers
}
