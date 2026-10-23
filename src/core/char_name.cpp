#include "core/char_name.hpp"

namespace uagfx::charname {

const uint8_t kCode[kCodeSize] = {0x53, 0x56, 0x57, 0x8D, 0xB1, 0x18, 0x06, 0x00, 0x00, 0xB9, 0x10, 0x00, 0x00, 0x00,
                                  0xBF, 0xE8, 0x2F, 0x77, 0x00, 0xF3, 0xA5, 0x8B, 0x0D, 0x78, 0x2F, 0x6E, 0x00};

std::string decode(const uint8_t* stored, const uint8_t* key) {
    bool set = false;
    for (size_t i = 0; i < kStored; ++i) set = set || stored[i] != 0;
    if (!set) return "";
    std::string name;
    for (size_t i = 0; i < kMost; ++i) {
        const auto c = uint8_t(stored[i] ^ key[i]);
        if (c == 0) return name;
        if (c < 0x20 || c == 0x7F) return "";
        name += char(c);
    }
    return "";
}

}  // namespace uagfx::charname
