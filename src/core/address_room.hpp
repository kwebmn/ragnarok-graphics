#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::addressroom {

// 3.0.22-beta.42 (the owner's R-C8SA9Z: std::bad_alloc after two graphics tests): the game's address space as
// VirtualQuery walks it - summed for the log every 30 s, so that a leak shows as the free room going down.
// 3.0.22-beta.43 (the owner: «делать» - who holds it): by its use - the modules and the files mapped in (by name),
// the private (heaps, VirtualAlloc)
enum class Kind { Free, Reserved, Committed };
enum class Use { Private, Mapped, Image };
struct Region {
    uint64_t base = 0, size = 0;
    Kind kind = Kind::Free;
    Use use = Use::Private;
    std::string name;  // a module's or a mapped file's (its file name)
};
struct Owner {
    std::string name;
    uint32_t mb = 0;
};
struct Room {
    uint32_t freeMb = 0, largestFreeMb = 0, committedMb = 0, reservedMb = 0;
    uint32_t privateMb = 0, mappedMb = 0, imageMb = 0;  // committed and reserved together
    std::vector<Owner> owners;  // the named ones (files mapped, modules), the largest first
};
Room sum(const std::vector<Region>& regions);
// «data.grf 400, uaro-textures.zip 50» - the `most` largest
std::string ownersLine(const Room& room, size_t most);

}  // namespace uagfx::addressroom
