#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace uagfx::windowguard {

// 3.0.11-beta.194 (R-ZS5B2D: uaRO.exe crashed on Enter 17 s into geffen_in - its Enter, 0x4CE990, pressed the menu's
// OK through the windows' manager's +0xD8, a window already freed): uaRO.exe's windows' manager (0x727ED8) names each
// kind of its windows in a field of its own (MakeWindow 0x4D42D0 sets it, DeleteWindow 0x4D1E60 clears it) and keeps
// them all in its list (+0x38: the head node of a ring of {next, previous, window}). A field naming a window the list
// has not: the window freed - the game would call through it.
struct Field {
    uint32_t offset;  // in the manager
    int window;       // its UI window's number (MakeWindow's); 0: the message box (0x4CEDF0's own)
    const char* name;
};
// Enter's (0x4CE990) in its order, the focused control aside (+0x6C: a part of a window, not in the list)
extern const Field kEnterFields[10];

// Reads a 32-bit value of the game's memory; false: not readable
using Read = std::function<bool(uint32_t at, uint32_t& value)>;
// The windows the ring names from its head node on, at most `most`; whole: the ring came back to its head (a node not
// readable or a ring longer: not whole - nothing to judge by)
std::vector<uint32_t> listedWindows(const Read& read, uint32_t head, size_t most, bool& whole);
// Of the named windows (0: none), the places of the ones the list has not
std::vector<size_t> gone(const std::vector<uint32_t>& named, const std::vector<uint32_t>& listed);

}  // namespace uagfx::windowguard
