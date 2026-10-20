#include <cstdint>
#include <cstring>

#include "core/blind_exe.hpp"
#include "core/game_camera.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(blind_darkness_sites_are_its_frames_four_strips) {
    // the four strips of the frame round the clear middle each add the game's 50.0 to the middle's half side
    CHECK_EQ(sizeof(blind_exe::kSites) / sizeof(blind_exe::kSites[0]), size_t(4));
    for (uint32_t at : blind_exe::kSites) CHECK(at >= blind_exe::kRender && at + 6 <= blind_exe::kRenderEnd);
    uint8_t game[6], wide[6];
    blind_exe::original(game);
    blind_exe::widened(wide);
    CHECK(game[0] == 0xD8 && game[1] == 0x05 && wide[0] == 0xD8 && wide[1] == 0x05);  // fadds dword [address]
    uint32_t from = 0, to = 0;
    std::memcpy(&from, game + 2, 4);
    std::memcpy(&to, wide + 2, 4);
    CHECK_EQ(from, blind_exe::kGameReachAt);
    CHECK_EQ(to, blind_exe::kWideReachAt);
}

TEST(blind_darkness_covers_a_wide_screen_from_afar) {
    // R-MCVUC8: "the black square on the screen is not whole" - the darkness is a square facing the camera round the
    // player, its frame the middle's half side plus 50 units out: from a camera 570 units off it covered three
    // quarters of a 16:9 screen. Plus 1000 it covers it from as far as the game draws anything.
    const camera::Projection p = camera::projectionFor(1920, 1080);
    for (float middle : {50.0f, 40.0f}) CHECK((middle + blind_exe::kGameReach) * p.f0 / 570.0f < 960.0f);  // blind's
    for (float middle : {50.0f, 40.0f, 130.0f}) {  // blind's; the darkness's levels, ten and one
        CHECK((middle + blind_exe::kWideReach) * p.f0 / p.farZ > 960.0f);
    }
}
