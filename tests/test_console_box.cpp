// 3.0.11-beta.209 (the tester's third list: «ця панелька кліента - не влазить на маленьких екранах, треба зробити
// адаптивною, можливо щоб теж ставала більш прозорою разом з інтерфейсом»): the console's size by the game's window -
// its letters smaller in a small window, all of it inside the window, its rows what is left after its foot's lines;
// its ground as see-through as the interface's panels
#include <tuple>

#include "core/console_box.hpp"
#include "test.hpp"

using namespace uagfx;

// A big window: the system's size, as before (two fifths across, three quarters down)
TEST(console_box_big_window_as_before) {
    const consolebox::Box b = consolebox::box(1920, 1080, 15);
    CHECK_EQ(b.fontH, 15);
    CHECK_EQ(b.line, 22);
    CHECK_EQ(b.w, 768);
    CHECK_EQ(b.h, 810);
    CHECK_EQ(b.x, 15);
    CHECK_EQ(b.y, 45);
}

// Small windows: smaller letters, and the whole console inside the window
TEST(console_box_small_windows_fit) {
    for (const auto& [w, h, dpi] : {std::tuple{1154, 864, 19}, std::tuple{800, 600, 15}, std::tuple{640, 480, 15},
                                    std::tuple{1280, 720, 19}}) {
        const consolebox::Box b = consolebox::box(w, h, dpi);
        CHECK(b.fontH >= 12 && b.fontH <= dpi);
        CHECK(b.x + b.w <= w);
        CHECK(b.y + b.h <= h);
        CHECK(b.h >= 8 * b.line || b.y + b.h == h - b.fontH);
    }
    CHECK(consolebox::box(640, 480, 15).fontH < 15);
}

// The rows left under the title and over the foot's lines, three at least
TEST(console_box_rows_over_the_foot) {
    const consolebox::Box b = consolebox::box(1920, 1080, 15);
    const int two = consolebox::visibleRows(b, 2 * b.line);
    const int three = consolebox::visibleRows(b, 3 * b.line);
    CHECK_EQ(two - three, 1);
    CHECK(two >= 30);
    CHECK_EQ(consolebox::visibleRows(consolebox::box(640, 480, 15), 2000), 3);
}

// The ground as see-through as the panels: 80 % as before (215 of 255), less with less, never fully clear or solid
TEST(console_box_ground_alpha) {
    CHECK(consolebox::groundAlpha(80) >= 210 && consolebox::groundAlpha(80) <= 218);
    CHECK(consolebox::groundAlpha(50) < consolebox::groundAlpha(80));
    CHECK(consolebox::groundAlpha(50) >= 110);
    CHECK(consolebox::groundAlpha(100) <= 243);
}
