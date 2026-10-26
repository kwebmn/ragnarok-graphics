#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace uagfx::pe {

// 3.0.2-beta.12 (a map's loading measured): a loaded module's named functions, so that the profile says which of
// Windows' functions the game's thread was in ("Sleep", "ReadFile") - read from the module's image in memory, 32-bit
// (the game's modules).
using Read = std::function<bool(uint32_t rva, void* out, size_t size)>;  // false past the image or not mapped

struct Export {
    uint32_t rva = 0;
    std::string name;
};

// Its named functions in the order of their places (forwarders to other modules left out); empty when it has none
// or its headers are not a 32-bit PE's.
std::vector<Export> exportsOf(const Read& read);
// The function a place is in: the nearest at or before it; nullptr before the first.
const Export* exportAt(const std::vector<Export>& sorted, uint32_t rva);
// Its code: from its first executable section's start to its last one's end; false when it is not a 32-bit PE.
bool codeOf(const Read& read, uint32_t& begin, uint32_t& end);

// 3.0.6-beta.3 (the game's thread on a FirePro laptop: the driver's share of it spread over its 64-byte places): a
// module's functions by its unwind table (.eh_frame, mingw's: the section's name cut to ".eh_fram") - each one's place
// and size, in the order of their places; empty without such a table. The profile tallies our module's samples by them.
struct Function {
    uint32_t rva = 0, size = 0;
};
std::vector<Function> functionsOf(const Read& read);
const Function* functionAt(const std::vector<Function>& sorted, uint32_t rva);  // nullptr: in none

}  // namespace uagfx::pe
