#include "core/address_room.hpp"

#include <algorithm>

namespace uagfx::addressroom {

Room sum(const std::vector<Region>& regions) {
    uint64_t free = 0, largest = 0, committed = 0, reserved = 0;
    for (const Region& r : regions) {
        if (r.kind == Kind::Free) {
            free += r.size;
            largest = std::max(largest, r.size);
        } else if (r.kind == Kind::Committed) {
            committed += r.size;
        } else {
            reserved += r.size;
        }
    }
    const auto mb = [](uint64_t b) { return uint32_t(b >> 20); };
    return Room{mb(free), mb(largest), mb(committed), mb(reserved)};
}

}  // namespace uagfx::addressroom
