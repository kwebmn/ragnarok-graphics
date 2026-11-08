// 3.0.16-beta.13 (the owner's pick 15 of «20 идей»): the body's tints (.14: the afterimages gone)
#include "core/afterimage.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(afterimage_body_tints) {
    CHECK(afterimage::tintOf(true, false).strength > 0 && afterimage::tintOf(false, false).strength == 0);
    CHECK(afterimage::tintOf(true, true).r == 150);  // Steel Body over Fury
}
