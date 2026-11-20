#include "core/ui_hover.hpp"

#include <algorithm>

namespace uagfx::uihover {

bool passiveOver(const std::vector<DocAt>& docs, int x, int y) {
    int best = -1;
    bool passive = false;
    for (const DocAt& d : docs) {
        if (x < d.x || y < d.y || x >= d.x + d.w || y >= d.y + d.h) continue;
        // a passive one that is not a window - the HUD edit's frame over the screen, the strips - is seen through
        if (d.passive && d.layer != Layer::Windows) continue;
        const int rank = d.layer == Layer::Top ? 2000 : d.layer == Layer::Windows ? 1000 + std::max(0, d.z) : 0;
        if (rank > best || (rank == best && !d.passive)) {  // a live one among equals keeps the pointer
            best = rank;
            passive = d.passive;
        }
    }
    return best >= 1000 && passive;
}

}  // namespace uagfx::uihover
