#include "core/game_camera.hpp"
#include "test.hpp"

using namespace uagfx::camera;

TEST(camera_view_z_of_the_target) {
    // A camera at (0, -100, -300) looking along +z with y down: the identity turn and a move of (0, 100, 300).
    const float view[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 100, 300};
    const float at[3] = {5, -100, 0};
    CHECK(viewZ(view, at) == 300.0f);
    const float behind[3] = {0, 0, -300};
    CHECK(viewZ(view, behind) == 0.0f);
}
