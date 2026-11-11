#include "core/equip_exe.hpp"

namespace uagfx::equip_exe {

bool wearable(uint32_t mask) {
    if (mask == kAmmo) return true;
    int places = 0;
    for (uint32_t m = mask & 0x3FF; m; m &= m - 1) ++places;
    return places >= 1 && places <= 3;
}

}  // namespace uagfx::equip_exe
