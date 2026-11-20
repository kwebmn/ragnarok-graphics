#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::addressroom {

// 3.0.22-beta.42 (the owner's R-C8SA9Z: std::bad_alloc after two graphics tests): the game's address space as
// VirtualQuery walks it - summed for the log every 30 s, so that a leak shows as the free room going down
enum class Kind { Free, Reserved, Committed };
struct Region {
    uint64_t base = 0, size = 0;
    Kind kind = Kind::Free;
};
struct Room {
    uint32_t freeMb = 0, largestFreeMb = 0, committedMb = 0, reservedMb = 0;
};
Room sum(const std::vector<Region>& regions);

}  // namespace uagfx::addressroom
