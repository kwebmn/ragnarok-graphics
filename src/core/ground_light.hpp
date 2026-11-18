#pragma once
#include <cstdint>
#include <string>

// 3.0.22-beta.10 (R-3SA4RB «Вийшов з пронти і такий баг», R-XHRKGN, R-MMMP29: the R7 M370 laptop's ground one flat
// light colour on Direct3D 11 - prt_fild08, Geffen, Aldebaran - though its own textures were there): the game draws the
// ground (two sets of texture coordinates) as its texture x colour in stage 0 and the light map in stage 1 by op 18,
// light.rgb + light.a x the rest - a light map none given or not there (drawn white) or a white one makes the ground
// white. The ground's draws by both stages' textures, for the log.
namespace uagfx::groundlight {

enum class Source : uint8_t { Game, Copy, None, Gone };  // the game's texture, its HD copy, none given, not there
struct Tally {
    uint64_t draws = 0;
    uint64_t stage0[4] = {};  // by Source
    uint64_t stage1[4] = {};  // by Source: the light map, where stage 1 is on
    uint64_t stage1Off = 0;   // stage 1 off (op 1): no light map sampled
    uint64_t lightOp = 0;     // stage 1 by op 18 (light.rgb + light.a x the rest)
    uint64_t otherOp = 0;     // stage 1 by another op - the last of them lastOp
    uint8_t lastOp = 0;
    uint64_t smooth = 0;  // the light map sampled smoothly ([effects] smooth_light)
};
void note(Tally& t, Source stage0, Source stage1, uint8_t op1, bool smooth);
// A draw drawn white for a texture none given or not there, in either stage
bool white(const Tally& t);
// «N draws: stage 0 - the game's textures .., their HD copies .., none given .., not there ..; stage 1 (the light map)
// - ...; op 18 (...) in .., another in .. (the last ..); the light map sampled smoothly in ..» - each count over `per`
// (the frames: a frame's; under 10 with a tenth)
std::string text(const Tally& t, double per);

}  // namespace uagfx::groundlight
