#include "core/cave_look.hpp"

namespace uagfx::cave_look {

Look forMap(const std::string& map, const Parts& wanted) {
    Look l;
    if (map == "beach_dun" || map == "beach_dun2" || map == "beach_dun3") {  // 3.0.24-beta.16: its caves
        l.parts = (wanted.crystals ? kCrystals : 0u) | (wanted.grotto ? kGrotto : 0u);
        return l;
    }
    if (map != "comodo") return l;
    l.parts = (wanted.rift ? kRift : 0u) | (wanted.crystals ? kCrystals : 0u) | (wanted.grotto ? kGrotto : 0u);
    if (l.parts) l.parts |= kTown;
    // over the bay's middle, short of the walls to the west and the east (the sea's cells 0-179 x 0-50, its middle
    // 86 x 42 with the coves; the open water south of 32)
    l.riftX = 90;
    l.riftZ = 24;
    l.radiusX = 44;
    l.radiusZ = 20;
    return l;
}

void pack(const Look& look, float& place, float& size) {
    if (!look.on()) {
        place = size = 0.0f;
        return;
    }
    place = float(look.riftX + look.riftZ * 1024);
    size = float(look.radiusX + look.radiusZ * 256 + int(look.parts) * 65536);
}

}  // namespace uagfx::cave_look
