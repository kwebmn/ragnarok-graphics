#include "core/ui_sheet_model.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.36 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5): a window's markup as plain data -
// a box as panel::roundRect takes it, a circle a round box, a line along an axis a thin box
TEST(ui_sheet_model_boxes_circles_lines) {
    sheet::Layer l;
    sheet::rect(l, panel::Box{1, 2, 30, 10}, 4, 0xFFFFFF, 0.1f, 0xC9A55B, 0.8f, 1.5f);
    sheet::rect(l, panel::Box{0, 0, 0, 10}, 0, 0xFFFFFF, 1);       // no size: none
    sheet::rect(l, panel::Box{0, 0, 10, 10}, 0, 0xFFFFFF, 0, 0, 0);  // nothing seen: none
    CHECK(l.boxes.size() == 1 && l.boxes[0].edgeW == 1.5f && l.boxes[0].r == 4);
    sheet::circle(l, 10, 10, 3, 0x5BD36C, 1);
    CHECK(l.boxes.back().box.w == 6 && l.boxes.back().r == 3);
    sheet::line(l, 0, 5, 20, 5, 2, 0xFF0000, 1);
    CHECK(l.boxes.back().box.w == 22 && l.boxes.back().box.h == 2);
    const size_t n = l.boxes.size();
    sheet::line(l, 0, 0, 10, 10, 2, 0xFF0000, 1);  // slanted: none
    CHECK(l.boxes.size() == n);
}
