#include <cmath>
#include <string>

#include "core/window_sizes.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.151 (the owner: the equipment window not sized by pulling): a window's own size - its
// corner pulled: the size by how far the corner went against where it was (the width's and the height's share
// together), 70-150 %; kept by the window's name in a file of lines «equip 0.85»
TEST(window_sizes_pulled_kept_and_read) {
    windowsizes::Sizes s;
    CHECK_EQ(s.of("equip"), 1.0f);
    CHECK(std::abs(windowsizes::pulled(1.0f, 400, 300, 200, 150) - 0.7f) < 0.001f);   // half: the least
    CHECK(std::abs(windowsizes::pulled(1.0f, 400, 300, 480, 360) - 1.2f) < 0.001f);
    CHECK(std::abs(windowsizes::pulled(0.8f, 400, 300, 500, 375) - 1.0f) < 0.001f);   // from its own size
    CHECK(std::abs(windowsizes::pulled(1.0f, 400, 300, 2000, 2000) - 1.5f) < 0.001f);  // the most
    s.set("equip", 0.85f);
    s.set("items", 1.2f);
    const std::string text = s.text();
    windowsizes::Sizes back = windowsizes::Sizes::parse(text);
    CHECK(std::abs(back.of("equip") - 0.85f) < 0.001f);
    CHECK(std::abs(back.of("items") - 1.2f) < 0.001f);
    CHECK_EQ(windowsizes::Sizes::parse("equip 9\nbad\n").of("equip"), 1.5f);  // clamped
}

// 3.0.24-beta.48 (the audit 28.11, the owner's «сделать»: «когда растягиваешь инвентарь то становится больше итемов а
// не просто размер окошка»): a grid window's corner adds whole cells (their size kept); its scale by Ctrl and the wheel
TEST(window_sizes_corner_adds_whole_cells) {
    using windowsizes::Cells;
    const Cells from{8, 5};
    const Cells wider = windowsizes::cellsPulled(from, 56, 120, 0);  // 2.1 cells to the right
    CHECK(wider.columns == 10 && wider.rows == 5);
    const Cells taller = windowsizes::cellsPulled(from, 56, 0, 150);  // 2.7 cells down
    CHECK(taller.columns == 8 && taller.rows == 8);
    const Cells small = windowsizes::cellsPulled(from, 56, -1000, -1000);
    CHECK(small.columns == windowsizes::kLeastColumns && small.rows == windowsizes::kLeastRows);
    const Cells big = windowsizes::cellsPulled(from, 56, 1000, 1000);
    CHECK(big.columns == windowsizes::kMostColumns && big.rows == windowsizes::kMostRows);
    CHECK(windowsizes::cellsPulled(from, 56, 20, -20).columns == 8);  // under half a cell: as it was
}

TEST(window_sizes_wheel_scales_by_five_percent) {
    CHECK(std::abs(windowsizes::wheeled(1.0f, 1) - 1.05f) < 0.001f);
    CHECK(std::abs(windowsizes::wheeled(1.0f, -2) - 0.90f) < 0.001f);
    CHECK(windowsizes::wheeled(1.48f, 3) == windowsizes::kMost);
    CHECK(windowsizes::wheeled(0.72f, -3) == windowsizes::kLeast);
}

TEST(window_sizes_keep_the_cells_beside_the_scales) {
    windowsizes::Sizes s;
    s.set("items", 0.85f);
    s.setCells("items", windowsizes::Cells{10, 6});
    s.setCells("storage", windowsizes::Cells{40, 1});  // clamped
    const windowsizes::Sizes back = windowsizes::Sizes::parse(s.text());
    CHECK(std::abs(back.of("items") - 0.85f) < 0.001f);
    const windowsizes::Cells items = back.cells("items", windowsizes::Cells{8, 5});
    CHECK(items.columns == 10 && items.rows == 6);
    const windowsizes::Cells storage = back.cells("storage", windowsizes::Cells{8, 5});
    CHECK(storage.columns == windowsizes::kMostColumns && storage.rows == windowsizes::kLeastRows);
    const windowsizes::Cells cart = back.cells("cart", windowsizes::Cells{8, 5});  // not kept: the window's own
    CHECK(cart.columns == 8 && cart.rows == 5);
    CHECK(windowsizes::Sizes::parse("items 0.900\nitems.cells 9x7\n").cells("items", {8, 5}).rows == 7);
}
