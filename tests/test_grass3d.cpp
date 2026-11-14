// 3.0.19-beta.33 (the owner: «кстати, я так траву в 3д и не увидел»; the 3D view's concept: «объёмная трава с
// цветами»): the 3D view's grass as blades in the world round the eye - their grid, and its switch [view3d] grass
#include "core/config.hpp"
#include "core/grass3d.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(grass3d_grid_round_the_eye) {
    const grass3d::Grid g = grass3d::gridFor(false);
    CHECK(g.side == grass3d::kSide && g.step > 0.0f);
    CHECK(g.blades() == uint32_t(g.side) * uint32_t(g.side));
    CHECK(g.reach() > 40.0f && g.reach() < 80.0f);   // about the eye: past it the ground's own
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
