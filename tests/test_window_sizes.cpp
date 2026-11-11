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
