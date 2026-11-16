#pragma once
#include <cstdint>
#include <vector>

#include "core/sharp_frames.hpp"

namespace uagfx::sharpart {

// 3.0.20-beta.68 (the owner's item 7, «умения как в игре, только чётко» - the buffs seen in town all day: Increase
// AGI's «AGI UP!», Gloria's nun, Magnificat's angel, Kyrie's bells, Impositio's angel, Assumptio's priest and wings;
// R-HBPQ54's and R-RDA885's records: their pictures 64 to 128 texels, a texel shown 1 to 5 of the game's pixels wide,
// twice that on a 4K screen - smoothed into a blur): a skill's painted picture - pixel art the game keys out of
// magenta (its alpha 0 or whole, flat colours with drawn outlines) - is drawn from a copy four times larger by xBR
// (core/xbr, as the damage digits of core/sharp_frames): the same picture, sharp. The game still places, colours,
// turns and blends every quad.
// The texels judged are the game's own: 16-bit (A1R5G5B5 - 32 levels a channel), and a .str animation's pictures have
// their black keyed out as well (uaRO.exe FUN_0042c850; core/pack_source.cpp blackKey) - a glow, a ring, a ball of
// lightning come with the same whole-or-nothing alpha as a painted bell. What tells them apart, measured on the HD
// client's 1503 effect pictures converted as the game converts them (/root/uagfx-tools/audit/buffs): light on keyed
// black fades to the dark at its edge (59-100 % of its texels beside the key are dark; a painted picture's 0 %) and
// goes in steps of one level (12-70 % of its unlike neighbours; a painted picture's 0 %). Taken: 235 pictures - the
// letters, bells, angels, wings, crosses, coins, signs. Not theirs: light on black, pictures with an edge of their own
// (.tga), those shaded softly (the ice blocks, the smooth hearts: xBR would band them), sheets of specks
// (core/sharp_specks). Where the player's texture pack holds an AI picture of it the renderer draws that one
// (DrawCmd::sharpArt).

constexpr int kLeastSide = 8;    // smaller: no picture
constexpr int kMostSide = 128;   // larger ones are shown near their own size (the 256-texel signs)
constexpr int kLeastPainted = 16;  // texels
constexpr int kClearFrom = 10, kClearTo = 97;  // percent of its texels keyed out
constexpr int kFlatFrom = 25;   // percent of its painted texels beside one of their own colour (4 neighbours)
constexpr int kSmoothMost = 25;  // percent of its unlike neighbours a level apart: more is soft shading
constexpr int kRimDarkMost = 20;  // percent of its texels beside the key that are dark: more is light fading out ...
constexpr int kBigFrom = 50;     // ... unless every step in it is a hard one (an outline drawn in black): percent
constexpr int kScale = 4;
constexpr int kLevel32 = 9, kLevel16 = 17;  // a step of one of the game's levels, of 255 (5 bits a channel; 4)
constexpr int kBigStep = 33;                // past four of the 32 levels: a hard step
constexpr int kRimDarkUnder = 40;           // a painted texel beside the key this dim (of 255) is dark

struct Facts {
    bool binary = false;  // every texel's alpha 0 or whole
    int clear = 0, flat = 0;  // percents, as the constants
    // of its painted neighbours of unlike colour (percents): a step of one of the game's levels, a hard step
    int smooth = 0, big = 0;
    int rimDark = 0;      // percent of its painted texels beside the key that are dark
    // the counts behind them (the rule goes by these: 20.9 % is past 20)
    int texels = 0, painted = 0, flatTexels = 0;
    int pairs = 0, smoothPairs = 0, bigPairs = 0;
    int rim = 0, rimDarkTexels = 0;
};
Facts factsOf(const uint32_t* argb, int w, int h);
bool painted(const uint32_t* argb, int w, int h);

// Its copy: kScale times its size by xBR, then its halves down to its own size - the card takes the level that fits
// how large the game shows it. Empty levels: not a painted picture.
sharpframes::Copy makeCopy(const uint32_t* argb, int w, int h);

// The same copy made a part at a time: whole it is 6 ms of the game's thread on average and 23 for a 128 x 128
// picture (the game's 233 timed, /root/uagfx-tools/audit/buffs/timeit.cpp) - a frame and a half lost as a buff's
// picture first shows. A few rows a step (xBR reads two texels round each: a strip with three rows more on either
// side comes out the same), then a step for each of its halves.
class CopyJob {
public:
    CopyJob() = default;
    // Its texels kept. Not a painted picture: done at once, no copy.
    CopyJob(const uint32_t* argb, int w, int h);
    bool done() const { return stage_ == Stage::Done; }
    // About `texels` of the picture more (whole rows, one at least), or one of its halves; true once it is done
    bool step(int texels);
    // The copy (no levels when there is none); the job empty after
    sharpframes::Copy take();

private:
    enum class Stage : uint8_t { Rows, Halves, Done };
    Stage stage_ = Stage::Done;
    std::vector<uint32_t> source_;
    int w_ = 0, h_ = 0, row_ = 0;
    int levelW_ = 0, levelH_ = 0;  // of the copy's last level made
    sharpframes::Copy copy_;
};

}  // namespace uagfx::sharpart
