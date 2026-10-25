#include <cmath>

#include "core/game_camera.hpp"
#include "core/gnd.hpp"
#include "core/map_axes.hpp"
#include "core/own_ground.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.4-beta.10 (the user: «сделай 1», «продолжай делать прототип»): the own ground drawn in place of the game's -
// where the game would put its vertices, in the game's own colours, on past its far plane
namespace {

float gameDepth(const camera::Projection& p, float zd) { return p.farZ * (1.0f - p.nearZ / zd) / (p.farZ - p.nearZ); }

// The camera at `eye` looking along the world's +z, x to the right, y down (as the game's view: up is -y)
own_ground::View straightAhead(float x, float y, float z) {
    const float m[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, x, y, z};
    own_ground::View v;
    CHECK(own_ground::viewOf(m, v));
    return v;
}

}  // namespace

TEST(own_ground_depth_as_the_game_s_and_on_past_its_far_plane) {
    const camera::Projection p = camera::projectionFor(1920, 1080);
    for (float zd : {20.0f, 300.0f, 1000.0f, 1480.0f})
        CHECK(std::fabs(own_ground::depthOf(p, zd) - gameDepth(p, zd)) < 1e-7f);
    // in order all the way out, never 1 (the far cut), the hills 10 units apart at 2000 still apart
    float last = 0.0f;
    for (float zd = 20.0f; zd < 20000.0f; zd += 5.0f) {
        const float z = own_ground::depthOf(p, zd);
        CHECK(z >= last);
        last = z;
    }
    CHECK(own_ground::depthOf(p, 1e7f) < 1.0f);
    CHECK(own_ground::depthOf(p, 2000.0f) < own_ground::depthOf(p, 2010.0f));
    CHECK(own_ground::depthOf(p, 3000.0f) < own_ground::depthOf(p, 3050.0f));
    // no step where the squeeze starts
    const float split = p.farZ - own_ground::kSqueeze;
    CHECK(std::fabs(own_ground::depthOf(p, split - 0.01f) - own_ground::depthOf(p, split + 0.01f)) < 1e-6f);
}

TEST(own_ground_vertices_where_the_game_puts_its_own) {
    const camera::Projection p = camera::projectionFor(1920, 1080);
    const own_ground::View v = straightAhead(0, 0, -400);
    own_ground::Tl2 t;
    const float file[3] = {10.0f, -20.0f, 100.0f};
    CHECK(own_ground::place(v, p, map::Axes{1, 1, 1}, file, t));
    // 500 ahead of the eye: x right of the middle, y above it (y down on the screen as in the world)
    CHECK(std::fabs(t.x - (10.0f / 500.0f * p.f0 + p.cx)) < 1e-3f);
    CHECK(std::fabs(t.y - (-20.0f / 500.0f * p.f1 + p.cy)) < 1e-3f);
    CHECK(std::fabs(t.rhw - 1.0f / 500.0f) < 1e-9f);
    CHECK(std::fabs(t.z - gameDepth(p, 500.0f)) < 1e-7f);
    // the maps' axes turned: the file's x the world's -x
    CHECK(own_ground::place(v, p, map::Axes{-1, 1, 1}, file, t));
    CHECK(std::fabs(t.x - (-10.0f / 500.0f * p.f0 + p.cx)) < 1e-3f);
    // behind the near plane: none
    const float behind[3] = {0.0f, 0.0f, -395.0f};
    CHECK(!own_ground::place(v, p, map::Axes{1, 1, 1}, behind, t));
}

TEST(own_ground_chunks_in_view) {
    const camera::Projection p = camera::projectionFor(1920, 1080);
    const own_ground::View v = straightAhead(0, 0, 0);
    float nearest = 0.0f, farthest = 0.0f;
    const float aheadLo[3] = {-10, -10, 100}, aheadHi[3] = {10, 10, 120};
    CHECK(own_ground::boxInView(v, p, 1920, 1080, aheadLo, aheadHi, 4000.0f, nearest, &farthest));
    CHECK(std::fabs(nearest - 100.0f) < 1e-3f);
    CHECK(std::fabs(farthest - 120.0f) < 1e-3f);  // 3.0.4-beta.14: the own models draw what lies past the far plane
    const float behindLo[3] = {-10, -10, -120}, behindHi[3] = {10, 10, -100};
    CHECK(!own_ground::boxInView(v, p, 1920, 1080, behindLo, behindHi, 4000.0f, nearest));
    const float rightLo[3] = {5000, -10, 100}, rightHi[3] = {5100, 10, 120};  // far off the screen's right
    CHECK(!own_ground::boxInView(v, p, 1920, 1080, rightLo, rightHi, 4000.0f, nearest));
    const float farLo[3] = {-10, -10, 5000}, farHi[3] = {10, 10, 5100};  // past how far it is drawn
    CHECK(!own_ground::boxInView(v, p, 1920, 1080, farLo, farHi, 4000.0f, nearest));
    const float roundLo[3] = {-10, -10, -50}, roundHi[3] = {10, 10, 50};  // round the eye: kept
    CHECK(own_ground::boxInView(v, p, 1920, 1080, roundLo, roundHi, 4000.0f, nearest));
    CHECK(nearest == 0.0f);
    // 3.0.4-beta.12: under the eye and partly behind it, yet off the screen (below its bottom edge) - left out: kept
    // as "round the eye", prt_fild08's chunks under the camera were most of the vertices projected (7262 of 81264 on
    // the screen at zoom 300)
    const float underLo[3] = {-10, 100, -50}, underHi[3] = {10, 120, 50};
    CHECK(!own_ground::boxInView(v, p, 1920, 1080, underLo, underHi, 4000.0f, nearest));
}

TEST(own_ground_takes_the_game_s_colours) {
    own_ground::Shading s;
    s.reset(4, 2, 10.0f);  // 4 x 2 cells of 10: corners x -20..20, z -10..10
    CHECK_EQ(s.diffuseAt(0.0f, 0.0f), 0xFFFFFFFFu);  // nothing seen yet: white
    // the game's vertices, a hair off the corners as its own projection leaves them
    s.note(-19.97f, -10.02f, 0xFF808080u, true);
    s.note(-10.0f, -10.0f, 0xFF404040u, true);
    s.note(-20.0f, 0.01f, 0xFF808080u, true);
    s.note(-9.98f, 0.0f, 0xFF404040u, true);
    const uint32_t mid = s.diffuseAt(-15.0f, -5.0f);
    for (int k = 0; k < 24; k += 8) CHECK(std::abs(int((mid >> k) & 0xFF) - 0x60) <= 1);
    CHECK_EQ(mid >> 24, 0xFFu);
    // where the game drew nothing yet: the mean of what it did
    const uint32_t far = s.diffuseAt(15.0f, 5.0f);
    for (int k = 0; k < 24; k += 8) CHECK(std::abs(int((far >> k) & 0xFF) - 0x60) <= 1);
    // a corner drawn anew in another colour: the new one counts, once
    s.note(-20.0f, -10.0f, 0xFF404040u, true);
    s.note(-20.0f, 0.0f, 0xFF404040u, true);
    const uint32_t now = s.diffuseAt(15.0f, 5.0f);
    CHECK(std::abs(int(now & 0xFF) - 0x40) <= 1);
    // off the map: none counted, the nearest edge's
    s.note(500.0f, 500.0f, 0xFF000000u, true);
    CHECK(std::abs(int(s.diffuseAt(15.0f, 5.0f) & 0xFF) - 0x40) <= 1);
}

TEST(own_ground_follows_the_game_s_fog) {
    own_ground::Shading s;
    s.reset(4, 2, 10.0f);
    s.beginFrame();
    s.fit();
    CHECK_EQ(s.specularAt(300.0f) >> 24, 0xFFu);  // no fog seen: none
    // alpha = 280 - zd / 4 by the game's vertices, its colour 0x101010
    s.beginFrame();
    for (float zd : {100.0f, 300.0f, 500.0f}) s.fog(zd, uint32_t(280.0f - zd / 4.0f) << 24 | 0x101010u);
    s.fit();
    CHECK(std::abs(int(s.specularAt(400.0f) >> 24) - 180) <= 1);
    CHECK_EQ(s.specularAt(2000.0f) >> 24, 0u);    // past its end: all fog
    CHECK_EQ(s.specularAt(50.0f) >> 24, 0xFFu);   // before its start: none
    CHECK_EQ(s.specularAt(400.0f) & 0xFFFFFFu, 0x101010u);
    // a frame of the game's without fog
    s.beginFrame();
    for (float zd : {100.0f, 900.0f}) s.fog(zd, 0xFF000000u);
    s.fit();
    CHECK_EQ(s.specularAt(5000.0f) >> 24, 0xFFu);
}

TEST(own_ground_tells_which_corners_changed) {
    // 3.0.4-beta.12: the renderer keeps each chunk's colours and makes them again only round the corners the game gave
    // another colour (their look-ups were most of the pass: 4.4 of its 5.7 ms at zoom 1200 on prt_fild08)
    own_ground::Shading s;
    s.reset(4, 2, 10.0f);
    std::vector<uint32_t> changed;
    bool mean = false;
    s.takeChanged(changed, mean);
    CHECK(changed.empty());
    s.note(-20.0f, -10.0f, 0xFF808080u, true);  // corner (0, 0)
    s.note(-10.0f, -10.0f, 0xFF808080u, true);  // corner (1, 0)
    s.takeChanged(changed, mean);
    CHECK_EQ(changed.size(), size_t(2));
    CHECK(mean);  // the first seen: the mean the unseen ones take moved from white
    CHECK(changed.size() == 2 && changed[0] == 0u && changed[1] == 1u);
    s.note(-20.0f, -10.0f, 0xFF808080u, true);  // the same colour again: nothing
    s.takeChanged(changed, mean);
    CHECK(changed.empty() && !mean);
    // corner (4, 2): its index 2 * 5 + 4; the mean moved by less than a little
    s.note(20.0f, 10.0f, 0xFF828282u, true);
    s.takeChanged(changed, mean);
    CHECK(changed.size() == 1 && changed[0] == 14u && !mean);
}

TEST(own_ground_keeps_what_lies_past_the_far_plane) {
    // 3.0.4-beta.14: a model's triangle across the game's far plane - the game draws it up to the plane, the driver the
    // rest: its part past it, as up to four corners, each the share of the triangle's three it is made of
    float w[4][3];
    const float allPast[3] = {1600.0f, 1700.0f, 1800.0f}, allNear[3] = {100.0f, 200.0f, 300.0f};
    CHECK_EQ(own_ground::pastDepth(allPast, 1500.0f, w), 3);
    CHECK(w[0][0] == 1.0f && w[1][1] == 1.0f && w[2][2] == 1.0f);
    CHECK_EQ(own_ground::pastDepth(allNear, 1500.0f, w), 0);
    // one corner past: its tip, cut where the edges cross 1500
    const float onePast[3] = {1000.0f, 1000.0f, 2000.0f};
    CHECK_EQ(own_ground::pastDepth(onePast, 1500.0f, w), 3);
    for (int k = 0; k < 3; ++k) {
        const float z = w[k][0] * onePast[0] + w[k][1] * onePast[1] + w[k][2] * onePast[2];
        CHECK(z >= 1500.0f - 1e-3f);
        CHECK(std::fabs(w[k][0] + w[k][1] + w[k][2] - 1.0f) < 1e-5f);
    }
    // two past: a four-cornered piece
    const float twoPast[3] = {2000.0f, 1000.0f, 2000.0f};
    CHECK_EQ(own_ground::pastDepth(twoPast, 1500.0f, w), 4);
    for (int k = 0; k < 4; ++k) {
        const float z = w[k][0] * twoPast[0] + w[k][1] * twoPast[1] + w[k][2] * twoPast[2];
        CHECK(z >= 1500.0f - 1e-3f);
    }
}

TEST(own_ground_a_corner_keeps_one_cell_s_colour) {
    // 3.0.4-beta.15 (R-N6FC5V: 7.7 ms a frame, 21 at most - the chunks' colours made anew every picture): the game
    // gives a corner a colour in each of the four cells round it; taken in turn, the corner changed four times a
    // picture. Its own colour is the one of the cell whose first corner it is; the others only fill a corner not seen
    own_ground::Shading s;
    s.reset(4, 2, 10.0f);
    std::vector<uint32_t> changed;
    bool mean = false;
    for (int picture = 0; picture < 3; ++picture) {
        s.note(-10.0f, -10.0f, 0xFF404040u, false);  // corner (1, 0) as the cell (0, 0)'s second corner
        s.note(-10.0f, -10.0f, 0xFF808080u, true);   // the cell (1, 0)'s own first corner
        s.note(-10.0f, -10.0f, 0xFF606060u, false);  // and as another's
        s.takeChanged(changed, mean);
        if (picture > 0) CHECK(changed.empty());  // no change from the second picture on
    }
    CHECK_EQ(s.diffuseAt(-10.0f, -10.0f) & 0xFFu, 0x80u);
    // a corner no cell has first (the map's last row): the first colour seen, kept
    s.note(20.0f, 10.0f, 0xFF202020u, false);
    s.note(20.0f, 10.0f, 0xFF303030u, false);
    CHECK_EQ(s.diffuseAt(20.0f, 10.0f) & 0xFFu, 0x20u);
}

TEST(own_ground_keeps_room_in_the_game_s_memory) {
    // 3.0.4-beta.20: uaRO.exe is 32-bit and not large address aware - 2 GB for the game, the driver and the card's
    // driver; lighthalzen's own world takes 214 MB as it is read. Less than kKeepFree left: the far models let go,
    // then the smooth ground, then all of it (the game's own ground)
    using own_ground::Room;
    const uint64_t mb = 1ull << 20;
    CHECK(own_ground::roomFor(1024 * mb, 100 * mb, 50 * mb) == Room::Whole);
    CHECK(own_ground::roomFor(300 * mb, 100 * mb, 50 * mb) == Room::NoModels);
    CHECK(own_ground::roomFor(250 * mb, 100 * mb, 50 * mb) == Room::Coarse);
    CHECK(own_ground::roomFor(100 * mb, 100 * mb, 50 * mb) == Room::None);
    CHECK(own_ground::roomFor(own_ground::kKeepFree, 0, 0) == Room::Whole);
}

TEST(own_ground_brings_the_models_colours_learnt_at_most_once_a_second) {
    // 3.0.4-beta.32 (R-Q3W727 «минукта неподвижно стоял ничего не трогал», prontera): 3571 of the minute's 3600
    // pictures were made anew for the colours learnt from the game's models - its cherry trees sway 4-8 degrees, its
    // signs and clock hands turn, each lit anew every picture. The far models stand still: what was learnt reaches
    // them at most every kModelsColoursEvery pictures, the last picture drawn again meanwhile
    using own_ground::kModelsColoursEvery;
    CHECK(!own_ground::modelsColoursDue(5, 5, 600));                       // nothing learnt anew
    CHECK(!own_ground::modelsColoursDue(5, 6, 0));                         // the picture made just now
    CHECK(!own_ground::modelsColoursDue(5, 9, kModelsColoursEvery - 1));  // a moment more
    CHECK(own_ground::modelsColoursDue(5, 6, kModelsColoursEvery));       // its time: made again
    CHECK(kModelsColoursEvery >= 30 && kModelsColoursEvery <= 120);       // half a second to two at 60 a second
}

TEST(own_ground_draws_its_own_from_where_its_depth_is_squeezed) {
    // 3.0.4-beta.22 (R-ZZN7GY «всеравно осталась черная полоса на воде»): past kSqueeze before the far plane the own
    // ground's depth is squeezed below 1 - a point of the game's there (its water, its models up to 1500) lies behind
    // it in depth though nearer: the game's water between 1490 and 1500 hid behind the own bed, the far water began
    // at 1500, a black strip. The own far water and models are drawn from there, their depth the own ground's
    const camera::Projection p = camera::projectionFor(1920, 1080);
    CHECK_EQ(own_ground::ownFrom(p), p.farZ - own_ground::kSqueeze);
    const float a = p.farZ / (p.farZ - p.nearZ), b = p.farZ * p.nearZ / (p.farZ - p.nearZ);
    const float gameWater = a - b / 1495.0f;  // the game's own water at 1495, its bed behind it at 1510
    CHECK(own_ground::depthOf(p, 1510.0f) < gameWater);  // hidden by the bed: not the game's to draw there
    CHECK(own_ground::depthOf(p, 1485.0f) == a - b / 1485.0f);  // nearer, the game's own depth: its water shows
    const float from = own_ground::ownFrom(p);  // where the two meet, one depth on both sides
    CHECK(std::fabs(own_ground::depthOf(p, from - 0.01f) - own_ground::depthOf(p, from + 0.01f)) < 1e-6f);
}

TEST(own_ground_cuts_a_triangle_where_its_depth_is_squeezed) {
    // 3.0.4-beta.23 (R-W7A5XC «на строениях крыша пропала под эту же линию»): a triangle of the own ground across 1490
    // had the game's depth at its near corners and the squeezed one at its far corners, the GPU carrying the depth
    // straight between them - nearer than either: a quay's wall came out in front of the roofs and the water before it.
    // Cut there, each piece one depth: the near side's part as up to four corners
    float n[4][3], f[4][3];
    const float across[3] = {1480.0f, 1500.0f, 1495.0f};
    CHECK_EQ(own_ground::nearDepth(across, 1490.0f, n), 3);   // its near corner and the two cut points
    CHECK_EQ(own_ground::pastDepth(across, 1490.0f, f), 4);   // its two far corners and the same two points
    int shared = 0;
    for (int a = 0; a < 3; ++a) {
        const float zn = n[a][0] * across[0] + n[a][1] * across[1] + n[a][2] * across[2];
        CHECK(zn <= 1490.0f + 1e-3f);
        for (int b = 0; b < 4; ++b)
            shared += std::fabs(n[a][0] - f[b][0]) < 1e-6f && std::fabs(n[a][1] - f[b][1]) < 1e-6f &&
                      std::fabs(n[a][2] - f[b][2]) < 1e-6f;
    }
    CHECK_EQ(shared, 2);  // the cut points the same on both sides: no crack
    const float allNear[3] = {100.0f, 200.0f, 300.0f};
    CHECK_EQ(own_ground::nearDepth(allNear, 1490.0f, n), 3);
    const float allFar[3] = {1600.0f, 1700.0f, 1800.0f};
    CHECK_EQ(own_ground::nearDepth(allFar, 1490.0f, n), 0);
}

TEST(own_ground_lights_its_slopes_as_the_smooth_hills) {
    // 3.0.4-beta.26 (the user chose «Свет своего мира» with «Своя земля в «Ремастер»»): the smooth hills' light
    // (shaders.cpp hillLight) worked out by the own ground itself - its corners' normals from the map's cells (up along
    // -y), the light of a slope against a flat ground's - near and past the game's far plane alike, where the game's
    // cells (the hills' grid) end
    gnd::Ground g;
    g.width = 3, g.height = 2, g.zoom = 10.0f;
    g.cells.resize(6);
    for (int k = 0; k < 6; ++k) {
        gnd::Cell& c = g.cells[size_t(k)];
        c.top = 0;
        const int x = k % 3;  // rising along x: up is -y, 2 units a cell
        c.h[0] = c.h[2] = -2.0f * float(x);
        c.h[1] = c.h[3] = -2.0f * float(x + 1);
    }
    const std::vector<float> n = own_ground::cornerNormals(g);
    CHECK_EQ(n.size(), size_t(4 * 3 * 3));
    const float len = std::sqrt(0.2f * 0.2f + 1.0f);
    for (size_t k = 0; k < n.size(); k += 3)  // every corner: the slope's normal (-0.2, -1, 0) made one long
        CHECK(std::fabs(n[k] + 0.2f / len) < 1e-5f && std::fabs(n[k + 1] + 1.0f / len) < 1e-5f && n[k + 2] == 0.0f);
    float at[3];
    own_ground::normalAt(n, g.width, g.height, g.zoom, 0.0f, 0.0f, at);  // the map's middle
    CHECK(std::fabs(at[0] + 0.2f / len) < 1e-5f);
    // the light: the sun over a flat ground gives 1; the slope towards the sun brighter, away darker; strength 0: 1
    const float flat[3] = {0.0f, -1.0f, 0.0f}, toSun[3] = {-0.6f, -0.8f, 0.0f};
    CHECK(std::fabs(own_ground::reliefLight(flat, toSun, 0.55f, 1.0f) - 1.0f) < 1e-5f);
    const float facing[3] = {-0.6f, -0.8f, 0.0f}, away[3] = {0.6f, -0.8f, 0.0f};
    CHECK(own_ground::reliefLight(facing, toSun, 0.55f, 1.0f) > 1.0f);
    CHECK(own_ground::reliefLight(away, toSun, 0.55f, 1.0f) < 1.0f);
    CHECK(own_ground::reliefLight(away, toSun, 0.55f, 0.0f) == 1.0f);
    CHECK(own_ground::reliefLight(away, toSun, 0.0f, 1.0f) >= 0.6f);  // as the shader: 0.6 to 1.4
}

TEST(own_ground_lights_its_corners_once_and_its_points_between_them) {
    // 3.0.4-beta.30: the hills' light a corner of the cells, made once a light (a map's 40 000 corners), each point's
    // between its cell's four - 73 ns a point were the normal and the light at each (the first picture of a map 3 ms
    // dearer), the light between the corners costs a few
    gnd::Ground g;
    g.width = 2, g.height = 1, g.zoom = 10.0f;
    g.cells.resize(2);
    for (gnd::Cell& c : g.cells) c.top = 0;
    g.cells[0].h[0] = g.cells[0].h[2] = 0.0f, g.cells[0].h[1] = g.cells[0].h[3] = -5.0f;  // rising along x
    g.cells[1].h[0] = g.cells[1].h[2] = -5.0f, g.cells[1].h[1] = g.cells[1].h[3] = -5.0f;  // flat on top
    const std::vector<float> n = own_ground::cornerNormals(g);
    const float toSun[3] = {-0.6f, -0.8f, 0.0f};
    const std::vector<float> lights = own_ground::cornerLights(n, toSun, 0.55f, 1.0f);
    CHECK_EQ(lights.size(), size_t(3 * 2));
    for (size_t k = 0; k < lights.size(); ++k)
        CHECK(std::fabs(lights[k] - own_ground::reliefLight(&n[k * 3], toSun, 0.55f, 1.0f)) < 1e-6f);
    // between: corner (0, 0) of the slope and corner (1, 0) shared by both cells - half way along the first cell
    const float half = own_ground::lightAt(lights, g.width, g.height, g.zoom, -5.0f, -5.0f);
    CHECK(std::fabs(half - 0.5f * (lights[0] + lights[1])) < 1e-5f);
    CHECK(own_ground::lightAt(lights, g.width, g.height, g.zoom, -10.0f, -5.0f) == lights[0]);  // a corner: its own
    CHECK(own_ground::lightAt({}, g.width, g.height, g.zoom, 0.0f, 0.0f) == 1.0f);  // none: no light
}

