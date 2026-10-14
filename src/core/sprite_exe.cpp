#include "core/sprite_exe.hpp"

namespace uagfx::sprite_exe {

bool isWriteCall(const uint8_t bytes[5]) {
    if (bytes[0] != 0xE8) return false;
    const uint32_t rel = uint32_t(bytes[1]) | uint32_t(bytes[2]) << 8 | uint32_t(bytes[3]) << 16 |
                         uint32_t(bytes[4]) << 24;
    return kWriteCall + 5 + rel == kWrite;
}

}  // namespace uagfx::sprite_exe
