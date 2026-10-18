#include "core/seasons.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(seasons_by_the_month_and_the_place) {
    living::Place field;
    CHECK(seasons::of("prt_fild08", 1, true, field).snow > 0.5f);
    CHECK(seasons::of("prt_fild08", 10, true, field).dry > 0.3f);  // 2.4: the leaves on the ground are gone
    CHECK(seasons::of("prt_fild08", 4, true, field).lush > 0.5f);
    CHECK(seasons::of("prt_fild08", 7, true, field).snow == 0.0f);
    CHECK(seasons::of("prt_fild08", 1, false, field).snow == 0.0f);  // indoors
    CHECK(seasons::of("cmd_fild01", 1, true, field).snow == 0.0f);   // Comodo: no winter
    living::Place desert;
    desert.sand = true;
    CHECK(seasons::of("moc_fild01", 1, true, desert).snow == 0.0f);
    living::Place lutie;
    lutie.snow = true;
    CHECK(seasons::of("xmas", 7, true, lutie).snow > 0.5f);  // all year
}
