// 3.0.19-beta.32 (the 3D view's concept https://claude.ai/artifact/RadnVxG35hjE9d8HgdqzAy, decision 4: «Подземелья:
// свод из отражённого рельефа пещеры и туман, без отдельной настройки плотности»): a cave's vault from its ground's
// heights, mirrored up about the walls' top - down to the walls over them, a dome over the passages
#include <algorithm>
#include <cmath>

#include "core/cave_vault.hpp"
#include "core/config.hpp"
#include "core/gnd.hpp"
#include "core/ground_cells.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "core/rsm.hpp"
#include "core/rsw.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A cave of w x h cells: walls `wall` high three cells round its edge and down its middle from row 9, floors at 0
// between - its corners as a .gnd keeps them (up along -y: a wall's -wall)
ground_heights::Ground cave(int w, int h, float wall) {
    ground_heights::Ground g;
    g.width = w;
    g.height = h;
    g.zoom = 10.0f;
    g.corners.assign(size_t(w) * size_t(h) * 4, 0.0f);
    for (int z = 0; z < h; ++z)
        for (int x = 0; x < w; ++x) {
            const bool walled = x < 3 || x >= w - 3 || z < 3 || z >= h - 3 || ((x == w / 2 - 1 || x == w / 2) && z > 8);
            if (walled)
                for (int k = 0; k < 4; ++k) g.corners[(size_t(z) * size_t(w) + size_t(x)) * 4 + size_t(k)] = -wall;
        }
    return g;
}

float upAt(const cave_vault::Vault& v, int x, int z) { return v.up[size_t(z) * size_t(v.width) + size_t(x)]; }

}  // namespace

TEST(cave_vault_mirrors_the_walls_over_the_passages) {
    cave_vault::Vault v;
    CHECK(cave_vault::build(cave(40, 40, 40.0f), v));
    CHECK(v.width == 40 && v.height == 40 && v.up.size() == 1600u);
    CHECK(std::fabs(v.wallTop - 40.0f) < 0.5f);
    const float mid = upAt(v, 10, 20);  // a passage's middle, 7 cells from any wall: a dome over the walls' top
    CHECK(mid > 50.0f && mid <= cave_vault::kMostRoom + 0.01f);
    const float over = upAt(v, 1, 20);  // over a wall: down by its top, never into it
    CHECK(over >= 40.0f + cave_vault::kLeastOver - 0.01f && over < mid);
}

TEST(cave_vault_leaves_room_over_low_walls) {
    cave_vault::Vault v;
    CHECK(cave_vault::build(cave(30, 30, 14.0f), v));  // the mirror alone: 28 over the floor
    CHECK(upAt(v, 10, 15) >= cave_vault::kLeastRoom - 0.01f);
}

TEST(cave_vault_over_a_flat_cave_a_plain_dome) {
    cave_vault::Vault v;
    CHECK(cave_vault::build(cave(30, 30, 0.0f), v));  // walls of models, the ground flat
    CHECK(std::fabs(upAt(v, 10, 15) - cave_vault::kFlatRoom) < 0.01f);
    CHECK(std::fabs(upAt(v, 1, 1) - cave_vault::kFlatRoom) < 0.01f);
}

TEST(cave_vault_height_between_the_cells) {
    cave_vault::Vault v;
    CHECK(cave_vault::build(cave(20, 20, 30.0f), v));
    float up = 0.0f;
    CHECK(cave_vault::heightAt(v, 5.0f, 5.0f, up) && std::fabs(up - upAt(v, 10, 10)) < 0.01f);  // cell 10's middle
    const float between = 0.5f * (upAt(v, 10, 10) + upAt(v, 11, 10));
    CHECK(cave_vault::heightAt(v, 10.0f, 5.0f, up) && std::fabs(up - between) < 0.01f);  // halfway to cell 11's
    CHECK(!cave_vault::heightAt(v, 1000.0f, 5.0f, up));  // off the map
}

TEST(cave_vault_none_without_a_ground) {
    cave_vault::Vault v;
    CHECK(!cave_vault::build(ground_heights::Ground{}, v));
}

TEST(cave_vault_for_the_dungeons_by_name) {
    CHECK(cave_vault::forMap("pay_dun00") && cave_vault::forMap("gef_dun01") && cave_vault::forMap("anthell01"));
    CHECK(cave_vault::forMap("orcsdun01") && cave_vault::forMap("iz_dun02") && cave_vault::forMap("PAY_DUN01"));
    CHECK(!cave_vault::forMap("prt_in") && !cave_vault::forMap("prontera") && !cave_vault::forMap("in_sphinx1"));
}

// 3.0.19-beta.38 (R-S3NQBF: the vault near black under walls three times lighter): its rock by the map's own light
TEST(cave_vault_rock_by_the_maps_light) {
    const float ambient[3] = {0.4f, 0.35f, 0.3f}, diffuse[3] = {0.6f, 0.5f, 0.4f};
    float rock[3] = {};
    cave_vault::rockOf(true, ambient, diffuse, rock);
    CHECK(std::fabs(rock[0] - (0.4f + 0.6f * 0.35f) * cave_vault::kRock[0]) < 1e-4f);
    CHECK(rock[0] > rock[2]);  // warm
    cave_vault::rockOf(false, ambient, diffuse, rock);
    CHECK(rock[0] == cave_vault::kRockUnlit[0] && rock[2] == cave_vault::kRockUnlit[2]);
    const float dark[3] = {0, 0, 0};
    cave_vault::rockOf(true, dark, dark, rock);
    CHECK(rock[1] >= 0.06f);  // never black
}

// 3.0.20-beta.46 (R-3T9MZC «колоны как будто в воздухе», pay_dun00: its pillars 31-34 tall - the walls' height - their
// black caps hanging 15-30 under a vault mirrored up to 48-66 over the passage): the vault comes down to the posts
// that reach the walls' height, a funnel of rock round each one's top
namespace {

// a model's faces: a shaft `half` wide each way from its middle and `tall` high, its top `topHalf` wide (a pillar's
// flared head wider than the shaft, a stalagmite's point 0) - the model's own frame (its bottom at 0, up along -y)
rsm::Model shaft(float half, float topHalf, float tall) {
    rsm::Model m;
    const float ring[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (int k = 0; k < 4; ++k) {
        const float* a = ring[k];
        const float* b = ring[(k + 1) % 4];
        rsm::Face f, g;
        const float lowA[3] = {a[0] * half, 0.0f, a[1] * half}, lowB[3] = {b[0] * half, 0.0f, b[1] * half};
        const float topA[3] = {a[0] * topHalf, -tall, a[1] * topHalf};
        const float topB[3] = {b[0] * topHalf, -tall, b[1] * topHalf};
        for (int c = 0; c < 3; ++c) {
            f.pos[0][c] = lowA[c], f.pos[1][c] = lowB[c], f.pos[2][c] = topA[c];
            g.pos[0][c] = lowB[c], g.pos[1][c] = topB[c], g.pos[2][c] = topA[c];
        }
        m.faces.push_back(f);
        m.faces.push_back(g);
    }
    return m;
}

// the game's own names (cp949): 외부소품\동굴리얼기둥.rsm - «cave pillar», 내부소품\던전종유석03.rsm - «dungeon
// stalactite», 글래스트\글래스트_기사동상1.rsm - a knight's statue
const char kPillarFile[] = "\xbf\xdc\xba\xce\xbc\xd2\xc7\xb0\\\xb5\xbf\xb1\xbc\xb8\xae\xbe\xf3\xb1\xe2\xb5\xd5.rsm";
const char kStalactiteFile[] = "\xb3\xbb\xba\xce\xbc\xd2\xc7\xb0\\\xb4\xf8\xc0\xfc\xc1\xbe\xc0\xaf\xbc\xae" "03.rsm";
const char kStatueFile[] =
    "\xb1\xdb\xb7\xa1\xbd\xba\xc6\xae\\\xb1\xdb\xb7\xa1\xbd\xba\xc6\xae_\xb1\xe2\xbb\xe7\xb5\xbf\xbb\xf3" "1.rsm";

rsw::Model standing(float x, float z, float scale, const char* file = kPillarFile) {
    rsw::Model at;
    at.file = file;
    at.pos[0] = x, at.pos[1] = 0.0f, at.pos[2] = z;
    at.scale[0] = at.scale[1] = at.scale[2] = scale;
    return at;
}

}  // namespace

TEST(cave_vault_a_post_is_tall_narrow_and_wide_topped) {
    cave_vault::Post p;
    // pay_dun00's pillar, about: 16 across, its head as wide, 40 tall, standing at scale 0.84
    CHECK(cave_vault::postOf(shaft(8.0f, 9.0f, 40.0f), standing(-385.0f, -305.0f, 0.84f), p));
    CHECK(std::fabs(p.x + 385.0f) < 0.01f && std::fabs(p.z + 305.0f) < 0.01f);
    CHECK(std::fabs(p.top - 40.0f * 0.84f) < 0.01f);
    CHECK(std::fabs(p.radius - 9.0f * 0.84f) < 0.01f);  // its head's half width
    CHECK(!cave_vault::postOf(shaft(8.0f, 0.0f, 40.0f), standing(0, 0, 1), p));    // a stalagmite: a point
    CHECK(!cave_vault::postOf(shaft(8.0f, 2.0f, 40.0f), standing(0, 0, 1), p));    // nearly one
    CHECK(!cave_vault::postOf(shaft(30.0f, 30.0f, 40.0f), standing(0, 0, 1), p));  // a block of rock: wider than tall
    CHECK(!cave_vault::postOf(shaft(4.0f, 4.0f, 12.0f), standing(0, 0, 1), p));    // a stump
    CHECK(!cave_vault::postOf(rsm::Model{}, standing(0, 0, 1), p));                // nothing
    // by its makers' own name for it: a pillar («기둥») or a stalactite («종유석») - the census of the 113 dungeon maps'
    // models found statues, clocks, curtains and trees of a pillar's shape too
    CHECK(cave_vault::postOf(shaft(8.0f, 9.0f, 40.0f), standing(0, 0, 1, kStalactiteFile), p));
    CHECK(!cave_vault::postOf(shaft(8.0f, 9.0f, 40.0f), standing(0, 0, 1, kStatueFile), p));
    CHECK(!cave_vault::postOf(shaft(8.0f, 9.0f, 40.0f), standing(0, 0, 1, "tree01.rsm"), p));
    CHECK(cave_vault::postName(kPillarFile) && cave_vault::postName(kStalactiteFile));
    CHECK(!cave_vault::postName(kStatueFile) && !cave_vault::postName("") && !cave_vault::postName("pillar.rsm"));
    // the word's four bytes across two other characters' halves (..B1 E2B5 D5..) are not the word
    CHECK(!cave_vault::postName("\xa1\xb1\xe2\xb5\xd5\xa1.rsm"));
}

TEST(cave_vault_refined_keeps_its_heights) {
    cave_vault::Vault coarse, v;
    CHECK(cave_vault::build(cave(40, 40, 40.0f), coarse));
    v = coarse;
    cave_vault::refine(v, 4);
    CHECK(v.width == 160 && v.height == 160 && v.up.size() == 25600u);
    CHECK(std::fabs(v.zoom - 2.5f) < 1e-5f && v.wallTop == coarse.wallTop && v.floorLevel == coarse.floorLevel);
    float a = 0.0f, b = 0.0f;
    for (const float* at : {(const float[2]){-95.0f, 5.0f}, (const float[2]){-150.0f, -120.0f},
                            (const float[2]){0.0f, 100.0f}, (const float[2]){-55.0f, 5.0f}}) {
        CHECK(cave_vault::heightAt(coarse, at[0], at[1], a) && cave_vault::heightAt(v, at[0], at[1], b));
        CHECK(std::fabs(a - b) < 1.5f);  // smooth between the cells' middles: the same vault, no creases
    }
    // over a wall's top still over it
    CHECK(cave_vault::heightAt(v, 5.0f, 100.0f, b) && b >= 40.0f + cave_vault::kLeastOver - 1.5f);
}

TEST(cave_vault_comes_down_to_a_post_that_reaches_the_walls) {
    const ground_heights::Ground g = cave(40, 40, 40.0f);
    cave_vault::Vault plain;
    CHECK(cave_vault::build(g, plain));
    cave_vault::refine(plain, cave_vault::kFine);
    // a passage's middle (cell 10, 20: x -95, z 5), the vault well over the walls' top there
    cave_vault::Post post;
    post.x = -95.0f, post.z = 5.0f, post.radius = 8.0f, post.top = 38.0f;
    float before = 0.0f, up = 0.0f;
    CHECK(cave_vault::heightAt(plain, post.x, post.z, before) && before > 50.0f);
    cave_vault::Vault v = plain;
    CHECK_EQ(cave_vault::restOn(v, g, {post}), size_t(1));
    // over its head the rock: its cap within it, all across
    for (const float* d : {(const float[2]){0.0f, 0.0f}, (const float[2]){7.5f, 0.0f}, (const float[2]){0.0f, -7.5f},
                           (const float[2]){5.5f, 5.5f}}) {
        CHECK(cave_vault::heightAt(v, post.x + d[0], post.z + d[1], up));
        CHECK(up <= post.top - cave_vault::kPostSink + 0.05f && up >= post.top - cave_vault::kPostSink - 0.05f);
    }
    // a funnel up from it: higher the further off, the vault's own height past its spread
    float last = post.top - cave_vault::kPostSink;
    for (float d = 10.0f; d <= post.radius + cave_vault::kPostSpread; d += 4.0f) {
        CHECK(cave_vault::heightAt(v, post.x + d, post.z, up) && up >= last - 0.01f);
        last = up;
    }
    const float past = post.radius + cave_vault::kPostSpread + 6.0f;
    CHECK(cave_vault::heightAt(v, post.x + past, post.z, up));
    CHECK(cave_vault::heightAt(plain, post.x + past, post.z, before));
    CHECK(std::fabs(up - before) < 0.01f);
    CHECK(last > post.top + 8.0f);  // and it does rise: the vault's own near its rim
    // a stone that does not reach the walls' height (under kPostReach of it) - the vault as it was
    cave_vault::Post stone = post;
    stone.top = 22.0f;
    cave_vault::Vault same = plain;
    CHECK_EQ(cave_vault::restOn(same, g, {stone}), size_t(0));
    CHECK(same.up == plain.up);
    // one the vault lies under already (a wall's model taller than the walls) - as it was
    cave_vault::Post tall = post;
    tall.top = 200.0f;
    CHECK_EQ(cave_vault::restOn(same, g, {tall}), size_t(0));
    CHECK(same.up == plain.up);
    // one off the map - none
    cave_vault::Post off = post;
    off.x = 5000.0f;
    CHECK_EQ(cave_vault::restOn(same, g, {off}), size_t(0));
}

TEST(cave_vault_built_with_its_posts_finer_only_where_one_reaches_it) {
    const ground_heights::Ground g = cave(40, 40, 40.0f);
    cave_vault::Post post;
    post.x = -95.0f, post.z = 5.0f, post.radius = 8.0f, post.top = 38.0f;
    cave_vault::Vault v;
    size_t met = 99;
    CHECK(cave_vault::build(g, {post}, v, &met));
    CHECK_EQ(met, size_t(1));
    CHECK(v.width == 40 * cave_vault::kFine && std::fabs(v.zoom - 10.0f / float(cave_vault::kFine)) < 1e-5f);
    float up = 0.0f;
    CHECK(cave_vault::heightAt(v, post.x, post.z, up) && std::fabs(up - (post.top - cave_vault::kPostSink)) < 0.05f);
    // no post that reaches it: the vault as it always was, a height a cell
    cave_vault::Post stone = post;
    stone.top = 22.0f;
    cave_vault::Vault plain, same;
    CHECK(cave_vault::build(g, plain));
    CHECK(cave_vault::build(g, {stone}, same, &met));
    CHECK(met == 0 && same.width == 40 && same.up == plain.up);
    CHECK(cave_vault::build(g, {}, same) && same.width == 40 && same.up == plain.up);
    CHECK(!cave_vault::build(ground_heights::Ground{}, {post}, same, &met) && met == 0);
}

// 3.0.20-beta.52 (the owner's «делай 1-9», 5 - ceilings indoors): in a house or a castle's hall (no sky, no
// dungeon) the 3D view showed a dark nothing over the walls. A ceiling: flat, kCeilingOver over the walls' top where
// the map's ground makes its walls; where its walls are models (its ground flat) at their height - the 80th of a
// hundred of its tall models' heights - or kCeilingFlat with none known
TEST(cave_vault_a_ceiling_indoors_flat_at_the_walls_top) {
    cave_vault::Vault v;
    CHECK(cave_vault::buildCeiling(cave(40, 40, 36.0f), 0.0f, v));
    CHECK(v.width == 40 && v.height == 40 && std::fabs(v.wallTop - 36.0f) < 0.5f);
    CHECK(std::fabs(v.ceiling - (36.0f + cave_vault::kCeilingOver)) < 0.5f);
    CHECK(std::fabs(upAt(v, 10, 20) - v.ceiling) < 0.01f);  // a room's middle
    CHECK(std::fabs(upAt(v, 5, 6) - v.ceiling) < 0.01f);    // and by its wall: flat
    CHECK(upAt(v, 1, 1) >= 36.0f + cave_vault::kLeastOver - 0.01f);  // over a wall's top never under it
    float up = 0.0f;
    CHECK(cave_vault::heightAt(v, -95.0f, 5.0f, up) && std::fabs(up - v.ceiling) < 0.01f);
    // walls of models over a flat ground: by the models' height, or the usual room
    CHECK(cave_vault::buildCeiling(cave(30, 30, 0.0f), 52.0f, v));
    CHECK(std::fabs(v.ceiling - (52.0f + cave_vault::kCeilingOver)) < 0.01f);
    CHECK(std::fabs(upAt(v, 10, 15) - v.ceiling) < 0.01f);
    CHECK(cave_vault::buildCeiling(cave(30, 30, 0.0f), 0.0f, v));
    CHECK(std::fabs(v.ceiling - cave_vault::kCeilingFlat) < 0.01f);
    CHECK(!cave_vault::buildCeiling(ground_heights::Ground{}, 0.0f, v));
    // a vault is none: its ceiling 0
    CHECK(cave_vault::build(cave(40, 40, 40.0f), v) && v.ceiling == 0.0f);
    // the walls' height by the models: the 80th of a hundred of the tall ones' heights
    CHECK(cave_vault::wallsByModels({}) == 0.0f);
    CHECK(std::fabs(cave_vault::wallsByModels({40.0f, 42.0f, 38.0f, 41.0f, 30.0f, 22.0f, 40.0f, 39.0f, 40.0f, 90.0f}) -
                    41.0f) < 0.01f);
    // which maps: a house's or a hall's by its name - «_in», «in_», a church, a castle's inside, a tower, the
    // pyramids; not a dungeon (its vault), not under the sky, and none of the rest the sky's rule takes for a roof
    // (a guild castle is mostly courtyards, an arena or a graveyard has no roof at all)
    for (const char* in : {"prt_in", "payon_in01", "in_sphinx1", "prt_church", "prt_castle", "gl_cas01", "gl_knt01",
                           "c_tower2", "gef_tower", "moc_pryd03", "PRT_IN"})
        CHECK(cave_vault::ceilingFor(in, false));
    for (const char* out : {"pay_dun00", "aldeg_cas01", "prtg_cas03", "pvp_y_1-1", "gl_chyard", "bat_a01",
                            "prt_maze01", "job_thief1", ""})
        CHECK(!cave_vault::ceilingFor(out, false));
    CHECK(!cave_vault::ceilingFor("prontera", true) && !cave_vault::ceilingFor("prt_in", true));
}

TEST(config_view3d_ceiling_on_by_default_and_live) {
    CHECK(parseConfig("").effects.ceiling3d);
    CHECK(!parseConfig("[view3d]\nceiling = 0\n").effects.ceiling3d);
    const settings::IniSetting* row = settings::findSetting("view3d.ceiling");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && row->control == settings::Control::Check);
    CHECK(appliesNow("view3d.ceiling"));
}

// 3.0.20-beta.54 (the owner's «делай 1-9», 5, part 2 - «свод пещер получает рисунок скалы самой карты вместо шума»):
// the dungeon's own rock - the texture its raised ground wears most: its walls' faces by their area and its tops that
// stand steep or between the passages and the walls' top; the makers' fillers never. By the census of the 118 dungeon
// maps (/root/uagfx-tools/audit/vault/rockpick.cpp): pay_dun00 동굴던전03, anthell01 antflat-3, iz_dun03 iz-03
namespace {

// a cave 12 x 12: its outer ring walls 40 high with the makers' filler on top, the next ring a ledge 20 high of
// `ledge` (a texture's number), the rest floors with two steep cells of moss; `turned`: the ledge's picture laid the
// other way round (its u along the cell's other side)
gnd::Ground rocky(int ledge, bool turned = false) {
    gnd::Ground g;
    g.width = 12, g.height = 12;
    g.zoom = 10.0f;
    g.textures = {"floor.bmp", "rock.bmp", "BACKSIDE.BMP", "moss.bmp"};
    const auto surface = [&](int texture, float span, bool other = false) {
        gnd::Surface s;
        for (int k = 0; k < 4; ++k) {
            const float along = (k & 1) ? span : 0.0f, across = (k & 2) ? span : 0.0f;
            s.u[k] = other ? across : along;
            s.v[k] = other ? along : across;
        }
        s.texture = int16_t(texture);
        g.surfaces.push_back(s);
        return int32_t(g.surfaces.size() - 1);
    };
    const int32_t floorTop = surface(0, 0.5f), ledgeTop = surface(ledge, 0.25f, turned), blackTop = surface(2, 1.0f);
    const int32_t mossTop = surface(3, 0.5f), ledgeWall = surface(ledge, 0.25f), blackWall = surface(2, 1.0f);
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 12; ++x) {
            const int ring = std::min(std::min(x, y), std::min(11 - x, 11 - y));
            gnd::Cell c{};
            const float up = ring == 0 ? 40.0f : ring == 1 ? 20.0f : 0.0f;
            for (float& h : c.h) h = -up;
            c.top = ring == 0 ? blackTop : ring == 1 ? ledgeTop : floorTop;
            if (ring == 0) c.front = c.right = blackWall;
            if (ring == 1) c.front = c.right = ledgeWall;
            if ((x == 5 && y == 5) || (x == 6 && y == 6)) {  // a hummock: steep
                c.h[2] = c.h[3] = -8.0f;
                c.top = mossTop;
            }
            g.cells.push_back(c);
        }
    return g;
}

}  // namespace

TEST(cave_vault_its_rock_is_what_the_raised_ground_wears) {
    const cave_vault::Rock rock = cave_vault::rockPicture(rocky(1));
    CHECK(rock.texture == 1);                      // the ledge's and its faces' - not the floor's, not the moss's
    CHECK(std::fabs(rock.cells - 4.0f) < 1e-4f);  // a quarter of the picture a cell
    CHECK(std::fabs(cave_vault::rockPicture(rocky(1, true)).cells - 4.0f) < 1e-4f);  // laid the other way round
    // the makers' filler never: the walls' tops and their faces wear it, more than anything
    CHECK(cave_vault::rockPicture(rocky(2)).texture == -1);  // (the ledge the filler's too: two cells of moss no rock)
    gnd::Ground low = rocky(1);
    for (gnd::Cell& c : low.cells) {
        for (float& h : c.h) h = 0.0f;
        c.front = c.right = -1;
    }
    CHECK(cave_vault::rockPicture(low).texture == -1);  // nothing raised: none - the vault's own noise
    CHECK(cave_vault::rockPicture(gnd::Ground{}).texture == -1);
    gnd::Ground odd = rocky(1);
    odd.surfaces[1].texture = 99;  // a surface of a texture the file has not: left out, no crash
    CHECK(cave_vault::rockPicture(odd).texture == 1);
}

// the picture on the vault by the map's own light alone (rockOf's without its warm stone - the picture has its own)
TEST(cave_vault_its_rock_picture_lit_by_the_maps_light) {
    const float ambient[3] = {0.4f, 0.35f, 0.3f}, diffuse[3] = {0.6f, 0.5f, 0.4f};
    float light[3] = {};
    cave_vault::rockLight(true, ambient, diffuse, light);
    CHECK(std::fabs(light[0] - (0.4f + 0.6f * 0.35f)) < 1e-4f && std::fabs(light[2] - (0.3f + 0.4f * 0.35f)) < 1e-4f);
    const float bright[3] = {0.9f, 0.9f, 0.9f}, dark[3] = {0, 0, 0};
    cave_vault::rockLight(true, bright, bright, light);
    CHECK(light[1] == 1.0f);  // no brighter than the picture
    cave_vault::rockLight(true, dark, dark, light);
    CHECK(light[1] == cave_vault::kRockLightLeast);  // never black
    cave_vault::rockLight(false, ambient, diffuse, light);
    CHECK(light[0] == cave_vault::kRockLightUnlit && light[2] == cave_vault::kRockLightUnlit);
}

// 3.0.20-beta.54 (the ceilings of beta.52 «в цвет верхней части стен»): the ceiling's colour by the room's walls -
// over a map's tall models the faces of their upper half, each by its area, in their textures' mean colours
TEST(cave_vault_a_ceilings_colour_by_the_walls_upper_part) {
    // a wall 40 tall, 10 wide: its lower half of texture 0, its upper half of texture 1 (two faces), and a small
    // face of texture 2 at its very top
    rsm::Model wall;
    wall.textures = {"plinth.bmp", "plaster.bmp", "trim.bmp", "unknown.bmp"};
    const auto quad = [&](float y0, float y1, float w, int texture) {
        rsm::Face a, b;
        const float p[4][3] = {{0, y0, 0}, {w, y0, 0}, {w, y1, 0}, {0, y1, 0}};
        for (int c = 0; c < 3; ++c) {
            a.pos[0][c] = p[0][c], a.pos[1][c] = p[1][c], a.pos[2][c] = p[2][c];
            b.pos[0][c] = p[0][c], b.pos[1][c] = p[2][c], b.pos[2][c] = p[3][c];
        }
        a.texture = b.texture = texture;
        wall.faces.push_back(a);
        wall.faces.push_back(b);
    };
    quad(0.0f, -20.0f, 10.0f, 0);   // the plinth (up along -y)
    quad(-20.0f, -38.0f, 10.0f, 1);  // the plaster: 180 of area
    quad(-38.0f, -40.0f, 10.0f, 2);  // the trim: 20
    quad(-30.0f, -40.0f, 5.0f, 3);   // a picture not known: not weighed
    const std::vector<cave_vault::Mean> means = {
        {{0.2f, 0.1f, 0.1f}, true}, {{0.8f, 0.7f, 0.6f}, true}, {{0.4f, 0.3f, 0.2f}, true}, {{0, 0, 0}, false}};
    cave_vault::Tone tone;
    cave_vault::wallTone(wall, standing(0, 0, 1), means, tone);
    float rgb[3] = {};
    CHECK(cave_vault::toneOf(tone, rgb));
    // 180 of the plaster and 20 of the trim; none of the plinth
    CHECK(std::fabs(rgb[0] - (0.8f * 180.0f + 0.4f * 20.0f) / 200.0f) < 1e-3f);
    CHECK(std::fabs(rgb[2] - (0.6f * 180.0f + 0.2f * 20.0f) / 200.0f) < 1e-3f);
    // twice as large a wall weighs four times as much: the room's colour is its large walls'
    cave_vault::Tone two;
    cave_vault::wallTone(wall, standing(0, 0, 2), means, two);
    CHECK(std::fabs(two.weight - 4.0 * tone.weight) < 1e-2 * tone.weight);
    cave_vault::Tone none;
    CHECK(!cave_vault::toneOf(none, rgb));
    cave_vault::wallTone(rsm::Model{}, standing(0, 0, 1), means, none);
    CHECK(!cave_vault::toneOf(none, rgb));
    // a picture's mean colour: its see-through texels (alpha 0) left out; none of it seen - not known
    const uint32_t texels[4] = {0xFF804020u, 0xFF402010u, 0x00FF00FFu, 0x00FF00FFu};
    const cave_vault::Mean m = cave_vault::meanOf(texels, 4);
    CHECK(m.known && std::fabs(m.rgb[0] - (128.0f + 64.0f) / 510.0f) < 1e-4f);
    CHECK(std::fabs(m.rgb[2] - (32.0f + 16.0f) / 510.0f) < 1e-4f);
    CHECK(!cave_vault::meanOf(texels + 2, 2).known && !cave_vault::meanOf(nullptr, 0).known);
    // the ceiling's colour: the walls' by the map's light, a little under them (it faces down); none known - rockOf's
    const float ambient[3] = {0.5f, 0.5f, 0.5f}, diffuse[3] = {1.0f, 1.0f, 1.0f}, walls[3] = {0.8f, 0.6f, 0.4f};
    float out[3] = {};
    cave_vault::ceilingColour(true, walls, true, ambient, diffuse, out);
    CHECK(std::fabs(out[0] - 0.8f * 0.85f * cave_vault::kCeilingShade) < 1e-4f);
    CHECK(std::fabs(out[2] - 0.4f * 0.85f * cave_vault::kCeilingShade) < 1e-4f);
    float stone[3] = {};
    cave_vault::rockOf(true, ambient, diffuse, stone);
    cave_vault::ceilingColour(false, walls, true, ambient, diffuse, out);
    CHECK(out[0] == stone[0] && out[1] == stone[1] && out[2] == stone[2]);
}

TEST(config_view3d_vault_rock_on_by_default_and_live) {
    CHECK(parseConfig("").effects.vaultRock);
    CHECK(!parseConfig("[view3d]\nvault_rock = 0\n").effects.vaultRock);
    const settings::IniSetting* row = settings::findSetting("view3d.vault_rock");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && row->control == settings::Control::Check);
    CHECK(appliesNow("view3d.vault_rock"));
}

// 3.0.20-beta.54: the rock's own picture. A cave's ground textures are painted sheets, four cells by four - the
// makers' blackness (the walls' unseen tops), a band of rock, the floor's sand (pay_dun00's 동굴던전01-03, orcsdun01's
// ge-flat2-4: /root/uagfx-tools/audit/vault/rock_candidates.png) - none of them lies whole on a vault. The vault
// takes a patch: the largest square of a sheet that is all rock - within the parts the raised ground shows (not
// those the floors show), clear of the blackness and its soft edge
namespace {

// a cave as rocky(1)'s, its ledge two rings wide, laid from a sheet 64 x 64: the ledge's cells show its second and
// third rows (v 0.25-0.75), the walls' tops its first (v 0-0.25: black), the floors its last (v 0.75-1: sand)
gnd::Ground sheeted() {
    gnd::Ground g;
    g.width = 12, g.height = 12;
    g.zoom = 10.0f;
    g.textures = {"sheet.bmp"};
    const auto surface = [&](float u0, float v0) {
        gnd::Surface s;
        for (int k = 0; k < 4; ++k) {
            s.u[k] = u0 + ((k & 1) ? 0.25f : 0.0f);
            s.v[k] = v0 + ((k & 2) ? 0.25f : 0.0f);
        }
        s.texture = 0;
        g.surfaces.push_back(s);
        return int32_t(g.surfaces.size() - 1);
    };
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 12; ++x) {
            const int ring = std::min(std::min(x, y), std::min(11 - x, 11 - y));
            gnd::Cell c{};
            const float up = ring == 0 ? 40.0f : ring <= 2 ? 20.0f : 0.0f;
            for (float& h : c.h) h = -up;
            const float u0 = 0.25f * float(x % 4);
            c.top = ring == 0 ? surface(u0, 0.0f) : ring <= 2 ? surface(u0, 0.25f + 0.25f * float(y % 2))
                                                             : surface(u0, 0.75f);
            g.cells.push_back(c);
        }
    return g;
}

// the sheet: black down to row 16, then the rock coming out of it over 6 rows (a soft edge: the rock's own colour,
// a seventh of it more each row), rock (two greys speckled in no order, a crack or two) to row 48, sand under it
std::vector<uint32_t> sheet() {
    std::vector<uint32_t> p(64 * 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const uint32_t speck =
                (uint32_t(x) * 374761393u + uint32_t(y) * 668265263u) ^ (uint32_t(x * y) * 2246822519u);
            uint32_t c = ((speck >> 13) & 1) ? 0xFF6A7A52u : 0xFF56664Au;  // rock
            if (y < 16) c = 0xFF000000u;
            else if (y < 22)
                c = 0xFF000000u | uint32_t(0x6A * (y - 15) / 7) << 16 | uint32_t(0x7A * (y - 15) / 7) << 8 |
                    uint32_t(0x52 * (y - 15) / 7);
            else if (y >= 48) c = 0xFFB4A05Au;  // sand
            p[size_t(y) * 64 + size_t(x)] = c;
        }
    p[size_t(30) * 64 + 20] = p[size_t(37) * 64 + 41] = 0xFF050505u;  // cracks: a texel each - still rock
    return p;
}

}  // namespace

TEST(cave_vault_its_rock_patch_is_all_rock) {
    const gnd::Ground g = sheeted();
    const std::vector<uint32_t> pic = sheet();
    size_t asked = 0;
    const ground_cells::PictureOf pictureOf = [&](size_t texture, ground_cells::Picture& out) {
        ++asked;
        if (texture != 0) return false;
        out = {64, 64, pic.data()};
        return true;
    };
    const cave_vault::Patch patch = cave_vault::rockPatch(g, pictureOf);
    CHECK(patch.texture == 0 && asked == 1);
    CHECK(std::fabs(patch.texels - 64.0f * 0.25f / 10.0f) < 1e-4f);  // 1.6 texels a unit
    // within the rows the ledge shows (16-48), clear of the blackness and of its soft edge's dark half (the rows to
    // 18: under kPatchBright of the rock's brightness) and of the sand (from 48): a square as large as fits - and no
    // larger than kPatchUnits of the world
    CHECK(patch.side >= 20 && patch.side <= int(cave_vault::kPatchUnits * patch.texels + 0.5f));
    CHECK(patch.y >= 19 && patch.y + patch.side <= 48);
    CHECK(patch.x >= 0 && patch.x + patch.side <= 64);
    // its mean colour the rock's (the two greys', near enough)
    CHECK(patch.known && std::fabs(patch.mean[0] - 0.376f) < 0.03f && std::fabs(patch.mean[1] - 0.439f) < 0.03f);
    // the floor's stuff within the parts the raised ground shows (a sheet's corner of sand: pay_dun00's 동굴던전03) is
    // not the rock: of the squares that fit, one of a single hue - clear of the sand from column 44
    std::vector<uint32_t> cornered = pic;
    for (int y = 34; y < 48; ++y)
        for (int x = 44; x < 64; ++x) cornered[size_t(y) * 64 + size_t(x)] = 0xFFB4A05Au;
    const cave_vault::Patch clean = cave_vault::rockPatch(g, [&](size_t, ground_cells::Picture& out) {
        out = {64, 64, cornered.data()};
        return true;
    });
    CHECK(clean.side >= 20 && clean.x + clean.side <= 44 && clean.y >= 19 && clean.y + clean.side <= 48);
    CHECK(std::fabs(clean.mean[2] - 0.286f) < 0.03f);  // no sand in its colour
    // a wall of bricks is no rock to lay in pieces - courses of mortar from side to side, straight lines a mosaic
    // would break (of the 107 dungeons' patches: lhz_dun01's bricks, gon_dun01's tiles, kh_dun01's painted «A-1»,
    // ama_dun03's painted screen). No patch then: the vault's own stone in the bricks' colour
    std::vector<uint32_t> bricks = pic;
    for (int y = 22; y < 48; ++y)
        for (int x = 0; x < 64; ++x) {
            const bool mortar = y % 6 == 0 || (x + (y / 6 % 2) * 6) % 12 == 0;
            bricks[size_t(y) * 64 + size_t(x)] = mortar ? 0xFF403830u : 0xFF8A6A52u;
        }
    const cave_vault::Patch laid = cave_vault::rockPatch(g, [&](size_t, ground_cells::Picture& out) {
        out = {64, 64, bricks.data()};
        return true;
    });
    CHECK(laid.side == 0 && laid.known && laid.mean[0] > 0.35f && laid.mean[0] > laid.mean[2] + 0.1f);
    // a sheet with too little rock on it for a patch: none - its colour all the same (the rock's texels seen)
    std::vector<uint32_t> thin = pic;
    for (int y = 26; y < 48; ++y)
        for (int x = 0; x < 64; ++x) thin[size_t(y) * 64 + size_t(x)] = 0xFF000000u;
    const ground_cells::PictureOf thinOf = [&](size_t, ground_cells::Picture& out) {
        out = {64, 64, thin.data()};
        return true;
    };
    const cave_vault::Patch none = cave_vault::rockPatch(g, thinOf);
    CHECK(none.side == 0 && none.known && none.mean[1] > 0.3f);
    // the picture not found: nothing known
    const cave_vault::Patch lost = cave_vault::rockPatch(g, [](size_t, ground_cells::Picture&) { return false; });
    CHECK(lost.side == 0 && !lost.known);
    CHECK(cave_vault::rockPatch(gnd::Ground{}, pictureOf).side == 0);
}
