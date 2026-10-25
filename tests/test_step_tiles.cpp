#include <cmath>
#include <deque>

#include "core/step_tiles.hpp"
#include "test.hpp"

using namespace uagfx;
using step_tiles::Tiles;
using step_tiles::View;

namespace {

// a camera at the world's origin looking along +z (view = world), 1920x1080, the projection's middle in the middle
View straight() {
    View v;
    const float m[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    for (int i = 0; i < 12; ++i) v.viewToWorld[i] = m[i];
    v.f0 = v.f1 = 1000.0f;
    v.cx = 960.0f;
    v.cy = 540.0f;
    v.nearZ = 10.0f;
    v.width = 1920;
    v.height = 1080;
    return v;
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

footsteps::Step at(float x, float y, float z) {
    footsteps::Step s;
    s.x = x;
    s.y = y;
    s.z = z;
    return s;
}

}  // namespace

// 3.0.5-beta.9 (R-VMPQKX: the footprints 4.1 ms of a Radeon 610M's frame on «Низькі»): a step in the middle of the
// screen marks the tiles its reach covers there, no others
TEST(step_tiles_mark_the_tiles_a_steps_reach_covers) {
    const Tiles t = step_tiles::build({at(0, 0, 200)}, straight());
    CHECK_EQ(t.columns, 30);
    CHECK_EQ(t.rows, 17);
    // its reach: 6.3 across, 3 up and down at 194-206 - pixels 927-993 by 524-556
    CHECK(t.at(14, 8) == 1 && t.at(15, 8) == 1);
    CHECK(t.at(13, 8) == 0 && t.at(16, 8) == 0 && t.at(15, 7) == 0 && t.at(15, 9) == 0);
    CHECK(t.x0 >= 915 && t.x0 <= 927 && t.x1 >= 993 && t.x1 <= 1005);
    CHECK(t.y0 >= 512 && t.y0 <= 524 && t.y1 >= 556 && t.y1 <= 568);
}

TEST(step_tiles_keep_each_step_by_its_place_in_the_list) {
    std::deque<footsteps::Step> steps;
    for (int k = 0; k < 20; ++k) steps.push_back(at(-300, 0, 200));  // off to the left, off the screen
    steps.push_back(at(0, 0, 200));                                   // the 21st: part 1, bit 4
    const Tiles t = step_tiles::build(steps, straight());
    CHECK(t.at(15, 8) == (uint64_t(1) << 20));
    CHECK_EQ(int(t.masks[(size_t(8) * t.columns + 15) * 4 + 1]), 1 << 4);
}

TEST(step_tiles_leave_out_the_steps_not_seen) {
    const Tiles behind = step_tiles::build({at(0, 0, -100)}, straight());
    CHECK(behind.empty());
    for (uint16_t m : behind.masks) CHECK_EQ(int(m), 0);
    const Tiles aside = step_tiles::build({at(5000, 0, 200)}, straight());
    CHECK(aside.empty());
}

// partly behind the camera's near plane: it may show anywhere
TEST(step_tiles_mark_every_tile_for_a_step_across_the_near_plane) {
    const Tiles t = step_tiles::build({at(0, 0, 12)}, straight());
    CHECK(t.at(0, 0) == 1 && t.at(29, 16) == 1);
    CHECK(t.x0 == 0 && t.y0 == 0 && t.x1 == 1920 && t.y1 == 1080);
}

// a picture wider than the texture's tiles: the last column takes the rest
TEST(step_tiles_fold_a_wide_picture_into_the_last_column) {
    View v = straight();
    v.width = 10000;
    v.cx = 9000.0f;  // the step at pixel 9000
    const Tiles t = step_tiles::build({at(0, 0, 200)}, v);
    CHECK_EQ(t.columns, step_tiles::kMaxColumns);
    CHECK(t.at(step_tiles::kMaxColumns - 1, 8) == 1);
}

// the pixel shader turns a pixel back into the view (viewFrom: game = (pixel - jitter) / scale) and the world
// (Constants::reproject); the bounds come from the same view forward - a turned and tilted camera, a smaller world
TEST(step_tiles_project_the_way_steps_ps_turns_pixels_back) {
    View v = straight();
    const float yaw = 0.6f, tilt = -0.8f;
    const float cy = std::cos(yaw), sy = std::sin(yaw), ct = std::cos(tilt), st = std::sin(tilt);
    // the view's ways in the world: x across, y down the screen, z forward
    const float xw[3] = {cy, 0, -sy}, zw[3] = {sy * ct, -st, cy * ct};
    const float yw[3] = {zw[1] * xw[2] - zw[2] * xw[1], zw[2] * xw[0] - zw[0] * xw[2], zw[0] * xw[1] - zw[1] * xw[0]};
    const float cam[3] = {100, -150, -40};
    for (int i = 0; i < 3; ++i) {
        v.viewToWorld[i] = xw[i];
        v.viewToWorld[3 + i] = yw[i];
        v.viewToWorld[6 + i] = zw[i];
        v.viewToWorld[9 + i] = cam[i];
    }
    v.scale = 0.75f;
    v.jitterX = 0.3f;
    v.jitterY = -0.2f;
    v.width = 1440;
    v.height = 810;
    // a step 200 ahead: its world place from the view
    const float ahead[3] = {12, -8, 200};
    footsteps::Step s;
    s.x = cam[0] + ahead[0] * xw[0] + ahead[1] * yw[0] + ahead[2] * zw[0];
    s.y = cam[1] + ahead[0] * xw[1] + ahead[1] * yw[1] + ahead[2] * zw[1];
    s.z = cam[2] + ahead[0] * xw[2] + ahead[1] * yw[2] + ahead[2] * zw[2];
    const Tiles t = step_tiles::build({s}, v);
    // the pixel the shader shows the step's middle at
    const float gx = ahead[0] / ahead[2] * v.f0 + v.cx, gy = ahead[1] / ahead[2] * v.f1 + v.cy;
    const int px = int(gx * v.scale + v.jitterX), py = int(gy * v.scale + v.jitterY);
    CHECK(px > t.x0 && px < t.x1 && py > t.y0 && py < t.y1);
    CHECK(t.at(px / step_tiles::kTile, py / step_tiles::kTile) == 1);
    // and that pixel turned back as steps_ps does lands on the step
    const float g2x = (float(px) - v.jitterX) / v.scale, g2y = (float(py) - v.jitterY) / v.scale;
    const float back[3] = {(g2x - v.cx) * ahead[2] / v.f0, (g2y - v.cy) * ahead[2] / v.f1, ahead[2]};
    float w[3];
    for (int i = 0; i < 3; ++i) w[i] = back[0] * xw[i] + back[1] * yw[i] + back[2] * zw[i] + cam[i];
    CHECK(std::fabs(w[0] - s.x) < 1.0f && std::fabs(w[1] - s.y) < 1.0f && std::fabs(w[2] - s.z) < 1.0f);
}

// steps_ps's t1 as both renderers fill it: row 0 two texels a step (its place and way, then its age), the rows under it
// a texel a tile, its four masks as whole numbers
TEST(step_tiles_fill_the_steps_texture_as_steps_ps_reads_it) {
    std::deque<footsteps::Step> steps{at(0, 0, 200), at(-300, 0, 200)};
    steps[0].angle = 0.5f;
    steps[0].born = 9.0f;
    const Tiles t = step_tiles::build(steps, straight());
    std::vector<float> texels(size_t(step_tiles::kTexelsPerRow) * 4 * size_t(1 + t.rows), -1.0f);
    CHECK_EQ(int(step_tiles::fill(steps, 10.0f, 4.0f, t, texels.data())), 2);
    CHECK(texels[0] == 0.0f && texels[2] == 200.0f && texels[3] == 0.5f);  // the first step's place and way
    CHECK(near(texels[4], 0.25f));                                      // its age: 1 s of 4
    CHECK(texels[8] == -300.0f);                                        // the second step's place
    CHECK(texels[16] == 0.0f);                                          // no third: nothing
    const size_t tile = (size_t(1 + 8) * step_tiles::kTexelsPerRow + 15) * 4;  // tile (15, 8)
    CHECK(texels[tile] == 1.0f && texels[tile + 1] == 0.0f && texels[tile + 3] == 0.0f);
    CHECK(texels[(size_t(1 + 8) * step_tiles::kTexelsPerRow + 16) * 4] == 0.0f);  // a tile without it
}
