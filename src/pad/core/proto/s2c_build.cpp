#include "pad/core/proto/s2c_build.hpp"

namespace uapad {

Bytes buildEmotion(uint32_t actor, uint8_t type) {
    Bytes b(7, 0);
    wr16(b, 0, 0x00c0);
    wr32(b, 2, actor);
    wr8(b, 6, type);
    return b;
}

Bytes buildUnitState(uint32_t actor, uint16_t opt1, uint16_t opt2, uint32_t option) {
    Bytes b(15, 0);
    wr16(b, 0, 0x0229);
    wr32(b, 2, actor);
    wr16(b, 6, opt1);
    wr16(b, 8, opt2);
    wr32(b, 10, option);
    return b;  // byte 14: karma, 0
}

}  // namespace uapad
