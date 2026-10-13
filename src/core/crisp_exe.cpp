#include "core/crisp_exe.hpp"

#include <cstdio>

namespace uagfx::crisp_exe {

std::string checkBitmapMethods(const uint32_t entries[kBitmapSlots]) {
    for (int i = 0; i < kBitmapSlots; ++i)
        if (entries[i] != kBitmapMethods[i]) {
            char s[96];
            std::snprintf(s, sizeof s, "window picture method %d is 0x%08x, expected 0x%08x", i,
                          static_cast<unsigned>(entries[i]), static_cast<unsigned>(kBitmapMethods[i]));
            return s;
        }
    return "";
}

}  // namespace uagfx::crisp_exe
