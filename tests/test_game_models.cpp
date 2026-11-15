// 3.0.20-beta.3 (stage 2a of «Модели нашим рендером»): the game's static model nodes captured as it draws them - their
// vertices in the world (its node-to-view matrix times the view's inverse), faces, texture coordinates and colours as
// it keeps them - and sent to the renderer whole
#include <cmath>
#include <vector>

#include "core/bridge_wire.hpp"
#include "core/game_models.hpp"
#include "core/view3d.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
bool near(float a, float b) { return std::fabs(a - b) <= 1e-3f; }
}  // namespace

TEST(game_models_node_goes_over_the_wire_whole) {
    gamemodels::Node n;
    n.id = 0x12345678u;
    n.map = 7;
    n.textures = {11, 12};
    n.vertices = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    gamemodels::Face f;
    f.v[0] = 0, f.v[1] = 1, f.v[2] = 2;
    for (int i = 0; i < 6; ++i) f.uv[i] = 0.1f * float(i);
    f.colour[1] = 0xFF8040u;
    f.lit[8] = 300;
    f.texture = 1;
    f.flags = gamemodels::kTwoSided;
    n.faces = {f, f};
    n.flipped = true;  // 3.0.20-beta.4
    std::vector<uint8_t> bytes;
    bridge::WireWriter w(bytes);
    gamemodels::write(w, n);
    bridge::WireReader r(bytes.data(), bytes.size());
    gamemodels::Node back;
    CHECK(gamemodels::read(r, back) && r.ok() && r.left() == 0);
    CHECK(back.id == n.id && back.map == 7 && back.textures == n.textures && back.vertices == n.vertices);
    CHECK(back.faces.size() == 2 && back.faces[1].v[2] == 2 && near(back.faces[1].uv[5], 0.5f));
    CHECK(back.faces[0].colour[1] == 0xFF8040u && back.faces[0].lit[8] == 300 && back.faces[0].texture == 1);
    CHECK(back.faces[0].flags == gamemodels::kTwoSided && back.flipped);
    bridge::WireReader cut(bytes.data(), bytes.size() - 3);
    CHECK(!gamemodels::read(cut, back));  // cut short: refused
}

TEST(game_models_world_from_the_combined_and_the_views_inverse) {
    // the node in the world: turned 90 degrees round y, moved to (10, -5, 20); the view: turned 30 round x, moved
    const float world[12] = {0, 0, -1, 0, 1, 0, 1, 0, 0, 10, -5, 20};
    const float c = std::cos(0.5236f), s = std::sin(0.5236f);
    const float view[12] = {1, 0, 0, 0, c, s, 0, -s, c, -3, 4, 100};
    float combined[12], inverse[12], back[12];
    gamemodels::compose(world, view, combined);
    view3d::inverse(view, inverse);
    gamemodels::worldOf(combined, inverse, back);
    for (int i = 0; i < 12; ++i) CHECK(near(back[i], world[i]));
    const float p[3] = {1, 2, 3};
    float out[3];
    gamemodels::transform(back, p, out);
    CHECK(near(out[0], 13.0f) && near(out[1], -3.0f) && near(out[2], 19.0f));  // x' = z + 10, y' = y - 5, z' = -x + 20
}

// the check in the renderer: the captured corners against our own models' (placed from the map's files) - by a grid
TEST(game_models_corners_found_near_ours_only) {
    gamemodels::Corners ours(1.0f);
    const float a[3] = {10.0f, -5.0f, 20.0f}, b[3] = {-3.5f, 0.0f, 7.25f};
    ours.add(a);
    ours.add(b);
    CHECK(ours.size() == 2);
    const float nearA[3] = {10.1f, -5.0f, 19.95f}, nearB[3] = {-3.45f, 0.02f, 7.3f}, away[3] = {12.0f, -5.0f, 20.0f};
    const float edge[3] = {10.0f, -5.0f, 20.9f};  // in the next cell, yet within reach of a
    CHECK(ours.within(nearA, 0.25f) && ours.within(nearB, 0.25f) && !ours.within(away, 0.25f));
    CHECK(ours.within(edge, 1.0f) && !ours.within(edge, 0.5f));
}

// 3.0.20-beta.4 (stage 2b): the frame's nodes drawn by us in place of the game's, its colour factors and way
TEST(game_models_frame_goes_over_the_wire_whole) {
    gamemodels::Frame f;
    f.ids = {0x1000u, 0x2000u, 0x3000u};
    f.factors[0] = 1.0f, f.factors[1] = 0.75f, f.factors[2] = 0.5f;
    f.lit = true;
    std::vector<uint8_t> bytes;
    bridge::WireWriter w(bytes);
    gamemodels::write(w, f);
    bridge::WireReader r(bytes.data(), bytes.size());
    gamemodels::Frame back;
    CHECK(gamemodels::read(r, back) && r.left() == 0);
    CHECK(back.ids == f.ids && back.lit && back.factors[1] == 0.75f && back.factors[2] == 0.5f);
    bridge::WireReader cut(bytes.data(), 6);
    CHECK(!gamemodels::read(cut, back));
}

// 3.0.20-beta.4 (stage 2b): a face the game draws as its own draw culls them (0x4DED2E): one-sided - shown with its
// corners on the screen turned (x2 - x0)(y1 - y0) > (x1 - x0)(y2 - y0), the other way round for a mirrored model (its
// actor's scale below 0 on an odd number of axes: the draw's second argument); two-sided - always
TEST(game_models_faces_culled_as_the_game_culls_them) {
    const float a[2] = {0, 0}, b[2] = {10, 0}, c[2] = {0, 10};
    CHECK(gamemodels::shown(a, c, b, false, false));   // 10 * 10 > 0 * 0
    CHECK(!gamemodels::shown(a, b, c, false, false));  // 0 * 0 <= 10 * 10
    CHECK(gamemodels::shown(a, b, c, false, true) && !gamemodels::shown(a, c, b, false, true));
    CHECK(gamemodels::shown(a, b, c, true, false) && gamemodels::shown(a, c, b, true, true));
    CHECK(!gamemodels::shown(a, a, a, false, false) && gamemodels::shown(a, a, a, false, true));  // flat: as the game
}
