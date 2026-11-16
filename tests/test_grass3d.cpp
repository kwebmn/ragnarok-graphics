// 3.0.19-beta.33 (the owner: «кстати, я так траву в 3д и не увидел»; the 3D view's concept: «объёмная трава с
// цветами»): the 3D view's grass as blades in the world round the eye - their grid, and its switch [view3d] grass
#include <cmath>
#include <cstdio>
#include <deque>

#include "core/config.hpp"
#include "core/footsteps.hpp"
#include "core/grass3d.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "core/trample.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(grass3d_grid_round_the_eye) {
    const grass3d::Grid g = grass3d::gridFor(false);
    CHECK(g.side == grass3d::kSide && g.step > 0.0f);
    CHECK(g.blades() == uint32_t(g.side) * uint32_t(g.side) * uint32_t(g.rings));  // 3.0.20-beta.25: each ring's
    CHECK(g.reachOf(0) > 40.0f && g.reachOf(0) < 80.0f);   // the first ring about the eye
    // 3.0.20-beta.47 (the meadow): a tuft of four bent blades of three links in the first ring, of three blades of
    // two links further off - 15 and 9 vertices a blade
    CHECK(grass3d::bladeVertices(grass3d::kLinksNear) == 15u && grass3d::bladeVertices(grass3d::kLinksFar) == 9u);
    CHECK(g.tuftVertices(0) == 60u && g.tuftVertices(1) == 27u && g.tuftVertices(2) == 27u);
    CHECK(g.vertices() == g.roots() * (60u + 27u * 2u));
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

// 3.0.20-beta.25 (R-6YX3UF «трава в 3д режимі трошки далі не видна»: none past some 55 units): rings of roots round
// the eye, each twice as far out, its roots twice as far apart. 3.0.20-beta.27 (R-TBD86F «когда идешь трава двигается
// не естественно»: beta.25's rings had roots of their own and blades twice and four times as wide - a ring's edge went
// along with the eye, and the grass at a place changed its blades as it crossed): a ring takes the half units' cells
// of its own step's corners alone - its roots the nearer rings' roots in the same spots, a level each (levelOf); the
// finer ones grow in round them as the eye nears (presenceOf), a blade's width and height going with the distance
TEST(grass3d_rings_reach_further_each_twice_as_sparse) {
    const grass3d::Grid g = grass3d::gridFor(false);
    CHECK(g.rings == grass3d::kRings && g.rings == 3);
    CHECK(g.roots() == uint32_t(g.side) * uint32_t(g.side) && g.blades() == g.roots() * 3u);  // its rings' roots
    CHECK(g.stepOf(1) == 2.0f * g.step && g.reachOf(1) == 2.0f * g.reachOf(0));
    CHECK(g.reach() == g.reachOf(2) && g.reach() > 200.0f && g.fullTo() * 2.0f == g.reachOf(0));
    CHECK(grass3d::ringOf(g, 10.0f) == 0 && grass3d::ringOf(g, g.reachOf(0) + 1.0f) == 1);
    CHECK(grass3d::ringOf(g, g.reach() - 1.0f) == 2 && grass3d::ringOf(g, g.reach() + 1.0f) == -1);
    const grass3d::Grid thin = grass3d::gridFor(true);  // a weak card's: two rings of fewer roots
    CHECK(thin.rings == 2 && thin.blades() * 2u < g.blades() && thin.reach() > 80.0f);
}

// a root's level - the times its half units' cell halves evenly both ways (at most the last ring's); the level wanted
// by the distance; how much of a root stands there - its level whole, none a level past it, the last over half a level
TEST(grass3d_a_root_keeps_its_place_and_the_finer_ones_grow_in) {
    CHECK(grass3d::levelOf(1, 0, 2) == 0 && grass3d::levelOf(0, 0, 2) == 2 && grass3d::levelOf(2, 4, 2) == 1);
    CHECK(grass3d::levelOf(4, 8, 2) == 2 && grass3d::levelOf(-2, 2, 5) == 1 && grass3d::levelOf(-4, 12, 5) == 2);
    CHECK(grass3d::levelOf(8, 16, 5) == 3 && grass3d::levelOf(1024, 0, 2) == 2);
    const float full = 27.5f;  // all the roots up to it
    CHECK(grass3d::needOf(10.0f, full) == 0.0f && grass3d::needOf(55.0f, full) == 1.0f);
    CHECK(std::fabs(grass3d::needOf(110.0f, full) - 2.0f) < 1e-5f);
    CHECK(grass3d::presenceOf(0, 0.0f, 3) == 1.0f && grass3d::presenceOf(0, 0.5f, 3) == 0.5f);
    CHECK(grass3d::presenceOf(0, 1.0f, 3) == 0.0f && grass3d::presenceOf(1, 1.0f, 3) == 1.0f);
    CHECK(grass3d::presenceOf(1, 1.5f, 3) == 0.5f && grass3d::presenceOf(2, 2.4f, 3) == 1.0f);
    CHECK(grass3d::presenceOf(2, 2.75f, 3) == 0.5f && grass3d::presenceOf(2, 3.0f, 3) == 0.0f);
    // the roots of a field 64 x 64 half units: a quarter of them a level on, none past the last - and no step
    float before = 2.0f;
    for (int k = 0; k <= 300; ++k) {
        const float need = float(k) / 100.0f;
        float sum = 0.0f;
        for (int x = 0; x < 64; ++x)
            for (int z = 0; z < 64; ++z) sum += grass3d::presenceOf(grass3d::levelOf(x, z, 2), need, 3);
        const float density = sum / 4096.0f;
        CHECK(density <= before && before - density < 0.02f + (k == 0 ? 2.0f : 0.0f));
        if (k == 0) CHECK(density == 1.0f);
        if (k == 100) CHECK(std::fabs(density - 0.25f) < 1e-4f);
        if (k == 200) CHECK(std::fabs(density - 0.0625f) < 1e-4f);
        if (k == 300) CHECK(density == 0.0f);
        before = density;
    }
    CHECK(grass3d::widthAt(10.0f, full) == 1.0f && grass3d::widthAt(55.0f, full) == 2.0f);  // as wide on the screen
    CHECK(grass3d::heightAt(0.0f, 3) == 1.0f && std::fabs(grass3d::heightAt(1.0f, 3) - 1.35f) < 1e-5f);
    CHECK(std::fabs(grass3d::heightAt(5.0f, 3) - 1.7f) < 1e-5f);
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
    grass3d::fadeOf(0.1f, wider, alpha);  // 3.0.20-beta.27: six times at most, its coverage the same
    CHECK(wider == grass3d::kMostWider && std::fabs(alpha - 1.0f / grass3d::kMostWider) < 1e-6f);
    grass3d::fadeOf(0.0f, wider, alpha);  // none across: none
    CHECK(alpha == 0.0f && wider == 1.0f);
}

// 3.0.20-beta.47 (the owner's «делай 1-9», 1 - «Трава-луг»; R-YAB82P «трава гораздо лучше, но в 3д она не
// смотрится»): the meadow's blades - which corner of which blade of which tuft a vertex is (the renderers' meadow_vs
// counts the same way); the furthest ring's first, each ring's tufts row by row from the furthest row (the painter's
// order: a near blade over a far one, their colours no longer one)
TEST(grass3d_a_vertex_is_a_corner_of_a_blade_of_a_tuft) {
    const grass3d::Grid g = grass3d::gridFor(false);
    grass3d::Corner c = grass3d::cornerOf(g, 0);
    CHECK(c.ring == g.rings - 1 && c.tuft == 0u && c.member == 0u && c.joint == 0u && c.side == -1);
    // a far blade's nine: a quad (two triangles) then its point
    const int joints[9] = {0, 0, 1, 1, 0, 1, 1, 1, 2}, sides[9] = {-1, 1, -1, -1, 1, 1, -1, 1, 0};
    for (uint32_t k = 0; k < 9; ++k) {
        c = grass3d::cornerOf(g, k);
        CHECK(int(c.joint) == joints[k] && c.side == sides[k] && c.member == 0u && c.tuft == 0u);
    }
    c = grass3d::cornerOf(g, 9);
    CHECK(c.member == 1u && c.joint == 0u && c.tuft == 0u);
    c = grass3d::cornerOf(g, 27);
    CHECK(c.tuft == 1u && c.member == 0u && c.ring == g.rings - 1);
    // the rings in turn, the first ring's last: four blades of three links
    const uint32_t outer = g.roots() * 27u;  // a further ring's vertices
    c = grass3d::cornerOf(g, outer);
    CHECK(c.ring == g.rings - 2 && c.tuft == 0u);
    c = grass3d::cornerOf(g, outer * 2u);
    CHECK(c.ring == 0 && c.tuft == 0u && c.member == 0u && c.joint == 0u);
    c = grass3d::cornerOf(g, outer * 2u + 14u);  // its first blade's point: the third joint
    CHECK(c.ring == 0 && c.member == 0u && c.joint == 3u && c.side == 0);
    c = grass3d::cornerOf(g, outer * 2u + 15u * 3u + 6u);  // the fourth blade's second quad
    CHECK(c.member == 3u && c.joint == 1u && c.side == -1);
    c = grass3d::cornerOf(g, g.vertices() - 1u);
    CHECK(c.ring == 0 && c.tuft == g.roots() - 1u && c.member == 3u && c.joint == 3u && c.side == 0);
}

TEST(grass3d_tufts_are_drawn_from_the_furthest_row) {
    int column = -1, row = -1;
    // (3.0.20-beta.69: inside a row from the furthest too - a little to +x here, the largest column first)
    grass3d::tuftAt(4, 0, 0.1f, 0.9f, column, row);  // looking along +z: the row of the largest z first
    CHECK(row == 3 && column == 3);
    grass3d::tuftAt(4, 15, 0.1f, 0.9f, column, row);
    CHECK(row == 0 && column == 0);
    grass3d::tuftAt(4, 0, 0.1f, -0.9f, column, row);  // along -z: the row of the least z
    CHECK(row == 0 && column == 3);
    grass3d::tuftAt(4, 0, 0.9f, 0.1f, column, row);  // along +x: the furthest column, row by row along it
    CHECK(column == 3 && row == 3);
    grass3d::tuftAt(4, 5, -0.9f, 0.1f, column, row);  // along -x: the second column's second from the furthest
    CHECK(column == 1 && row == 2);
    // every tuft once, whichever way
    const float ways[3][2] = {{0.3f, 0.8f}, {-0.7f, 0.2f}, {0.6f, -0.6f}};
    for (const float* way : {ways[0], ways[1], ways[2]}) {
        bool seen[25] = {};
        for (uint32_t n = 0; n < 25; ++n) {
            grass3d::tuftAt(5, n, way[0], way[1], column, row);
            CHECK(column >= 0 && column < 5 && row >= 0 && row < 5 && !seen[row * 5 + column]);
            if (column >= 0 && column < 5 && row >= 0 && row < 5) seen[row * 5 + column] = true;
        }
    }
}

TEST(grass3d_a_roots_colour_goes_whole_in_one_number) {
    // the roots' picture keeps a root's colour in one float: three bytes in its 24 bits, none lost
    for (const int* c : {(const int[3]){0, 0, 0}, (const int[3]){255, 255, 255}, (const int[3]){76, 138, 50},
                         (const int[3]){1, 254, 128}}) {
        const float packed = grass3d::packBytes(c[0], c[1], c[2]);
        int out[3] = {-1, -1, -1};
        grass3d::unpackBytes(packed, out);
        CHECK(out[0] == c[0] && out[1] == c[1] && out[2] == c[2]);
    }
}

TEST(grass3d_the_meadow_has_clumps_and_an_edge) {
    // taller where the field's own noise is high, never bare; fewer roots where it is low
    CHECK(std::fabs(grass3d::tallOf(0.0f) - grass3d::kTallLow) < 1e-4f);
    CHECK(std::fabs(grass3d::tallOf(1.0f) - grass3d::kTallHigh) < 1e-4f);
    CHECK(grass3d::tallOf(0.5f) > grass3d::kTallLow && grass3d::tallOf(0.5f) < grass3d::tallOf(0.8f));
    CHECK(grass3d::kTallLow >= 1.0f && grass3d::kTallHigh <= 5.0f);  // a character stands some 16 tall
    CHECK(std::fabs(grass3d::chanceOf(0.0f) - grass3d::kThin) < 1e-4f && grass3d::chanceOf(1.0f) == 1.0f);
    // by the green's edge the grass goes down to nothing, not ends in a wall: by the share of green round the root
    CHECK(grass3d::edgeOf(0.2f) == 0.0f && grass3d::edgeOf(0.9f) == 1.0f);
    CHECK(grass3d::edgeOf(0.5f) > 0.2f && grass3d::edgeOf(0.5f) < 0.8f);
    // the fourth blade of a near tuft gone before the first ring's edge (a further ring draws three)
    const grass3d::Grid g = grass3d::gridFor(false);
    CHECK(grass3d::fourthOf(g, 10.0f) == 1.0f && grass3d::fourthOf(g, g.reachOf(0) * 0.95f) == 0.0f);
    CHECK(grass3d::fourthOf(g, g.fullTo() * 1.5f) > 0.0f && grass3d::fourthOf(g, g.fullTo() * 1.5f) < 1.0f);
}

// 3.0.20-beta.58 (the owner: «когда двигаешься трава становится маленькой, когда останавливаешься большой»): the
// steps' grid (core/trample) keeps the share of a blade's height LEFT at a spot - 1 where no step fell, 0 trodden
// flat - and reads 1 past its edges. The 3D view's grass took that share for how hard the spot was trodden: with any
// step still holding (you walk) the whole field stood at 0.4 of its height and the trodden spots at their full one;
// with none left (you stand a while) the grid went and the field rose. A tuft's height by the share left: whole
// where none trod, kTrodLeast of it where trodden flat
TEST(grass3d_a_tufts_height_by_the_share_the_steps_left) {
    CHECK(grass3d::troddenTo(1.0f) == 1.0f);                      // no step there: its whole height
    CHECK(std::fabs(grass3d::troddenTo(0.0f) - grass3d::kTrodLeast) < 1e-6f);  // trodden flat
    CHECK(grass3d::troddenTo(0.5f) > grass3d::troddenTo(0.2f));   // the more left the taller
    CHECK(grass3d::troddenTo(7.0f) == 1.0f && grass3d::troddenTo(-3.0f) == grass3d::kTrodLeast);
    // the steps' own grid: where no step fell the share left is whole - a field walked through keeps its height
    std::deque<footsteps::Step> steps;
    footsteps::Step s;
    s.x = 100.0f, s.z = 50.0f;
    s.born = 0.0f;
    steps.push_back(s);
    trample::Grid grid;
    trample::build(steps, 0.1f, 10.0f, grid);
    CHECK(!grid.texels.empty());
    const auto leftAt = [&](int x, int z) {
        return float(grid.texels[(size_t(z) * size_t(trample::Grid::kSide) + size_t(x)) * 4 + 2]) / 127.0f;
    };
    CHECK(leftAt(0, 0) > 0.99f);  // its edge: nothing trodden - what the shader reads past it
    const int mx = int((100.0f - grid.originX) / grid.cell), mz = int((50.0f - grid.originZ) / grid.cell);
    CHECK(leftAt(mx, mz) < 0.5f);  // under the step
    CHECK(grass3d::troddenTo(leftAt(0, 0)) > 0.99f && grass3d::troddenTo(leftAt(mx, mz)) < 0.75f);
}

// 3.0.20-beta.62 (the owner: «трава вдали дорисовывается, что-то надо придумать, чтобы не так заметно было»): the
// tufts go down to nothing toward the grass's reach - none sprouts at its edge
TEST(grass3d_the_field_tapers_to_nothing_at_its_reach) {
    const grass3d::Grid g = grass3d::gridFor(false);
    CHECK(std::fabs(g.reach() - 220.0f) < 0.01f);
    CHECK_EQ(grass3d::taperOf(g, 0.0f), 1.0f);
    CHECK_EQ(grass3d::taperOf(g, 110.0f), 1.0f);       // whole to the reach's half
    CHECK(grass3d::taperOf(g, 160.0f) < 0.6f);
    CHECK(grass3d::taperOf(g, 160.0f) > 0.4f);
    CHECK(grass3d::taperOf(g, 200.0f) < 0.07f);        // where the last roots used to sprout whole
    CHECK_EQ(grass3d::taperOf(g, 214.0f), 0.0f);
    CHECK_EQ(grass3d::taperOf(g, 500.0f), 0.0f);
    float before = 1.0f;
    for (float d = 100.0f; d <= 220.0f; d += 2.0f) {  // never up again: nothing grows as the eye goes away
        const float now = grass3d::taperOf(g, d);
        CHECK(now <= before);
        before = now;
    }
    const grass3d::Grid thin = grass3d::gridFor(true);
    CHECK_EQ(grass3d::taperOf(thin, thin.reach() * 0.5f), 1.0f);
    CHECK_EQ(grass3d::taperOf(thin, thin.reach()), 0.0f);
}

// 3.0.20-beta.69 (the owner's check list, 3: «не мигает почти, но есть чуть-чуть заметное мелькание» as the view
// turns): the tufts are laid over one another by their alpha, so they must come far to near. The rows did; inside a
// row the tufts went by their column's number whichever way the eye looked - near to far for half of the ways, and
// the other way round once the view crossed a diagonal: the blades' overlaps flipped there. A tuft's neighbour that
// stands further along the view is always drawn before it.
TEST(grass3d_tufts_are_painted_far_to_near_every_way) {
    const int side = 6;
    for (int deg = 5; deg < 360; deg += 10) {
        const float rad = float(deg) * 3.14159265f / 180.0f;
        const float fx = std::cos(rad), fz = std::sin(rad);
        int order[6][6];
        for (uint32_t n = 0; n < uint32_t(side * side); ++n) {
            int column = -1, row = -1;
            grass3d::tuftAt(side, n, fx, fz, column, row);
            order[row][column] = int(n);
        }
        int wrong = 0;
        const int steps[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int row = 0; row < side; ++row)
            for (int column = 0; column < side; ++column)
                for (const auto& s : steps) {
                    const int c = column + s[0], r = row + s[1];
                    if (c < 0 || r < 0 || c >= side || r >= side) continue;
                    const float further = float(s[0]) * fx + float(s[1]) * fz;  // along the view
                    if (further > 0.0f && order[r][c] > order[row][column]) ++wrong;
                }
        if (wrong) std::printf("  looking %d degrees: %d neighbours drawn over a nearer tuft\n", deg, wrong);
        CHECK_EQ(wrong, 0);
    }
}
