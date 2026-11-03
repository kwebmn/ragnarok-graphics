// 3.0.11-beta.197 (the tester: «візуальний баг ассумптіо + енжел вінгс на топ голові» - a white square round the
// head): an HD copy keeps its edge's colour under its see-through texels; a draw adding it with the source's factor one
// (its alpha unused) takes its colour times its alpha - as the game's own picture, black there, would add
#include "core/ai_copy_blend.hpp"
#include "core/d3d7.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(ai_copy_blend_colour_times_alpha_when_added_by_one) {
    CHECK(aicopy::colourTimesAlpha(true, true, d3d7::BLEND_ONE));
    CHECK(!aicopy::colourTimesAlpha(false, true, d3d7::BLEND_ONE));         // the game's own picture: black there
    CHECK(!aicopy::colourTimesAlpha(true, false, d3d7::BLEND_ONE));         // not blended: no see-through to show
    CHECK(!aicopy::colourTimesAlpha(true, true, d3d7::BLEND_SRCALPHA));     // its alpha already takes it
    CHECK(!aicopy::colourTimesAlpha(true, true, d3d7::BLEND_ZERO));
}
