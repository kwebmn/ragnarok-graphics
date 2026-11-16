#include "win/gpu/sharp_art_stats.hpp"

#include <windows.h>

#include "win/log.hpp"

namespace uagfx::gpu {

void SharpArtStats::note(bool fromCopy, bool fromPack, const char* card) {
    if (fromCopy) ++copies;
    else if (fromPack) ++pack;
    else ++own;
    if (fromCopy && !toldCopy) {
        toldCopy = true;
        LOG_INFO("skills' painted pictures: the first drawn from the driver's copy four times larger (%s)", card);
    }
    if (fromPack && !toldPack) {
        toldPack = true;
        LOG_INFO("skills' painted pictures: one left to the pack's AI picture of it - the copy not drawn (%s)", card);
    }
    const unsigned long long now = GetTickCount64();
    if (!toldAt) toldAt = now;
    if (now - toldAt < 60000) return;
    toldAt = now;
    LOG_INFO("skills' painted pictures: in a minute %u draws from the driver's copies, %u from the pack's AI pictures, "
             "%u from the game's own (%s)", copies, pack, own, card);
    copies = pack = own = 0;
}

}  // namespace uagfx::gpu
