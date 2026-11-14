// 3.0.11-beta.197 (the tester: «візуальний баг ассумптіо + енжел вінгс на топ голові» - a white square round the
// head): an HD copy keeps its edge's colour under its see-through texels; a draw adding it with the source's factor one
// (its alpha unused) takes its colour times its alpha - as the game's own picture, black there, would add
#include "core/ai_copy_blend.hpp"
#include "core/d3d7.hpp"
#include "core/sprite_exe.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(ai_copy_blend_colour_times_alpha_when_added_by_one) {
    CHECK(aicopy::colourTimesAlpha(true, true, d3d7::BLEND_ONE));
    CHECK(!aicopy::colourTimesAlpha(false, true, d3d7::BLEND_ONE));         // the game's own picture: black there
    CHECK(!aicopy::colourTimesAlpha(true, false, d3d7::BLEND_ONE));         // not blended: no see-through to show
    CHECK(!aicopy::colourTimesAlpha(true, true, d3d7::BLEND_SRCALPHA));     // its alpha already takes it
    CHECK(!aicopy::colourTimesAlpha(true, true, d3d7::BLEND_ZERO));
}

// 3.0.19-beta.5 (R-P75FMG, the beta tester: «знову асумптіо квадратом навколо персонажа»): under Assumptio the game
// draws the character's body and head a second time, a little larger and pulsing, added as light (uaRO.exe 0x545b40:
// actor flag 8 at +0xD4 set by effect 375, the quad through 0x547e20 with mode 5 = source alpha, destination one) -
// that pass takes the game's own picture of the frame, not its HD copy; the frame drawn as usual keeps the copy
TEST(ai_copy_actor_frame_added_as_light_takes_the_game_picture) {
    using S = sprite_exe::Source;
    CHECK(aicopy::gameOwnWhenAdded(uint8_t(S::Player), false, true, d3d7::BLEND_ONE));
    CHECK(aicopy::gameOwnWhenAdded(uint8_t(S::Monster), false, true, d3d7::BLEND_ONE));  // an MVP under Assumptio
    CHECK(aicopy::gameOwnWhenAdded(uint8_t(S::Npc), false, true, d3d7::BLEND_ONE));
    CHECK(!aicopy::gameOwnWhenAdded(uint8_t(S::Player), false, true, d3d7::BLEND_INVSRCALPHA));  // as usual: its copy
    CHECK(!aicopy::gameOwnWhenAdded(uint8_t(S::Effect), false, true, d3d7::BLEND_ONE));  // a skill's sprite: its own
    CHECK(!aicopy::gameOwnWhenAdded(uint8_t(S::Item), false, true, d3d7::BLEND_ONE));
    CHECK(!aicopy::gameOwnWhenAdded(uint8_t(S::Unknown), false, true, d3d7::BLEND_ONE));
    CHECK(!aicopy::gameOwnWhenAdded(uint8_t(S::Player), true, true, d3d7::BLEND_ONE));  // the interface
    CHECK(!aicopy::gameOwnWhenAdded(uint8_t(S::Player), false, false, d3d7::BLEND_ONE));  // not blended
}
