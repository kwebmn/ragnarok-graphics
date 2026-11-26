#include "core/outdoors.hpp"
#include "core/cave_vault.hpp"
#include "core/sound_place.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(outdoors_the_desert_shimmers) {
    // 0.35: Morroc and its fields shimmer far off in the heat.
    for (const char* m : {"morocc", "moc_fild01", "moc_fild20", "moc_ruins"}) CHECK(desert(m));
    for (const char* m : {"prontera", "moc_pryd01", "moc_prydb1", "prt_fild08", ""}) CHECK(!desert(m));
}

TEST(outdoors_are_the_fields_and_the_towns) {
    // 0.33: cloud shadows only under the open sky.
    for (const char* m : {"prontera", "payon", "morocc", "geffen", "alberta", "aldebaran", "umbala", "yuno",
                          "lighthalzen", "rachel", "splendide", "manuk", "eclage", "prt_fild08", "moc_fild20",
                          "pay_fild01", "um_fild02", "spl_fild01", "cmd_fild03", "mid_camp"})
        CHECK(outdoors(m));
    for (const char* m : {"mag_dun01", "prt_in", "geffen_in", "alberta_in", "1@tower", "gld_dun01", "prtg_cas01",
                          "pvp_y_1-1", "que_qsch01", "job3_war01", "mid_campin", "", "prt_maze01", "gl_prison"})
        CHECK(!outdoors(m));
}

TEST(outdoors_floors_shine_where_they_are_smooth) {
    // 2.5.17: the floors reflected what stands on them on every map under a roof - a cave's cracked earth too (the
    // developer's report R-HNEYBN: anthell01, smeared copies of the loot and the knight under them). Now the smooth
    // floors alone: castles, churches, towers, labs, temples, ice; houses half; dungeons of earth and rock none.
    CHECK(floorShine("anthell01") == 0.0f && floorShine("mag_dun01") == 0.0f && floorShine("iz_dun02") == 0.0f);
    CHECK(floorShine("orcsdun01") == 0.0f && floorShine("moc_pryd04") == 0.0f && floorShine("gl_prison") == 0.0f);
    CHECK(floorShine("thor_v02") == 0.0f && floorShine("odin_tem02") == 0.0f);  // lava rock; a temple under the sky
    CHECK(floorShine("prt_church") == 1.0f && floorShine("prtg_cas01") == 1.0f && floorShine("gl_cas02") == 1.0f);
    CHECK(floorShine("c_tower2") == 1.0f && floorShine("lhz_dun03") == 1.0f && floorShine("tha_t07") == 1.0f);
    CHECK(floorShine("ice_dun02") == 1.0f && floorShine("ra_san04") == 1.0f && floorShine("kh_dun01") == 1.0f);
    CHECK(floorShine("prt_in") == 0.5f && floorShine("payon_in01") == 0.5f && floorShine("yuno_in03") == 0.5f);
    CHECK(floorShine("prontera") == 0.0f && floorShine("moc_fild15") == 0.0f);  // under the sky: the rain decides
    CHECK(floorShine("") == 0.0f);
}

// 3.0.11-beta.299 (the owner: «нубзона, мжолниры ... плоской карта быть не должна»): the green open maps the name
// rule took for a roof - Mjolnir, the novice grounds, the guild castles' fields, Byalan Island, Moscovia's forests -
// under the sky now; their castles, the school's rooms and the dungeons still not
TEST(outdoors_the_green_open_maps) {
    for (const char* m : {"mjolnir_01", "mjolnir_12", "new_1-2", "new_5-3", "new_zone03", "pay_gld", "izlu2dun",
                          "mosk_dun01", "glast_01", "alb2trea", "pvp_y_2-3", "prt_monk"})
        CHECK(uagfx::outdoors(m));
    for (const char* m : {"new_1-1", "new_1-4", "payg_cas01", "izlu_dun01", "mosk_in", "gl_cas01", "gefenia01"})
        CHECK(!uagfx::outdoors(m));
}

// 3.0.18-beta.252 (R-9UNG8S, the owner on prtg_cas01: «в замке трава плоская с отражением»): a guild castle's map is
// mostly its courtyards' grass under the sky, its halls (the Emperium's) inside the same map - our grass grows there
// (off the halls' floors: ground_heights::interiorTexture), the rest stays as under a roof: no weather, no sky's
// light, the halls' polished floors reflecting
TEST(outdoors_grass_grows_in_the_guild_castles_courtyards) {
    for (const char* m : {"prtg_cas01", "payg_cas03", "gefg_cas05", "aldeg_cas02", "schg_cas01", "arug_cas02",
                          "nguild_prt", "nguild_pay"}) {
        CHECK(castleGrounds(m));
        CHECK(grassGrows(m));
        CHECK(!outdoors(m));
        CHECK(floorShine(m) == 1.0f);
    }
    for (const char* m : {"prt_fild08", "prontera", "mjolnir_01", "pay_gld"}) {
        CHECK(grassGrows(m));
        CHECK(!castleGrounds(m));
    }
    for (const char* m : {"prt_castle", "gl_cas01", "prt_church", "pay_dun00", "prt_in", "gefenia01", ""}) {
        CHECK(!grassGrows(m));
        CHECK(!castleGrounds(m));
    }
}

// 3.0.22-beta.17 (the owner's R-EE9QTG, new_zone02 «интересный глюк нашел в помещении»: the open sky over a hall and
// a white void past its door): the novice academy's hall a room - the 3D view's vault over it; the novice grounds
// round it still under the sky (.299)
TEST(outdoors_the_novice_academy_hall_is_a_room) {
    CHECK(!uagfx::outdoors("new_zone02"));
    CHECK(uagfx::outdoors("new_zone01") && uagfx::outdoors("new_zone03") && uagfx::outdoors("new_zone04"));
}

// 3.0.24-beta.10 (a player: «Comodo Beach was inside a cave ... Seeing a sun and a horizon there is just wrong»): the
// town lies in a cave - its pink is the crystals' light on the roof (comodo.rsw: diffuse 0.6/0.2/0.3, ambient
// 0.3/0.4/0.8); its fields stand under the sky (cmd_fild01-09: white light)
TEST(outdoors_comodo_town_in_a_cave_its_fields_open) {
    CHECK(!uagfx::outdoors("comodo"));
    CHECK(uagfx::outdoors("cmd_fild01"));
    CHECK(uagfx::outdoors("cmd_fild09"));
    CHECK(uagfx::cave_vault::forMap("comodo"));
    CHECK(!uagfx::cave_vault::forMap("cmd_fild02"));
    CHECK(uagfx::sound::roomOf("comodo") == uagfx::sound::Room::Cave);
    CHECK(uagfx::sound::roomOf("cmd_fild02") == uagfx::sound::Room::Open);
}
