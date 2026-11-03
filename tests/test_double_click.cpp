// 3.0.11-beta.202 (the tester: «треба повернути знімання дабл кліком по речі»): the game's window class has no
// CS_DBLCLKS - Windows never sends WM_LBUTTONDBLCLK; «Спорядження», «Навички», the choice cards and the rest waited for
// it. A second left press soon and near the first is one (core/double_click), the third begins again
#include "core/double_click.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(double_click_the_second_press_soon_and_near) {
    dblclick::Clicks c;
    CHECK(!dblclick::second(c, 1000, 100, 100, 500, 4, 4));  // the first
    CHECK(dblclick::second(c, 1300, 102, 99, 500, 4, 4));    // the second: soon, near
    CHECK(!dblclick::second(c, 1400, 102, 99, 500, 4, 4));   // a third begins again
    CHECK(dblclick::second(c, 1700, 101, 100, 500, 4, 4));   // and a fourth is its second
    CHECK(!dblclick::second(c, 3000, 100, 100, 500, 4, 4));
    CHECK(!dblclick::second(c, 3600, 100, 100, 500, 4, 4));  // too late: a first again
    CHECK(!dblclick::second(c, 3700, 120, 100, 500, 4, 4));  // too far: a first again
    CHECK(dblclick::second(c, 3800, 121, 101, 500, 4, 4));
}
