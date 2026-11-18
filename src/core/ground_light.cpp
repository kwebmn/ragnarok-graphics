#include "core/ground_light.hpp"

#include <cmath>
#include <cstdio>

#include "core/d3d7.hpp"

namespace uagfx::groundlight {
namespace {

// A count over `per`: whole as it is, under 10 with a tenth
std::string num(uint64_t count, double per) {
    const double v = per > 0.0 ? double(count) / per : double(count);
    char out[32];
    if (v == 0.0 || v >= 10.0 || v == std::floor(v))
        std::snprintf(out, sizeof out, "%.0f", v);
    else
        std::snprintf(out, sizeof out, "%.1f", v);
    return out;
}

}  // namespace

void note(Tally& t, Source stage0, Source stage1, uint8_t op1, bool smooth) {
    ++t.draws;
    ++t.stage0[int(stage0)];
    if (op1 == d3d7::TOP_DISABLE) {  // no light map sampled: none given is no white
        ++t.stage1Off;
        return;
    }
    ++t.stage1[int(stage1)];
    if (op1 == d3d7::TOP_MODULATEALPHA_ADDCOLOR) {
        ++t.lightOp;
    } else {
        ++t.otherOp;
        t.lastOp = op1;
    }
    if (smooth) ++t.smooth;
}

bool white(const Tally& t) {
    return t.stage0[int(Source::None)] || t.stage0[int(Source::Gone)] || t.stage1[int(Source::None)] ||
           t.stage1[int(Source::Gone)];
}

std::string text(const Tally& t, double per) {
    const auto at = [&](const uint64_t* stage, Source s) { return num(stage[int(s)], per); };
    return num(t.draws, per) + " draws: stage 0 - the game's textures " + at(t.stage0, Source::Game) +
           ", their HD copies " + at(t.stage0, Source::Copy) + ", none given " + at(t.stage0, Source::None) +
           ", not there " + at(t.stage0, Source::Gone) + "; stage 1 (the light map) - the game's " +
           at(t.stage1, Source::Game) + ", its HD copies " + at(t.stage1, Source::Copy) + ", none given " +
           at(t.stage1, Source::None) + ", not there " + at(t.stage1, Source::Gone) + ", off " +
           num(t.stage1Off, per) + "; op 18 (light.rgb + light.a x the rest) in " + num(t.lightOp, per) +
           ", another in " + num(t.otherOp, per) + " (the last " + std::to_string(unsigned(t.lastOp)) +
           "); the light map sampled smoothly in " + num(t.smooth, per);
}

}  // namespace uagfx::groundlight
