#include "core/cave_look.hpp"
#include "core/cave_vault.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.24-beta.11 (the owner 26.11: «Небо в разломе», «саму пещеру можно еще с вариантами 1 и 2»): Comodo's cave -
// a rift over its bay with the night sky and fireworks, crystals on the vault, the grotto's water light and its
// stalactites; each its own box, all on
TEST(cave_look_comodo_has_the_rift_over_its_bay) {
    const cave_look::Look l = cave_look::forMap("comodo", {true, true, true});
    CHECK(l.on());
    CHECK((l.parts & cave_look::kRift) && (l.parts & cave_look::kCrystals) && (l.parts & cave_look::kGrotto));
    // over the sea: comodo.gnd's water (under its level 15) fills rows 0-50 along the whole width
    CHECK(l.riftX > 60 && l.riftX < 120);
    CHECK(l.riftZ > 10 && l.riftZ < 45);
    CHECK(l.radiusX >= 30 && l.radiusX <= 60 && l.radiusZ >= 12 && l.radiusZ <= 30);
}

TEST(cave_look_each_part_its_own_box) {
    const cave_look::Look l = cave_look::forMap("comodo", {false, true, false});
    CHECK_EQ(l.parts, cave_look::kCrystals | cave_look::kTown);
    CHECK(!cave_look::forMap("comodo", {false, false, false}).on());
    CHECK(!cave_look::forMap("pay_dun00", {true, true, true}).on());  // another cave keeps its plain vault
    CHECK(!cave_look::forMap("cmd_fild02", {true, true, true}).on());
}

TEST(cave_look_packs_into_two_floats_exactly) {
    const cave_look::Look l = cave_look::forMap("comodo", {true, false, true});
    float place = 0.0f, size = 0.0f;
    cave_look::pack(l, place, size);
    CHECK_EQ(int(place) % 1024, l.riftX);
    CHECK_EQ(int(place) / 1024, l.riftZ);
    CHECK_EQ(int(size) % 256, l.radiusX);
    CHECK_EQ((int(size) / 256) % 256, l.radiusZ);
    CHECK_EQ(int(size) / 65536, int(cave_look::kRift | cave_look::kGrotto | cave_look::kTown));
    float p0 = 1.0f, s0 = 1.0f;
    cave_look::pack(cave_look::Look{}, p0, s0);
    CHECK(p0 == 0.0f && s0 == 0.0f);  // none: the shader's vault as before
}

// beta.10's report R-BYS7WU: the vault came down in a spike to a lamp post - Comodo's 146 «기둥» models (lamp posts,
// railings' posts, wooden posts) are no cave's pillars
TEST(cave_look_comodo_has_no_posts) {
    CHECK(!cave_vault::postsFor("comodo"));
    CHECK(cave_vault::postsFor("pay_dun00"));
}

// 3.0.24-beta.16 (the owner 27.11 «да»: Comodo's own caves as its town's vault): beach_dun, beach_dun2, beach_dun3 -
// the sculpted vault, the grotto's water light, the crystals; no rift (deep dungeons) and the rock in each one's own
// light (no town's pink and blue)
TEST(cave_look_comodo_caves_sculpted_without_the_rift) {
    for (const char* m : {"beach_dun", "beach_dun2", "beach_dun3"}) {
        const cave_look::Look l = cave_look::forMap(m, {true, true, true});
        CHECK(l.on());
        CHECK(!(l.parts & cave_look::kRift));
        CHECK(!(l.parts & cave_look::kTown));
        CHECK((l.parts & cave_look::kCrystals) && (l.parts & cave_look::kGrotto));
    }
    CHECK(cave_look::forMap("comodo", {true, true, true}).parts & cave_look::kTown);
    CHECK(cave_look::forMap("comodo", {false, false, false}).parts == 0);
    CHECK(!cave_look::forMap("beach_dun", {true, false, false}).on());  // the rift's box alone: nothing here
}
