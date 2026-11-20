#include "core/address_room.hpp"

#include <algorithm>
#include <map>

namespace uagfx::addressroom {

Room sum(const std::vector<Region>& regions) {
    uint64_t free = 0, largest = 0, committed = 0, reserved = 0, priv = 0, mapped = 0, image = 0;
    std::map<std::string, uint64_t> named;
    for (const Region& r : regions) {
        if (r.kind == Kind::Free) {
            free += r.size;
            largest = std::max(largest, r.size);
            continue;
        }
        (r.kind == Kind::Committed ? committed : reserved) += r.size;
        (r.use == Use::Mapped ? mapped : r.use == Use::Image ? image : priv) += r.size;
        if (r.use != Use::Private && !r.name.empty()) named[r.name] += r.size;
    }
    const auto mb = [](uint64_t b) { return uint32_t(b >> 20); };
    Room room{mb(free), mb(largest), mb(committed), mb(reserved), mb(priv), mb(mapped), mb(image), {}};
    for (const auto& [name, bytes] : named) room.owners.push_back(Owner{name, mb(bytes)});
    std::stable_sort(room.owners.begin(), room.owners.end(),
                     [](const Owner& a, const Owner& b) { return a.mb > b.mb; });
    return room;
}

std::string ownersLine(const Room& room, size_t most) {
    std::string out;
    for (size_t i = 0; i < room.owners.size() && i < most; ++i)
        out += (out.empty() ? "" : ", ") + room.owners[i].name + " " + std::to_string(room.owners[i].mb);
    return out;
}

}  // namespace uagfx::addressroom
