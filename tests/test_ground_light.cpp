#include <string>

#include "core/ground_light.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.22-beta.10 (R-3SA4RB, R-XHRKGN, R-MMMP29: the R7 M370 laptop's ground one flat light colour on Direct3D 11, its
// own textures there): the ground's draws by both stages' textures - a light map (stage 1) none given or not there is
// drawn white, and op 18 (light.rgb + light.a x the rest) makes the ground white with it

TEST(ground_light_draws_by_their_stages) {
    using S = groundlight::Source;
    groundlight::Tally t;
    for (int i = 0; i < 6; ++i) groundlight::note(t, S::Game, S::Game, 18, true);
    for (int i = 0; i < 2; ++i) groundlight::note(t, S::Copy, S::None, 18, false);
    groundlight::note(t, S::Game, S::Gone, 4, false);
    CHECK_EQ(t.draws, uint64_t(9));
    CHECK(groundlight::white(t));
    CHECK_EQ(groundlight::text(t, 3.0),
             std::string("3 draws: stage 0 - the game's textures 2.3, their HD copies 0.7, none given 0, not there 0; "
                         "stage 1 (the light map) - the game's 2, its HD copies 0, none given 0.7, not there 0.3, off "
                         "0; op 18 (light.rgb + light.a x the rest) in 2.7, another in 0.3 (the last 4); the light map "
                         "sampled smoothly in 2"));
    CHECK_EQ(groundlight::text(t, 1.0).substr(0, 9), std::string("9 draws: "));
}

TEST(ground_light_white_only_for_a_texture_missing) {
    using S = groundlight::Source;
    groundlight::Tally t;
    groundlight::note(t, S::Game, S::Game, 18, false);
    groundlight::note(t, S::Copy, S::Copy, 18, false);
    CHECK(!groundlight::white(t));
    groundlight::Tally gone0;
    groundlight::note(gone0, S::Gone, S::Game, 18, false);
    CHECK(groundlight::white(gone0));
    groundlight::Tally none0;
    groundlight::note(none0, S::None, S::Game, 18, false);
    CHECK(groundlight::white(none0));
    CHECK(!groundlight::white(groundlight::Tally{}));
    CHECK_EQ(groundlight::text(groundlight::Tally{}, 0.0).substr(0, 8), std::string("0 draws:"));
    // stage 1 off (op 1): no light map sampled - none given is no white then
    groundlight::Tally off;
    groundlight::note(off, S::Game, S::None, 1, false);
    CHECK(!groundlight::white(off));
    CHECK_EQ(off.stage1Off, uint64_t(1));
    CHECK_EQ(off.stage1[int(S::None)], uint64_t(0));
    CHECK(groundlight::text(off, 1.0).find("not there 0, off 1; op 18") != std::string::npos);
}
