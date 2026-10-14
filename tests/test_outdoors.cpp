#include "core/outdoors.hpp"
#include "test.hpp"

using namespace uagfx;

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
