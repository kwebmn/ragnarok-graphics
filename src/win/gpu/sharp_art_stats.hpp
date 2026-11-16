#pragma once
#include <cstdint>

namespace uagfx::gpu {

// 3.0.20-beta.68 (core/sharp_art, the owner's item 7 - the buffs «как в игре, только чётко»): a skill's draw came with
// the driver's copy of its painted picture (DrawCmd::sharpArt) - what the renderer drew it from: the copy, the pack's
// AI picture of the game's texture (it goes first), or the game's own (the copy not on the card yet). Told once each
// way, then a line a minute while such draws come.
struct SharpArtStats {
    uint32_t copies = 0, pack = 0, own = 0;
    bool toldCopy = false, toldPack = false;
    unsigned long long toldAt = 0;
    void note(bool fromCopy, bool fromPack, const char* card);
};

}  // namespace uagfx::gpu
