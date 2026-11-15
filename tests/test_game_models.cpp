// 3.0.20-beta.3 (stage 2a of «Модели нашим рендером»): the game's static model nodes captured as it draws them - their
// vertices in the world (its node-to-view matrix times the view's inverse), faces, texture coordinates and colours as
// it keeps them - and sent to the renderer whole
#include <cmath>
#include <vector>

#include "core/bridge_wire.hpp"
#include "core/game_models.hpp"
#include "core/game_models_place.hpp"
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

// 3.0.20-beta.5 (R-UQPC3E: 865 of 1628 Prontera nodes «moving» by their keys' count): the game moves a node by its
// keys of moving (+0xE8, 16 bytes: the frame, x y z) and turning (+0xF8, 20: the frame, a quaternion) - every model
// runs its animation (the actor's kind 2 from its constructor FUN_004E0540), so a node moves only when the values of
// its keys differ; frames alone do not move it
TEST(game_models_keys_move_a_node_only_when_their_values_differ) {
    struct Turn {
        int32_t frame;
        float q[4];
    };
    Turn keys[3] = {{0, {0.0f, 0.0f, 0.0f, 1.0f}}, {500, {0.0f, 0.0f, 0.0f, 1.0f}}, {1000, {0.0f, 0.0f, 0.0f, 1.0f}}};
    const auto* bytes = reinterpret_cast<const uint8_t*>(keys);
    CHECK(!gamemodels::keysMove(bytes, 3, sizeof(Turn), 4, 16));  // the frames differ, the turn does not
    CHECK(!gamemodels::keysMove(bytes, 1, sizeof(Turn), 4, 16));
    CHECK(!gamemodels::keysMove(nullptr, 0, sizeof(Turn), 4, 16));
    keys[2].q[1] = 0.01f;
    CHECK(gamemodels::keysMove(bytes, 3, sizeof(Turn), 4, 16));
}

// 3.0.20-beta.5: a node drawn by the renderer is placed by its parent's matrix in the world (the draw's first argument
// times the view's inverse) - the same as when it was captured within the float noise of the view's inverse, else
// given back to the game: a part hung on a moving node never stands still
TEST(game_models_places_alike_within_the_float_noise_only) {
    const float a[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 100, -20, 300};
    float b[12];
    for (int i = 0; i < 12; ++i) b[i] = a[i];
    CHECK(gamemodels::samePlace(a, b));
    b[9] += 0.004f;  // the noise of a far place
    b[1] += 0.00005f;
    CHECK(gamemodels::samePlace(a, b));
    b[10] += 0.05f;  // moved
    CHECK(!gamemodels::samePlace(a, b));
    b[10] = a[10];
    b[4] = 0.995f;  // turned
    CHECK(!gamemodels::samePlace(a, b));
}

// 3.0.20-beta.7 (R-Y2JJKH: in the 3D view with «Моделі нашим рендером» on the still models were gone - the renderer let
// the frame's list go at the frame's first clear of the picture, which comes after the list): the list is the
// frame's until its present (the renderer's clears leave it), drawn; the driver told «drawn» from the present on
TEST(game_models_frame_list_kept_until_the_frames_present) {
    gamemodels::FrameState s;
    gamemodels::Frame f;
    f.ids = {11, 12, 13};
    s.told(f);
    CHECK(s.frame().ids.size() == 3);
    s.drawn(true);
    CHECK(!s.shown());  // told from the present on
    s.presented();
    CHECK(s.shown() && s.frame().ids.empty());
    s.presented();  // a picture with no own ground drawn
    CHECK(!s.shown());
}

// 3.0.20-beta.14 (the owner's «делай все» - stage 4: the moving models ours too): a moving node kept with its own
// (node-space) vertices, and the frame's moving nodes with their node-to-view matrices as the game's draw makes them
TEST(game_models_moving_nodes_go_over_the_wire) {
    gamemodels::Node n;
    n.id = 7;
    n.textures = {3};
    n.vertices = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    gamemodels::Face f;
    f.v[0] = 0, f.v[1] = 1, f.v[2] = 2;
    n.faces = {f};
    n.moving = true;
    std::vector<uint8_t> bytes;
    bridge::WireWriter w(bytes);
    gamemodels::write(w, n);
    bridge::WireReader r(bytes.data(), bytes.size());
    gamemodels::Node back;
    CHECK(gamemodels::read(r, back) && r.left() == 0 && back.moving);
    gamemodels::Frame fr;
    fr.ids = {1};
    fr.moving = {7, 8};
    for (int i = 0; i < 24; ++i) fr.matrices.push_back(float(i));
    std::vector<uint8_t> fb;
    bridge::WireWriter fw(fb);
    gamemodels::write(fw, fr);
    bridge::WireReader fr2(fb.data(), fb.size());
    gamemodels::Frame fback;
    CHECK(gamemodels::read(fr2, fback) && fr2.left() == 0);
    CHECK(fback.moving == fr.moving && fback.matrices.size() == 24 && fback.matrices[23] == 23.0f);
    fr.matrices.pop_back();  // a matrix short of its nodes: refused
    std::vector<uint8_t> bad;
    bridge::WireWriter bw(bad);
    gamemodels::write(bw, fr);
    bridge::WireReader br(bad.data(), bad.size());
    CHECK(!gamemodels::read(br, fback));
}

// 3.0.20-beta.14 (stage 4): a moving node's run's box (in its own space) into the world by its matrix this frame - the
// box of its eight corners, for the casters' reach and boxes
TEST(game_models_box_by_a_matrix) {
    const float turned[12] = {0, 1, 0, -1, 0, 0, 0, 0, 1, 10, 20, 30};  // a quarter turn about z, then moved
    const float lo[3] = {0, 0, 0}, hi[3] = {2, 1, 1};
    float a[3], b[3];
    gamemodels::boxBy(turned, lo, hi, a, b);
    // x' = 10 - y (9..10), y' = x + 20 (20..22), z' = z + 30
    CHECK(a[0] == 9.0f && b[0] == 10.0f && a[1] == 20.0f && b[1] == 22.0f && a[2] == 30.0f && b[2] == 31.0f);
}

// 3.0.20-beta.14: a frame's moving nodes kept to its present as its still ones, then let go with their matrices (a
// frame the driver tells nothing of draws none of them by old matrices)
TEST(game_models_frame_moving_kept_to_the_present) {
    gamemodels::FrameState s;
    gamemodels::Frame f;
    CHECK(f.empty());
    f.moving = {7};
    f.matrices.assign(12, 1.0f);
    CHECK(!f.empty());
    s.told(f);
    CHECK(!s.frame().empty());
    s.presented();
    CHECK(s.frame().empty() && s.frame().matrices.empty());
}

// 3.0.20-beta.18 (the owner's «ок», item 2 - with «Моделі нашим рендером» the game draws almost none of its models and
// the far models learnt their colours from little: R-DHG86V 15 %, R-8UPTJW 3 % of the noted triangles ours): a face we
// drew as the game's draw of it was for that learning - its corners in the file's places (a moving node's by its
// matrix into the world, then the map's axes), the colours the game gives them
TEST(game_models_a_face_for_the_far_models) {
    gamemodels::Node n;
    n.vertices = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    gamemodels::Face f;
    f.v[0] = 0, f.v[1] = 1, f.v[2] = 2;
    f.colour[0] = 0x00102030u, f.colour[1] = 0x00405060u, f.colour[2] = 0x00708090u;
    const modelcensus::Colouring plain{modelcensus::Way::Plain, {1.0f, 1.0f, 1.0f}, 0xFF};
    const float sign[3] = {1.0f, -1.0f, 1.0f};
    float corners[3][3];
    uint32_t colours[3];
    gamemodels::faceForLearning(n, f, nullptr, sign, plain, corners, colours);
    CHECK(corners[0][0] == 1.0f && corners[0][1] == -2.0f && corners[2][2] == 9.0f);
    CHECK(colours[0] == modelcensus::expected(plain, 0x00102030u, f.lit) && colours[2] == modelcensus::expected(
                                                                               plain, 0x00708090u, &f.lit[6]));
    const float moved[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 100, 0, 0};  // a moving node's own space into the world
    gamemodels::faceForLearning(n, f, moved, sign, plain, corners, colours);
    CHECK(corners[0][0] == 101.0f && corners[1][1] == -5.0f);
}
