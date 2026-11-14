// 3.0.19-beta.32 (the 3D view's concept https://claude.ai/artifact/RadnVxG35hjE9d8HgdqzAy, decision 4: «Подземелья:
// свод из отражённого рельефа пещеры и туман, без отдельной настройки плотности»): a cave's vault from its ground's
// heights, mirrored up about the walls' top - down to the walls over them, a dome over the passages
#include <cmath>

#include "core/cave_vault.hpp"
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
