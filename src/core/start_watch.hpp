#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace uagfx::startwatch {

// 3.0.18-beta.223 (R-H42AS4, R-3PETVR «процес є, а гра так і не запускається»: no log at all - the game stopped before
// its DirectDraw, where the driver's log opens): the start watch's words (win/start_watch reads the thread and the
// modules)
struct Module {
    std::string name;
    uint32_t base = 0, size = 0;
};
// An address as «module+0xoffset», «0x........» outside every module
std::string placeOf(const std::vector<Module>& mods, uint32_t address);
// The thread's place and the calls under it: its instruction, then the stack's words inside a module that follow a call
// (`afterCall`), innermost first - at most `most` of them - joined by « <- »
std::string where(const std::vector<Module>& mods, uint32_t at, const uint32_t* stack, size_t words,
                  const std::function<bool(uint32_t)>& afterCall, size_t most = 12);

}  // namespace uagfx::startwatch
