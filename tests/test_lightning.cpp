#include "core/lightning.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(lightning_flashes_now_and_then) {
    // 0.80: some flashes in ten minutes, most of the time none, never past 1.
    int lit = 0, flashes = 0;
    bool was = false;
    for (int k = 0; k < 600 * 50; ++k) {
        const float f = lightning::flash(k / 50.0);
        CHECK(f >= 0.0f && f <= 1.0f);
        lit += f > 0.05f;
        flashes += f >= 1.0f && !was;
        was = f >= 1.0f;
    }
    CHECK(flashes > 15 && flashes < 45);
    CHECK(lit < 600 * 50 / 10);
    CHECK(lightning::flash(-1.0) == 0.0f);
}
