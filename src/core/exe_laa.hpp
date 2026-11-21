#pragma once
#include <cstdint>
#include <string>

namespace uagfx::exelaa {

// 3.0.22-beta.44 (the owner 21.11: «да», after R-6W84UK - 2767 MB of the game's address space free with the flag,
// 266 MB without in Prontera, std::bad_alloc on R-C8SA9Z): uaRO.exe large address aware - its PE header's
// IMAGE_FILE_LARGE_ADDRESS_AWARE bit (0x20 in the file header's Characteristics): 4 GB for the game on a 64-bit Windows
enum class State { NotExe, Clear, Set };
State stateOf(const std::string& exe);
// The bit set or cleared in place; false - not an exe
bool set(std::string& exe, bool on);

}  // namespace uagfx::exelaa
