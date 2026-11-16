// 3.0.19-beta.32 (the 3D view's concept https://claude.ai/artifact/RadnVxG35hjE9d8HgdqzAy, decision 4: «Подземелья:
// свод из отражённого рельефа пещеры и туман, без отдельной настройки плотности»): a cave's vault from its ground's
// heights, mirrored up about the walls' top - down to the walls over them, a dome over the passages
#include <cmath>

#include "core/cave_vault.hpp"
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
