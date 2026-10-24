#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace uagfx {

// 0.50.1: which graphics card to draw on when the computer has several - a laptop's integrated Intel and its NVIDIA
// or AMD, a desktop with two cards. Windows' first adapter is the one the main monitor hangs on (often the integrated
// one); a game takes the fast one, as Windows orders them for high performance (DXGI 1.6, where the player's choice
// in Windows' graphics settings comes first), else by its video memory.
struct AdapterFacts {
    std::string name;
    unsigned vendor = 0;
    unsigned long long vramMb = 0;
    bool software = false;  // WARP, the Basic Render Driver
};

// The indexes of the adapters to try, best first; software ones left out. byPreference: the list is already in
// Windows' order for high performance - kept as it is. chosen (0.50.2, [renderer] gpu): the card of that name first.
std::vector<size_t> adapterOrder(const std::vector<AdapterFacts>& adapters, bool byPreference,
                                 const std::string& chosen = "");
// "NVIDIA GeForce RTX 4070 (12282 MB), Intel(R) UHD Graphics 770 (128 MB)": for the log.
std::string adaptersText(const std::vector<AdapterFacts>& adapters);

// 3.0.4-beta.2 (the user): the settings app's graphics card row for `cards` real cards found, one `chosen` by name or
// not - shown with any card (which one draws the game), a list to choose from with several or a chosen one.
struct GpuRow {
    bool shown = false;
    bool choosable = false;
};
GpuRow gpuRow(size_t cards, bool chosen);

}  // namespace uagfx
