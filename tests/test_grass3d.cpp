// 3.0.19-beta.33 (the owner: «кстати, я так траву в 3д и не увидел»; the 3D view's concept: «объёмная трава с
// цветами»): the 3D view's grass as blades in the world round the eye - their grid, and its switch [view3d] grass
#include <cmath>

#include "core/config.hpp"
#include "core/grass3d.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(grass3d_grid_round_the_eye) {
    const grass3d::Grid g = grass3d::gridFor(false);
    CHECK(g.side == grass3d::kSide && g.step > 0.0f);
    CHECK(g.blades() == uint32_t(g.side) * uint32_t(g.side) * uint32_t(g.rings));  // 3.0.20-beta.25: each ring's
    CHECK(g.reachOf(0) > 40.0f && g.reachOf(0) < 80.0f);   // the first ring about the eye
    CHECK(g.vertices() == g.blades() * 3u * grass3d::kTuft);  // a tuft of kTuft triangles a root (3.0.19-beta.38)
    CHECK(grass3d::kTuft == 3u);
    const grass3d::Grid thin = grass3d::gridFor(true);  // a weak card's, keep_fps's: fewer, a little further apart
    CHECK(thin.blades() * 2u <= g.blades() && thin.step > g.step);
    CHECK(thin.reach() > 30.0f);
}

TEST(config_view3d_grass_on_by_default_and_live) {
    CHECK(parseConfig("").effects.grass3d);
    CHECK(!parseConfig("[view3d]\ngrass = 0\n").effects.grass3d);
    const settings::IniSetting* row = settings::findSetting("view3d.grass");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && row->control == settings::Control::Check);
    CHECK(appliesNow("view3d.grass"));
    CHECK(!(parseConfig("").effects == parseConfig("[view3d]\ngrass = 0\n").effects));
}

// 3.0.20-beta.25 (R-6YX3UF «трава в 3д режимі трошки далі не видна і якась вона не дуже»: none past some 55 units, its
// edge thinned at random): rings of roots round the eye - each further one twice as far out, its roots twice as far
// apart (as many of them), its blades wider by as much and a little taller; a ring's outer part thinned to the next
// one's density, the last one's to none with its blades shrinking to nothing - no edge, no step between the rings
TEST(grass3d_rings_reach_further_each_twice_as_sparse) {
    const grass3d::Grid g = grass3d::gridFor(false);
    CHECK(g.rings == grass3d::kRings && g.rings == 3);
    CHECK(g.roots() == uint32_t(g.side) * uint32_t(g.side) && g.blades() == g.roots() * 3u);
    CHECK(g.stepOf(1) == 2.0f * g.step && g.reachOf(1) == 2.0f * g.reachOf(0));
    CHECK(g.reach() == g.reachOf(2) && g.reach() > 200.0f);
    CHECK(grass3d::ringOf(g, 10.0f) == 0 && grass3d::ringOf(g, g.reachOf(0) + 1.0f) == 1);
    CHECK(grass3d::ringOf(g, g.reach() - 1.0f) == 2 && grass3d::ringOf(g, g.reach() + 1.0f) == -1);
    CHECK(grass3d::keepOf(g, 0, g.reachOf(0) * 0.5f) == 1.0f);  // whole inside 60 % of its reach
    CHECK(std::fabs(grass3d::keepOf(g, 0, g.reachOf(0) * 0.9999f) - 0.25f) < 0.01f);  // the next ring's density
    CHECK(grass3d::keepOf(g, 1, g.reachOf(0) * 1.0001f) == 1.0f);  // a quarter as dense from there: the same
    CHECK(grass3d::keepOf(g, 2, g.reach() * 0.9999f) < 0.01f);    // the last ring's to none
    CHECK(grass3d::widthOf(0) == 1.0f && grass3d::widthOf(1) == 2.0f && grass3d::widthOf(2) == 4.0f);
    CHECK(grass3d::heightOf(g, 0, 5.0f) == 1.0f && grass3d::heightOf(g, 1, g.reachOf(1) * 0.7f) > 1.0f);
    CHECK(grass3d::heightOf(g, 2, g.reach() * 0.9999f) < 0.01f);  // shrinking to nothing at the last edge
    const grass3d::Grid thin = grass3d::gridFor(true);  // a weak card's: two rings of fewer roots
    CHECK(thin.rings == 2 && thin.blades() * 2u < g.blades() && thin.reach() > 80.0f);
}

// 3.0.20-beta.25 (R-6YX3UF: blades in patches on prt_fild08's olive meadow - its pixels' green 0.99-1.03 of their red,
// the test 1.04); 3.0.20-beta.26 (R-PKPE9D «впечатление что трава мигает»: beta.25's 0.985 a hard edge the meadow lies
// on - a root whose pixel crossed it as the camera moved came and went): how green a pixel is, 0 to 1 - none below
// 0.96 of its red or 1.08 of its blue, all past 1.01 and 1.16; the report's meadow, its sand
TEST(grass3d_greenness_of_an_olive_meadow_not_sand) {
    CHECK(grass3d::greenness(116.0f / 255.0f, 118.0f / 255.0f, 96.0f / 255.0f) == 1.0f);  // the meadow, left
    const float feet = grass3d::greenness(131.0f / 255.0f, 130.0f / 255.0f, 103.0f / 255.0f);  // by the feet
    CHECK(feet > 0.5f && feet < 0.8f);
    CHECK(grass3d::greenness(175.0f / 255.0f, 167.0f / 255.0f, 146.0f / 255.0f) == 0.0f);  // its sand
    CHECK(grass3d::greenness(103.0f / 255.0f, 93.0f / 255.0f, 73.0f / 255.0f) == 0.0f);    // a tree's bark
    CHECK(grass3d::greenness(0.05f, 0.06f, 0.04f) == 0.0f);                                 // the dark
    CHECK(grass3d::greenness(0.5f, 0.5f, 0.5f) == 0.0f);                                    // grey stone
    CHECK(grass3d::greenness(0.0f, 0.4f, 0.0f) == 1.0f);                                    // no red, no blue
}

// 3.0.20-beta.26 (R-PKPE9D): a root by an edge - of the green ground, of a ring's thinning - grows and shrinks with how
// far it lies inside it, never comes and goes whole: its height share by its chance p and its own number u (0-1); all
// of them whole at p 1, none at 0
TEST(grass3d_a_root_by_an_edge_grows_not_blinks) {
    CHECK(grass3d::growOf(1.0f, 0.0f) == 1.0f && grass3d::growOf(1.0f, 0.999f) == 1.0f);
    CHECK(grass3d::growOf(0.0f, 0.0f) == 0.0f && grass3d::growOf(0.0f, 0.5f) == 0.0f);
    const float edge = grass3d::growOf(0.5f, 0.5f);  // just inside: small, a little further in: whole
    CHECK(edge > 0.0f && edge < 0.6f && grass3d::growOf(0.6f, 0.5f) == 1.0f);
    CHECK(grass3d::growOf(0.5f, 0.6f) == 0.0f);
    float before = 0.0f;  // as the chance rises the root only grows
    for (int k = 0; k <= 100; ++k) {
        const float g = grass3d::growOf(float(k) / 100.0f, 0.37f);
        CHECK(g >= before);
        before = g;
    }
}

// 3.0.20-beta.26 (R-PKPE9D, under DLSS's jitter: a far blade under two pixels wide flickered from frame to frame): a
// blade narrower than kMinPixels on the target drawn that wide, as much fainter
TEST(grass3d_a_thin_blade_wider_and_fainter) {
    float wider = 0.0f, alpha = 0.0f;
    grass3d::fadeOf(5.0f, wider, alpha);
    CHECK(wider == 1.0f && alpha == 1.0f);
    grass3d::fadeOf(grass3d::kMinPixels / 2.0f, wider, alpha);
    CHECK(wider == 2.0f && alpha == 0.5f);
    grass3d::fadeOf(0.0f, wider, alpha);  // none across: kept finite
    CHECK(alpha == 0.0f && wider > 0.0f && wider < 1e9f);
}
