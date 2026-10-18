#include "core/outdoors.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(outdoors_the_desert_shimmers) {
    // 0.35: Morroc and its fields shimmer far off in the heat.
    for (const char* m : {"morocc", "moc_fild01", "moc_fild20", "moc_ruins"}) CHECK(desert(m));
    for (const char* m : {"prontera", "moc_pryd01", "moc_prydb1", "prt_fild08", ""}) CHECK(!desert(m));
}

TEST(outdoors_are_the_fields_and_the_towns) {
    // 0.33: cloud shadows only under the open sky.
    for (const char* m : {"prontera", "payon", "morocc", "geffen", "alberta", "aldebaran", "comodo", "umbala", "yuno",
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
