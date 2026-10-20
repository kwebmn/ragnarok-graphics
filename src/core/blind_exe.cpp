#include "core/blind_exe.hpp"

#include <cstring>

namespace uagfx::blind_exe {
namespace {

void faddsAt(uint32_t address, uint8_t out[6]) {
    out[0] = 0xD8;  // fadds dword [address]
    out[1] = 0x05;
    std::memcpy(out + 2, &address, 4);
}

}  // namespace

void original(uint8_t out[6]) { faddsAt(kGameReachAt, out); }
void widened(uint8_t out[6]) { faddsAt(kWideReachAt, out); }

}  // namespace uagfx::blind_exe
